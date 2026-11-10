/**
  Lenovo TB375FC (MediaTek MT6897) keys for SiliciumPkg KeypadDeviceDxe.

  The board has no usable UART console - the live bootargs carry
  mtk_printk_ctrl.disable_uart=1 and console=tty0 - so the UEFI menu can only
  be driven from the physical keys. This library exposes them as ordinary
  UEFI input:

    Volume Down                 SCAN_DOWN  (move the selection down)
    Volume Up tap (< 700 ms)    SCAN_UP    (move the selection up)
    Volume Up hold >= 700 ms    ENTER      (activate, reported on release)
    any key held >= 3 s         power off, before the menu sees anything

  The tap/hold split on Volume Up is what makes the menu usable with a single
  navigable pair: Volume Down scrolls, Volume Up confirms on a long press.
  TB365FC (same vendor, same MediaTek LK, documented in
  UniXNU docs/zh/devices/tb365fc.md) proves the gesture works on this family.

  Where the key numbers come from - live device tree, hw-tmp/decoded/live-fdt.dts:

    /soc/kp@1c00e000
      compatible              = "mediatek,kp"
      reg                     = <0x0 0x1c00e000 0x0 0x1000>
      mediatek,hw-map-num     = <0x48>            (72 matrix keys)
      mediatek,hw-init-map    = <0x72 0xf9 0x0 ...>
                                 0x72 = 114 = KEY_VOLUMEDOWN at hardware key 0
      mediatek,hw-pwrkey      = <0x08>
      mediatek,sw-pwrkey      = <0x74>            (116 = KEY_POWER)
      mediatek,hw-rstkey      = <0x11>            (17)
      mediatek,sw-rstkey      = <0x73>            (115 = KEY_VOLUMEUP)

    /soc/spmi@1cc04000/pmic@4  compatible = "mediatek,mt6363"
      the power key really lives on the MT6363 PMIC, which this firmware does
      not drive yet; only the KPD matrix (Volume Up/Down) is read here.

  MISREAD RISK: hw key 17 -> KEY_VOLUMEUP is inferred from hw-rstkey/sw-rstkey
  plus hw-init-map[0]. It has NOT been confirmed by a key press on hardware.
  The first boot records which bit moves the menu and this comment gets
  corrected from that observation.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/BitmapLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/KeypadDeviceLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

//
// MediaTek KPD registers (same layout as the mainline mtk-kpd driver)
//
#define KP_MEM1                0x04
#define KP_DEBOUNCE            0x18
#define KP_EN                  0x24

#define KP_NUM_REGS            5

//
// TB375FC keys (live device tree, see the file header)
//
#define KP_BIT_VOLUME_DOWN     0    // hw-init-map[0] = 0x72 = KEY_VOLUMEDOWN
#define KP_BIT_VOLUME_UP       17   // hw-rstkey 0x11 = 17 = KEY_VOLUMEUP

#define KP_DEBOUNCE_VALUE      0x0400
#define KP_DEBOUNCE_MASK       0x3FFF

UINTN gBitmapScanCodes[BITMAP_NUM_WORDS(0x18)]    = {0};
UINTN gBitmapUnicodeChars[BITMAP_NUM_WORDS(0x7f)] = {0};

STATIC EFI_KEY_DATA mVolumeUpKeyData = { .Key = { .ScanCode = SCAN_UP } };

STATIC EFI_MEMORY_REGION_DESCRIPTOR mKeypadRegion;
STATIC BOOLEAN                      mKeypadAvailable;

STATIC BOOLEAN mVolumeUpDown       = FALSE;
STATIC BOOLEAN mVolumeUpShutdown   = FALSE;
STATIC UINT64  mVolumeUpTime       = 0;
STATIC BOOLEAN mVolumeDownHeld     = FALSE;
STATIC BOOLEAN mVolumeDownShutdown = FALSE;
STATIC UINT64  mVolumeDownTime     = 0;

STATIC EFI_KEY_DATA mVolumeDownKeyData = { .Key = { .ScanCode = SCAN_DOWN } };

#define MS2NS(ms) (((UINT64)(ms)) * 1000000ULL)

STATIC
inline
VOID
KeySetState (
  UINT16  ScanCode,
  CHAR16  UnicodeChar,
  BOOLEAN Value)
{
  if (ScanCode && ScanCode < 0x18) {
    if (Value) {
      BitmapSet (gBitmapScanCodes, ScanCode);
    } else {
      BitmapClear (gBitmapScanCodes, ScanCode);
    }
  }

  if (UnicodeChar && UnicodeChar < 0x7f) {
    if (Value) {
      BitmapSet (gBitmapUnicodeChars, UnicodeChar);
    } else {
      BitmapClear (gBitmapUnicodeChars, UnicodeChar);
    }
  }
}

STATIC
VOID
KeypadReadState (
  OUT UINT16 *State)
{
  for (UINTN Index = 0; Index < KP_NUM_REGS; Index++) {
    State[Index] = MmioRead16 (mKeypadRegion.Address + KP_MEM1 + Index * 4);
  }
}

STATIC
BOOLEAN
KeypadIsPressed (
  IN UINT16 *State,
  IN UINT32  Bit)
{
  // A released key reads 1, a pressed key reads 0
  return ((State[Bit / 16] & (1U << (Bit % 16))) == 0);
}

STATIC
VOID
LibKeyUpdateVolumeUp (
  IN KEYPAD_RETURN_API *KeypadReturnApi,
  IN BOOLEAN            IsPressed,
  IN UINT64             Delta
  )
{
  KeySetState (SCAN_UP, 0, IsPressed);

  if (IsPressed) {
    if (!mVolumeUpDown) {
      mVolumeUpDown     = TRUE;
      mVolumeUpShutdown = FALSE;
      mVolumeUpTime     = 0;
    }

    // Already powered off once during this hold, wait for the release
    if (mVolumeUpShutdown) {
      return;
    }

    mVolumeUpTime += Delta;

    //
    // Reserved shutdown: a very long hold powers the machine off directly,
    // before the menu sees any key.
    //
    if (mVolumeUpTime >= MS2NS (3000)) {
      mVolumeUpShutdown = TRUE;
      gRT->ResetSystem (EfiResetShutdown, EFI_SUCCESS, 0, NULL);
      return;
    }
  } else if (mVolumeUpDown) {
    if (!mVolumeUpShutdown) {
      if (mVolumeUpTime >= MS2NS (700)) {
        // A hold acts as ENTER, reported on release so the 3 s gesture stays reachable
        EFI_KEY_DATA EnterData = { .Key = { .UnicodeChar = CHAR_CARRIAGE_RETURN } };
        KeypadReturnApi->PushEfikeyBufTail (KeypadReturnApi, &EnterData);
      } else {
        // A tap moves the selection
        KeypadReturnApi->PushEfikeyBufTail (KeypadReturnApi, &mVolumeUpKeyData);
      }
    }

    mVolumeUpDown     = FALSE;
    mVolumeUpShutdown = FALSE;
    mVolumeUpTime     = 0;
  }
}

STATIC
VOID
LibKeyUpdateVolumeDown (
  IN KEYPAD_RETURN_API *KeypadReturnApi,
  IN BOOLEAN            IsPressed,
  IN UINT64             Delta
  )
{
  KeySetState (SCAN_DOWN, 0, IsPressed);

  if (IsPressed) {
    if (!mVolumeDownHeld) {
      mVolumeDownHeld     = TRUE;
      mVolumeDownShutdown = FALSE;
      mVolumeDownTime     = 0;
    }

    if (!mVolumeDownShutdown) {
      mVolumeDownTime += Delta;
      if (mVolumeDownTime >= MS2NS (3000)) {
        mVolumeDownShutdown = TRUE;
        gRT->ResetSystem (EfiResetShutdown, EFI_SUCCESS, 0, NULL);
        return;
      }
    }
  } else {
    mVolumeDownHeld     = FALSE;
    mVolumeDownShutdown = FALSE;
    mVolumeDownTime     = 0;
  }

  //
  // Report on release; the menu sees one move per tap instead of a repeat
  // storm while the key is held down.
  //
  if (!IsPressed && !mVolumeDownShutdown && (mVolumeDownTime > 0)) {
    if (mVolumeDownTime < MS2NS (700)) {
      KeypadReturnApi->PushEfikeyBufTail (KeypadReturnApi, &mVolumeDownKeyData);
    } else {
      EFI_KEY_DATA EnterData = { .Key = { .UnicodeChar = CHAR_CARRIAGE_RETURN } };
      KeypadReturnApi->PushEfikeyBufTail (KeypadReturnApi, &EnterData);
    }

    mVolumeDownTime = 0;
  }
}

RETURN_STATUS
EFIAPI
KeypadDeviceConstructor ()
{
  EFI_STATUS Status;

  // Locate the Keypad Registers
  Status = LocateMemoryRegionByName ("Keypad", &mKeypadRegion);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "KPD: No Keypad Memory Region! Status = %r\n", Status));
    return RETURN_SUCCESS;
  }

  mKeypadAvailable = TRUE;

  // Same set-up as the stock keypad driver: KP_EN = 1, 0x400 ms debounce
  MmioWrite16 (mKeypadRegion.Address + KP_DEBOUNCE, KP_DEBOUNCE_VALUE & KP_DEBOUNCE_MASK);
  MmioWrite16 (mKeypadRegion.Address + KP_EN, 1);

  DEBUG ((EFI_D_ERROR, "KPD: base 0x%lx\n", (UINT64)mKeypadRegion.Address));

  return RETURN_SUCCESS;
}

EFI_STATUS
EFIAPI
KeypadDeviceReset (KEYPAD_DEVICE_PROTOCOL *This)
{
  mVolumeUpDown       = FALSE;
  mVolumeUpShutdown   = FALSE;
  mVolumeUpTime       = 0;
  mVolumeDownHeld     = FALSE;
  mVolumeDownShutdown = FALSE;
  mVolumeDownTime     = 0;

  return EFI_SUCCESS;
}

EFI_STATUS
KeypadDeviceGetKeys (
  KEYPAD_DEVICE_PROTOCOL *This,
  KEYPAD_RETURN_API      *KeypadReturnApi,
  UINT64                  Delta)
{
  UINT16 State[KP_NUM_REGS];

  if (!mKeypadAvailable) {
    return EFI_SUCCESS;
  }

  KeypadReadState (State);

  LibKeyUpdateVolumeUp (KeypadReturnApi, KeypadIsPressed (State, KP_BIT_VOLUME_UP), Delta);
  LibKeyUpdateVolumeDown (KeypadReturnApi, KeypadIsPressed (State, KP_BIT_VOLUME_DOWN), Delta);

  return EFI_SUCCESS;
}

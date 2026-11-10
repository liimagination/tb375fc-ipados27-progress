##
#  Lenovo TB375FC (MediaTek MT6897 / Dimensity 8300) UEFI platform.
#
#  Built by UniXNU:  ./unixnu build firmware --uefi <this checkout>
#  which runs EDK II directly (scripts/unixnu/firmware.py):
#
#    build -p tb375fcPkg/tb375fc.dsc -a AARCH64 -b DEBUG -t CLANGPDB \
#          -D FD_BASE=0x40000000 -D FD_SIZE=0x400000 -D FD_BLOCKS=0x400
#
#  FD_BASE / FD_SIZE / FD_BLOCKS therefore arrive from the command line, and
#  [uefi_fd] in Resources/Configs/tb375fc.toml must agree with them: the
#  builder rejects an FD whose length differs from [uefi_fd] size.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

################################################################################
#
# Defines Section
#
################################################################################
[Defines]
  PLATFORM_NAME                  = tb375fc
  PLATFORM_GUID                  = 6F2B9D14-3A7C-4E58-B0D3-9C5A1E8F7B26
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/tb375fcPkg
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = tb375fcPkg/tb375fc.fdf

  #
  # The board has no panel driver in this firmware: LK already scans out and
  # SimpleFbDxe republishes that surface, so the custom display path stays off.
  #
  USE_CUSTOM_DISPLAY_DRIVER      = 0

!ifndef ENABLE_SECUREBOOT
  DEFINE ENABLE_SECUREBOOT       = 0
!endif

  #
  # Advanced Logger is ON: it is the only log channel this board has.
  # The live bootargs carry mtk_printk_ctrl.disable_uart=1 and console=tty0,
  # and the tablet exposes no UART header, so UART cannot be used for bring-up
  # output. With the logger the DXE log ring is rendered by UniXNU's Tools menu
  # as QR codes on the panel; without it a black first boot is undiagnosable.
  #
  # Cost: AdvLoggerPkg and MsGraphicsPkg live in Common/Mu, so PACKAGES_PATH
  # must carry {uefi}/Common/Mu. Set ENABLE_ADVANCED_LOG=0 on the EDK II
  # command line to build without that submodule.
  #
!ifndef ENABLE_ADVANCED_LOG
  DEFINE ENABLE_ADVANCED_LOG     = 1
!endif

  #
  # Publish Resources/DTBs/tb375fc.dtb as the EFI device-tree configuration
  # table. Off until a TB375FC firmware device tree exists; the payload's
  # initrd window reaches the firmware through the SEC entry HOB instead.
  #
!ifndef ENABLE_FIRMWARE_DTB
  DEFINE ENABLE_FIRMWARE_DTB     = 1
!endif

  #
  # Layer order, base to SoC. The platform owns the composition, so the SoC
  # package does not pull the MediaTek base layer in by itself:
  #
  #   1. SiliciumPkg.dsc.inc          the Mu-Silicium framework layer: it owns
  #                                   the [Components] entries for Sec.inf and
  #                                   DxeMain.inf and the shared build options.
  #   2. tb375fcPkg/MediaTekPkg.dsc.inc
  #                                   the MediaTek base layer, kept as a
  #                                   platform-local copy: upstream
  #                                   MediaTekPkg/MediaTekPkg.dsc.inc adds five
  #                                   MT6789 DXE drivers (GpioDxe, ClockDxe,
  #                                   PmicWrapperDxe, MT6358PmicDxe, MsdcDxe)
  #                                   whose register tables were never validated
  #                                   on MT6897. The copy keeps every PCD and the
  #                                   library mapping and drops those drivers.
  #   3. MT6897Pkg.dsc.inc            this SoC's CPU/GIC/SMBIOS and PlatformSecLib.
  #
  # Each fragment contributes its own sections, so the order above is what
  # decides which value wins when two of them set the same PCD.
  #
!include SiliciumPkg/SiliciumPkg.dsc.inc
!include tb375fcPkg/MediaTekPkg.dsc.inc
!include MT6897Pkg/MT6897Pkg.dsc.inc

#
# NOTE: the BuildOptions workaround for edk2-202511 ArmPkg/Drivers/CpuDxe/CpuDxe.c:305
#      ("variable 'TestBit' set but not used" under CLANGPDB -Werror) is NOT here.
#
#      A per-INF [BuildOptions] line such as
#        ArmPkg/Drivers/CpuDxe/CpuDxe.inf: *_CLANGPDB_AARCH64_CC_FLAGS = -Wno-unused-but-set-variable
#      looks right but does not reach the compiler: when BaseTools assembles
#      CLANGPDB_ALL_CC_FLAGS it drops the override that CLANGPDB_WARNING_OVERRIDES
#      contributes, so the per-module line never gets the base flag list it is
#      supposed to be appended to. Verified on this build by inspecting the
#      generated compile command line: the flag is absent.
#
#      The build therefore injects the suppression into the CONF copy of
#      tools_def.txt instead (see tb375fc-port/tools/build-fd.sh), which proves
#      the flag actually reaches clang and keeps -Werror live for every other
#      diagnostic. Do not re-add the DSC line expecting it to work.
#

[PcdsFixedAtBuild]
  #
  # DDR: 8 GiB at 0x40000000. This value only separates the memory map from
  # the register regions in MemoryInitPeiLib (see MemoryMapLib.c).
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x40000000

  #
  # UEFI stack: directly above the FD, which itself starts at the kernel_addr
  # LK reads from the stock vendor_boot header (0x40000000) - the address the
  # gzip kernel field (our FD) is decompressed to.
  #
  # 0x40800000 == 0x40000000 + [uefi_fd].size (0x00800000). MemoryMapLib.c
  # asserts exactly this relation, so the FD size, this PCD and
  # Resources/Configs/tb375fc.toml must always change together.
  #
  gArmPlatformTokenSpaceGuid.PcdCPUCoresStackBase|0x42000000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x40000

  #
  # Generic timer interrupts. The FDT /timer node declares the standard four
  # (secure 13, non-secure 14, virtual 11, hypervisor 10) with
  # clock-frequency 0xc65d40 (13 MHz) - the same PPI numbering every ARMv8
  # platform uses, so no board value is needed here.
  #
  gArmTokenSpaceGuid.PcdArmArchTimerSecIntrNum|29
  gArmTokenSpaceGuid.PcdArmArchTimerIntrNum|30
  gArmTokenSpaceGuid.PcdArmArchTimerVirtIntrNum|27
  gArmTokenSpaceGuid.PcdArmArchTimerHypIntrNum|26

  #
  # SMBIOS
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemManufacturer|"Lenovo"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemModel|"Lenovo TB375FC"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailModel|"tb375fc"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailSku|"TB375FC"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemBoardModel|"TB375FC"

  #
  # Simple Frame Buffer: LK already drives the panel and leaves its scanout
  # in RAM, so this firmware republishes that surface and never touches DSI.
  #
  #   LK window    = 0xfc16f000, 0x3e90000 bytes   (memory map: "Display Reserved",
  #                   but declared from frame 1, see MemoryMapLib.c)
  #   scanout      = 0xfd619000 = 0xfc16f000 + 0x14AA000  (MEASURED, beacon6/7)
  #   panel        = Tianma NT36532, DSI video mode, 144 Hz
  #   lcmname      = "nt36532_dsi_vdo_144hz_tianma_lcm_drv"
  #
  # Framebuffer geometry, measured on hardware (2026-11-06).
  #
  # Android reports the *logical* panel size as 1840x2944 portrait, but the
  # panel is natively LANDSCAPE: SurfaceFlinger reports
  # primary_display_orientation = ORIENTATION_270, i.e. the panel scans out
  # 2944 pixels per line and 1840 lines.
  #
  # Evidence: with 1840x2944 the FrameBufferSerialPortLib console came out
  # rotated and sheared on the first hardware boot (each drawn row covered only
  # 1840*4 = 7360 of the real 2944*4 = 11776 bytes per scanline). The two
  # orderings describe exactly the same buffer size, so this cannot be told
  # from atag,videolfb-vramSize (= 0x3E90000, 3 buffers plus padding) alone.
  #
  # SimpleFbDxe derives the pitch as Width * ColorDepth / 8, so these two values
  # are the only place that has to change.
  #
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth|2944
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferHeight|1840
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferColorDepth|32

  #
  # Storage: the board boots from UFS (FDT /soc/ufshci@112b0000,
  # "mediatek,mt8183-ufshci"), not eMMC.
  #
  # gMediaTekPkgTokenSpaceGuid.PcdStorageIsEMMC would describe that, but the
  # token space is declared in MediaTekPkg.dec, and no INF reached through the
  # platform FDF references that package any more (the five MediaTek DXE
  # drivers that did were dropped in Platforms/Lenovo/tb375fcPkg/
  # MediaTekPkg.dsc.inc). EDK II rejects a DSC PCD whose DEC is not pulled in
  # by an FDF-referenced INF (error F001), so the setting is left out until a
  # MediaTek package is back on the FDF path.

  #
  # Device Tree: publish the built-in TB375FC tree as the EFI configuration
  # table instead of ACPI.
  #
  gEmbeddedTokenSpaceGuid.PcdDefaultDtPref|TRUE

  #
  # UART0 at 0x11001000. Not the boot console: the stock bootargs carry
  # mtk_printk_ctrl.disable_uart=1, so DEBUG output goes to the framebuffer
  # through FrameBufferSerialPortLib and the panel is the log channel.
  #
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultBaudRate|115200
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultDataBits|8
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultParity|0
  gEfiMdePkgTokenSpaceGuid.PcdUartDefaultStopBits|1

  #
  # Bring-up debug: the stock mask hides every BDS decision ("Booting ...",
  # "Image Return Status = ..."), which are exactly the lines needed while the
  # menu is being brought up. Restore 0x8007EE0F once boot is stable.
  #
  # INFO and above (0x80000046 = ERROR|WARN|LOAD|INFO + reserved bit).
  #
  # The INFO level is required during bring-up: UniXNU reports the display
  # decision as "UNIXNU: ui: graphics 2944x1840, scale 3, landscape layout",
  # "ui: text console WxH" or "ui: headless (no GOP, no text console)". Without
  # INFO that line never reaches the framebuffer console, which is the only log
  # channel this board has (the stock cmdline sets
  # mtk_printk_ctrl.disable_uart=1 and the tablet exposes no serial header).
  #
  # NOTE: the console clears the whole framebuffer once it runs past the last
  # text row (FrameBufferSerialPortLib::AdvanceNewLine -> ZeroMem), so the log
  # that matters is near the END of the output, not the start.
  #
  # Restore 0x8007EE0F once the port is stable.
  #
  # Bring-up: SILENT. The panel belongs to the UniXNU menu.
  #
  # On this board DEBUG output is rendered by FrameBufferSerialPortLib straight
  # into the same framebuffer the menu draws into. Two problems follow and both
  # were measured on the unit:
  #
  #   1. the console repaint lands on top of the menu, and when it scrolls past
  #      its last row it calls ZeroMem over the whole visible surface, erasing
  #      the menu entirely;
  #   2. it keeps its cursor state in the last bytes of whatever region is named
  #      "Display Reserved", so the console's idea of where the screen is can
  #      drift from the menu's.
  #
  # With the level at 0 nothing is written to the panel at all: the menu is the
  # only thing on screen, which is what a boot menu has to be. Restore
  # 0x8007EE0F once the port is stable and a serial channel exists.
  #
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x00000000

  #
  # UniXNU: the model name is part of the file name the Tools menu exports.
  #
  gUniXNUPkgTokenSpaceGuid.PcdUniXNUDeviceModel|"TB375FC"

!if $(ENABLE_ADVANCED_LOG) == 1
  #
  # Advanced Logger: an in-RAM ring that the UniXNU Tools menu renders as QR
  # codes, so the panel doubles as the log channel.
  #
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerPages|256
!endif

!if $(ENABLE_ADVANCED_LOG) == 1
[PcdsFeatureFlag]
  #
  # Keep the newest messages once the ring is full: the boot story that
  # matters is at the end.
  #
  gAdvLoggerPkgTokenSpaceGuid.PcdAdvancedLoggerAutoWrapEnable|TRUE
!endif

[LibraryClasses]
  #
  # Memory Libraries
  #
  MemoryMapLib|tb375fcPkg/Library/MemoryMapLib/MemoryMapLib.inf

  #
  # Keypad Device (the only input this firmware has for the menu)
  #
  KeypadDeviceLib|tb375fcPkg/Library/KeypadDeviceLib/KeypadDeviceLib.inf

  #
  # BDS: start the UniXNU application directly.
  #
  # SiliciumPkg.dsc.inc maps DeviceBootManagerLib to the Null implementation,
  # and BootManagerLib uses the Microsoft Surface boot policy, which registers
  # MS_SDD_BOOT / MS_USB_BOOT / MS_PXE_BOOT / UEFI shell as default options.
  # None of those exist in this firmware volume, so BootOrder stays empty and
  # BDS ends with "failed to find an operating system" (measured on the unit,
  # 2026-11-07). This override starts UniXNU from the FV at BDS entry.
  #
  DeviceBootManagerLib|tb375fcPkg/Library/DeviceBootManagerLib/DeviceBootManagerLib.inf

  #
  # UniXNU Libraries
  #
  UniXNULib|UniXNUPkg/Library/UniXNULib/UniXNULib.inf

  #
  # Required by the UniXNU integration contract
  # (docs/zh/porting/uefi-firmware.md "必需的集成"). SiliciumPkg.dsc.inc
  # already maps these; they are restated so the board file states what it
  # depends on instead of inheriting it silently.
  #
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  FdtLib|MdePkg/Library/BaseFdtLib/BaseFdtLib.inf

!if $(ENABLE_ADVANCED_LOG) == 1
  AdvancedLoggerHdwPortLib|AdvLoggerPkg/Library/AdvancedLoggerHdwPortLib/AdvancedLoggerHdwPortLib.inf
  AdvancedLoggerAccessLib|AdvLoggerPkg/Library/AdvancedLoggerAccessLib/AdvancedLoggerAccessLib.inf
  QrEncoderLib|MsGraphicsPkg/Library/QrEncoderLib/QrEncoderLib.inf

[LibraryClasses.common.DXE_CORE, LibraryClasses.common.DXE_DRIVER, LibraryClasses.common.DXE_RUNTIME_DRIVER, LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.UEFI_APPLICATION]
  DebugLib|AdvLoggerPkg/Library/BaseDebugLibAdvancedLogger/BaseDebugLibAdvancedLogger.inf
  AssertLib|AdvLoggerPkg/Library/AssertLib/AssertLib.inf

[LibraryClasses.common.DXE_DRIVER, LibraryClasses.common.UEFI_DRIVER, LibraryClasses.common.UEFI_APPLICATION]
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/Dxe/AdvancedLoggerLib.inf

[LibraryClasses.common.DXE_RUNTIME_DRIVER]
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/Runtime/AdvancedLoggerLib.inf

[LibraryClasses.common.DXE_CORE]
  AdvancedLoggerLib|AdvLoggerPkg/Library/AdvancedLoggerLib/DxeCore/AdvancedLoggerLib.inf
!endif

[Components]
  #
  # Serial I/O
  #
  MdeModulePkg/Universal/SerialDxe/SerialDxe.inf

  #
  # Keypad (KPD -> Simple Text Input)
  #
  SiliciumPkg/Drivers/KeypadDeviceDxe/KeypadDeviceDxe.inf
  SiliciumPkg/Drivers/KeypadDxe/KeypadDxe.inf

  #
  # UniXNU: the menu application, the registrar that gives it a Boot####
  # entry, and the initrd-backed block device the payload volume is read from.
  #
  UniXNUPkg/Application/UniXNU/UniXNU.inf
  UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf
  UniXNUPkg/Driver/UniXNURamDiskDxe/UniXNURamDiskDxe.inf {
    <LibraryClasses>
      NULL|MdeModulePkg/Library/LzmaCustomDecompressLib/LzmaCustomDecompressLib.inf
  }

  #
  # XNU loader, part of the firmware volume (UniXNU "loader = firmware").
  #
  DarwinBootPkg/Driver/DarwinBoot.inf

!if $(ENABLE_ADVANCED_LOG) == 1
  #
  # UniXNU Tools menu content: exports the Advanced Logger ring as QR pages.
  #
  UniXNUPkg/Driver/AdvancedLogDxe/AdvancedLogDxe.inf
!endif

[PcdsDynamicExDefault]
  #
  # Diagnostics: skip the boot logo background fill so the SEC stage beacons
  # stay visible under the logo.
  #
  gMsGraphicsPkgTokenSpaceGuid.PcdPostBackgroundColoringSkipCount|1
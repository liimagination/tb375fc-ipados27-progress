/** @file
  TB375FC DeviceBootManagerLib: launch the UniXNU application directly.

  Why this exists
  ---------------
  The stock Silicium BDS path is Microsoft Surface shaped:

    PlatformBootManagerBeforeConsole()
      -> MsBootOptionsLibRegisterDefaultBootOptions()

  and that lib registers MS_SDD_BOOT, MS_USB_BOOT, MS_PXE_BOOT and the UEFI
  shell. None of those files exist in this firmware volume, so every one of
  them fails to be created, BootOrder stays empty, and BDS finishes with

      failed to find an operating system

  Measured on the unit, 2026-11-07.

  This library takes the direct route instead: at BDS entry it locates the
  UniXNU application file inside the firmware volume and starts it with
  gBS->LoadImage + gBS->StartImage. No boot option, no BootOrder, no variable
  store involvement.

  Bring-up instrumentation
  ------------------------
  The framebuffer console scrolls too fast to read on this board, so every
  step paints a 150x40 block along the bottom edge of the panel, into frame 1
  (0xFD619000), the buffer the panel actually scans (measured with beacon6).
  The number of blocks visible says how far the code got before it stopped:

    1 white    DeviceBootManagerBdsEntry entered
    2 red      application GUID resolved
    3 green    LocateHandleBuffer(FirmwareVolume2) succeeded
    4 blue     the UniXNU file was found inside a firmware volume
    5 yellow   LoadImage succeeded
    6 magenta  StartImage returned (the application exited)

  If the runner stops after N blocks, step N+1 is the one that failed. With
  the previous build the panel showed a data abort at FAR=0, ELR=0x4750DCC4,
  an ASSERT from ArmCpuDxe's DefaultExceptionHandler - i.e. a NULL read
  somewhere in this path, which these blocks narrow down.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceBootManagerLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/FirmwareVolume2.h>
#include <Protocol/LoadedImage.h>

STATIC CONST EFI_GUID  mUniXnuApplicationGuid = {
  0x9B665204, 0xA133, 0x47A2, { 0x93, 0x96, 0x9E, 0x2B, 0x37, 0x0A, 0x4E, 0x25 }
};

/**
  Paint one progress block along the bottom edge of the panel.

  @param[in] Step    Block index, 0-based; each block is 200 pixels wide.
  @param[in] Colour  ARGB8888 pixel value; the alpha byte must be 0xFF.
**/
/**
  Read a file out of any firmware volume.

  @param[in]  FileGuid  Name of the file to find.
  @param[out] Buffer    Buffer holding the image.  Caller frees.
  @param[out] Size      Size of the image, in bytes.
**/
STATIC
EFI_STATUS
ReadApplicationFromFv (
  IN  CONST EFI_GUID  *FileGuid,
  OUT VOID            **Buffer,
  OUT UINTN           *Size
  )
{
  EFI_STATUS                     Status;
  EFI_HANDLE                     *Handles;
  UINTN                          Count;
  UINTN                          Index;
  EFI_FIRMWARE_VOLUME2_PROTOCOL  *Fv;
  EFI_FV_FILETYPE                Type;
  EFI_FV_FILE_ATTRIBUTES         Attributes;
  UINT32                         AuthenticationStatus;

  *Buffer = NULL;
  *Size   = 0;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiFirmwareVolume2ProtocolGuid,
                  NULL,
                  &Count,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = EFI_NOT_FOUND;
  for (Index = 0; Index < Count; Index++) {
    if (EFI_ERROR (gBS->HandleProtocol (Handles[Index], &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Fv))) {
      continue;
    }

    Type         = 0;
    Attributes   = 0;
    AuthenticationStatus = 0;
    if (EFI_ERROR (
          Fv->ReadFile (
                Fv,
                FileGuid,
                Buffer,
                Size,
                &Type,
                &Attributes,
                &AuthenticationStatus
                )
          ))
    {
      continue;
    }

    Status = EFI_SUCCESS;
    break;
  }

  FreePool (Handles);
  return Status;
}

/**
  Start the UniXNU application, painting a progress block at every step.
**/
STATIC
EFI_STATUS
StartUniXnuApplication (
  VOID
  )
{
  EFI_STATUS                         Status;
  VOID                               *Buffer;
  UINTN                              Size;
  EFI_HANDLE                         ImageHandle;
  EFI_HANDLE                         *FvHandles;
  UINTN                              FvCount;
  UINTN                              Index;
  CONST EFI_GUID                     *FileGuid;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  Node;
  EFI_DEVICE_PATH_PROTOCOL           *DevicePath;
  EFI_FIRMWARE_VOLUME2_PROTOCOL      *Fv;
  EFI_FV_FILETYPE                    Type;
  EFI_FV_FILE_ATTRIBUTES             Attributes;
  UINT32                             AuthenticationStatus;


  //
  // Resolve the application GUID. The PCD is the integration contract, but
  // fall back to the compiled-in value so a missing PCD cannot turn into a
  // NULL dereference inside ReadFile's GUID comparison.
  //
  FileGuid = (CONST EFI_GUID *)PcdGetPtr (PcdUniXNUApplicationFile);
  if (FileGuid == NULL) {
    FileGuid = &mUniXnuApplicationGuid;
  }


  //
  // Find the firmware volume that carries the application.
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiFirmwareVolume2ProtocolGuid,
                  NULL,
                  &FvCount,
                  &FvHandles
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TB375FC: no FV handles: %r\n", Status));
    return Status;
  }


  for (Index = 0; Index < FvCount; Index++) {
    if (EFI_ERROR (gBS->HandleProtocol (FvHandles[Index], &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Fv))) {
      continue;
    }

    Type                 = 0;
    Attributes           = 0;
    AuthenticationStatus = 0;
    Buffer               = NULL;
    Size                 = 0;
    if (EFI_ERROR (
          Fv->ReadFile (
                Fv,
                FileGuid,
                &Buffer,
                &Size,
                &Type,
                &Attributes,
                &AuthenticationStatus
                )
          ))
    {
      continue;
    }


    DevicePath = NULL;
    if (DevicePathFromHandle (FvHandles[Index]) != NULL) {
      EfiInitializeFwVolDevicepathNode (&Node, FileGuid);
      DevicePath = AppendDevicePathNode (DevicePathFromHandle (FvHandles[Index]), (EFI_DEVICE_PATH_PROTOCOL *)&Node);
    }

    FreePool (Buffer);

    if (DevicePath != NULL) {
      Status = gBS->LoadImage (FALSE, gImageHandle, DevicePath, NULL, 0, &ImageHandle);
      FreePool (DevicePath);
    } else {
      //
      // No device path: load the raw image instead.
      //
      Status = ReadApplicationFromFv (FileGuid, &Buffer, &Size);
      if (!EFI_ERROR (Status)) {
        Status = gBS->LoadImage (FALSE, gImageHandle, NULL, Buffer, Size, &ImageHandle);
        FreePool (Buffer);
      }
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "TB375FC: LoadImage(UniXNU) failed: %r\n", Status));
      FreePool (FvHandles);
      return Status;
    }


    FreePool (FvHandles);
    Status = gBS->StartImage (ImageHandle, NULL, NULL);

    DEBUG ((DEBUG_ERROR, "TB375FC: UniXNU returned %r\n", Status));
    return Status;
  }

  FreePool (FvHandles);
  DEBUG ((DEBUG_ERROR, "TB375FC: UniXNU application %g not found in any FV\n", FileGuid));
  return EFI_NOT_FOUND;
}

EFI_HANDLE
EFIAPI
DeviceBootManagerBeforeConsole (
  OUT EFI_DEVICE_PATH_PROTOCOL   **DevicePath,
  OUT BDS_CONSOLE_CONNECT_ENTRY  **PlatformConsoles
  )
{
  return NULL;
}

EFI_DEVICE_PATH_PROTOCOL **
EFIAPI
DeviceBootManagerAfterConsole (
  VOID
  )
{
  return NULL;
}

EFI_DEVICE_PATH_PROTOCOL **
EFIAPI
DeviceBootManagerOnDemandConInConnect (
  VOID
  )
{
  return NULL;
}

VOID
EFIAPI
DeviceBootManagerProcessBootCompletion (
  OUT EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption
  )
{
  return;
}

EFI_STATUS
EFIAPI
DeviceBootManagerPriorityBoot (
  IN OUT EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption
  )
{
  return EFI_SUCCESS;
}

VOID
EFIAPI
DeviceBootManagerUnableToBoot (
  VOID
  )
{
  return;
}

/**
  BDS entry hook: runs before BDS consults BootOrder.
**/
VOID
EFIAPI
DeviceBootManagerBdsEntry (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = StartUniXnuApplication ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TB375FC: UniXNU was not started (%r), falling back to BDS\n", Status));
  }
}
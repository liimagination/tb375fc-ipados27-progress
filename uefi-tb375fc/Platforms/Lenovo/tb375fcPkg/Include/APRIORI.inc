APRIORI DXE {
  INF MdeModulePkg/Core/Dxe/DxeMain.inf
  INF MdeModulePkg/Universal/PCD/Dxe/Pcd.inf

  INF MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
  INF MdeModulePkg/Universal/StatusCodeHandler/RuntimeDxe/StatusCodeHandlerRuntimeDxe.inf
  INF MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  INF ArmPkg/Drivers/CpuDxe/CpuDxe.inf
  INF ArmPkg/Drivers/ArmGicDxe/ArmGicDxe.inf
  INF EmbeddedPkg/MetronomeDxe/MetronomeDxe.inf
  INF ArmPkg/Drivers/TimerDxe/TimerDxe.inf

  #
  # The initrd block device must exist before PartitionDxe and Fat start:
  # UniXNURamDiskDxe publishes EFI_BLOCK_IO_PROTOCOL for the payload volume
  # that the bootloader handed over in /chosen/linux,initrd-start.
  #
  INF UniXNUPkg/Driver/UniXNURamDiskDxe/UniXNURamDiskDxe.inf

  INF MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  INF MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  INF FatPkg/EnhancedFatDxe/Fat.inf
  INF MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf
  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf
  INF MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf
  INF MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf
  INF EmbeddedPkg/EmbeddedMonotonicCounter/EmbeddedMonotonicCounter.inf
  INF EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf
  INF MdeModulePkg/Universal/PrintDxe/PrintDxe.inf
  INF MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  INF MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf
  INF MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf

  #
  # UniXNUDxe MUST run before BdsDxe.
  #
  # It is the registrar that creates the Boot#### option pointing at the
  # UniXNU application and puts it first in BootOrder. BdsDxe is in this
  # APRIORI list, so listing UniXNUDxe in DXE.inc alone is not enough: DXE
  # dispatched BdsDxe first, BdsDxe found an empty BootOrder, printed
  # "failed to find an operating system", and only then did UniXNUDxe run and
  # write the variable - too late for anything to consume it. Measured on the
  # unit on 2026-11-07.
  #
  INF UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf

  INF MdeModulePkg/Universal/BdsDxe/BdsDxe.inf

  #
  # GOP: LK leaves the panel scanning out, SimpleFbDxe republishes that
  # surface from the "Display Reserved" memory region.
  #
!if $(USE_CUSTOM_DISPLAY_DRIVER) == 0
  INF SiliciumPkg/Drivers/SimpleFbDxe/SimpleFbDxe.inf
!endif

  INF MdeModulePkg/Universal/SerialDxe/SerialDxe.inf
  INF EmbeddedPkg/SimpleTextInOutSerial/SimpleTextInOutSerial.inf
  INF MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
  INF MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  INF MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf

  #
  # Simple Text Input: the KPD matrix is the only input device in this build.
  #
  INF SiliciumPkg/Drivers/KeypadDeviceDxe/KeypadDeviceDxe.inf
  INF SiliciumPkg/Drivers/KeypadDxe/KeypadDxe.inf
}

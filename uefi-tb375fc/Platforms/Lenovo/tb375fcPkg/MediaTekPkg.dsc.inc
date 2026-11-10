##
#  Platform-local override of MediaTekPkg/MediaTekPkg.dsc.inc for TB375FC (MT6897).
#
#  Why this file exists
#  --------------------
#  The upstream Silicon/MediaTek/MediaTekPkg/MediaTekPkg.dsc.inc unconditionally
#  puts five MediaTek DXE drivers into [Components]:
#
#      GpioDxe, ClockDxe, PmicWrapperDxe, MT6358PmicDxe, MsdcDxe
#
#  Those drivers are written for other MediaTek SoCs (MT6789/MT6768 register
#  maps). Nothing in this firmware mounts a validated MT6897 replacement, so
#  building and dispatching them would touch hardware registers that were never
#  checked on a TB375FC. Suppressing their warnings would be worse than
#  removing them: they would still enter the FV and still be dispatched at DXE.
#
#  The bootstrap milestone only needs: SEC entry (PlatformSecLib), GOP on the
#  framebuffer LK already handed over, keypad, and the UniXNU/DarwinBoot stack.
#  None of the five MediaTek DXE drivers is on that path, so this file keeps the
#  upstream PCDs and library mapping but drops the [Components] entries.
#
#  PmicWrapperDxe deserves a note because KeypadDeviceDxe can consume it: on
#  boards whose Volume/Power keys hang off the PMIC through the PMIC wrapper
#  protocol, dropping it would remove those keys. It is not needed here - the
#  TB375FC menu keys come from the KPD matrix at 0x1c00e000, which
#  tb375fcPkg/Library/KeypadDeviceLib reads directly, and that library neither
#  includes Protocol/MtkPmicWrapper.h nor lists
#  gMediaTekPmicWrapperProtocolGuid. Reintroduce this driver, together with a
#  validated MT6897 PMIC wrapper, only if the matrix keys stop being enough.
#
#  Everything else mirrors Silicon/MediaTek/MediaTekPkg/MediaTekPkg.dsc.inc.
#  Keep the two in sync when the upstream file changes.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

# SiliciumPkg.dsc.inc is included by the platform DSC before this file, so it is
# not repeated here (a second !include of the same DSC fragment would redefine
# its sections).

[PcdsFixedAtBuild]
  #
  # ACPI
  #
  # Upstream MediaTekPkg.dsc.inc sets the OEM id here; MT6897Pkg.dsc.inc owns
  # the OEM table id and revision with this SoC's numbers.
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemId|"MTK   "

[LibraryClasses]
  #
  # ARM Libraries
  #
  # Only MdePkg libraries remain: with the five MediaTek DXE drivers out of
  # [Components] and the MediaTekPkg libraries no longer mapped, MediaTekPkg.dec
  # is off the build path entirely (see the note below about F001).
  #
  ArmSmcLib|MdePkg/Library/ArmSmcLib/ArmSmcLib.inf

  #
  # Memory Libraries
  #
  # RamManagerLib is deliberately NOT mapped to
  # MediaTekPkg/Library/RamManagerLib here, which is what the upstream
  # MediaTekPkg.dsc.inc does.
  #
  # That implementation computes the DRAM size from the MT6789 EMI_CONA /
  # EMI_CONH bitfields (Silicon/MediaTek/MediaTekPkg/Library/RamManagerLib/
  # RamManager.c). Nobody has checked those offsets against MT6897, and a wrong
  # size does not fail loudly - it silently reports the wrong amount of RAM.
  #
  # It is also not needed. Its single consumer is
  # SiliciumPkg/Drivers/RamManagerDxe, which the platform FDF never places in
  # the firmware volume (it is absent from SiliciumPkg/Common.fdf.inc,
  # SiliciumPkg/Extra.fdf.inc and MediaTekPkg/Extra.fdf.inc), so it is never
  # dispatched and the library is never called. Leaving the mapping to the
  # SiliciumPkg default (SiliciumNullPkg/Library/RamManagerLibNull, which
  # returns EFI_UNSUPPORTED) also keeps another MT6789 source file out of the
  # build.
  #
  # Reinstate this mapping together with a validated MT6897 EMI decode and a
  # RamManagerDxe entry in the FDF.

#
# There is deliberately NO [Components] section in this file.
#
# Upstream MediaTekPkg.dsc.inc lists five drivers - GpioDxe, ClockDxe,
# PmicWrapperDxe, MT6358PmicDxe, MsdcDxe - whose register tables are the
# MT6789 ones and were never verified on a TB375FC. This file replaces that
# include, so none of them is built or dispatched.
#
# The section header is omitted rather than left empty on purpose: EDK II
# merges every included fragment into one DSC, and no upstream DSC in
# mu_basecore release/202511 ships a section with no entries. Leaving
# "[Components]" with nothing under it would be the odd one out for no gain.
#
# Add a driver here only together with a validated MT6897 implementation and a
# matching entry in the platform FDF.
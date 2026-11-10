##
#  MediaTek MT6897 (Dimensity 8300) SoC support for Mu-Silicium.
#
#  Structure is a 1:1 copy of the upstream Silicon/MediaTek/MT6789Pkg, which
#  is the closest MediaTek SoC package Mu-Silicium ships. Everything that is
#  a board fact (GIC base, core/cluster count, SMBIOS identity) is replaced
#  with this unit's measured values:
#
#    E1  hw-tmp/iomem.txt                live /proc/iomem
#    E2  hw-tmp/decoded/live-fdt.dts     live device tree
#    E5  hw-tmp/cpuinfo.txt              live /proc/cpuinfo (8 cores)
#
#  Library status:
#    PlatformSecLib        MT6897 implementation, on the SEC entry path.
#    GpioImplLib,          byte copies of the MT6789 implementations. They are
#    ClockImplLib,         retained in the package as placeholders for a future
#    PmicWrapperImplLib,   MT6897 port but are NOT mapped in [LibraryClasses]
#    MsdcImplLib           below, so EDK II never compiles them into this build.
#                          Nothing here has been validated on a TB375FC.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

[Defines]
  #
  # Timer
  #
  USE_PHYSICAL_TIMER                         = 1

#
# Layer composition is owned by the platform DSC, which includes
#   SiliciumPkg.dsc.inc  ->  tb375fcPkg/MediaTekPkg.dsc.inc  ->  this file
# in that order. The MediaTek base layer the upstream package used to pull in
# here (MediaTekPkg/MediaTekPkg.dsc.inc) is replaced by the platform copy
# because it lists five MT6789 DXE drivers that were never validated on MT6897.
# This SoC package must therefore NOT include the MediaTek base layer itself;
# see Platforms/Lenovo/tb375fcPkg/tb375fc.dsc.
#

[BuildOptions]
  *_*_AARCH64_CC_FLAGS = -march=armv8.2-a+crypto+rcpc

[PcdsFixedAtBuild]
  #
  # ACPI
  #
  # E2 /soc/compatible = "mediatek,MT6897"; keep the MTK OEM id and use the
  # SoC number as the OEM revision so the tables are attributable.
  #
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemTableId|0x202039383936544D
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemRevision|0x6897

  #
  # CPU
  #
  # E5 /proc/cpuinfo: 8 CPUs. E2 /cpus: cpu@0..3 "arm,cortex-a510",
  # cpu@4..7 "arm,cortex-a715"; every one uses enable-method = "psci".
  #
  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|2

  #
  # GIC
  #
  # E2 /interrupt-controller (compatible "arm,gic-v3"):
  #   reg = <0x0 0x0c400000 0x0 0x40000  0x0 0x0c440000 0x0 0x200000>
  # i.e. distributor 0x0c400000 (256 KiB), redistributors 0x0c440000 (2 MiB).
  # NOTE: this is NOT the MT6789 layout (0x0c000000 / 0x0c040000).
  #
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x0C400000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x0C440000

  #
  # SMBIOS
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosProcessorModel|"MediaTek (TM) Dimensity 8300 @ 2.20 GHz"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosProcessorPartNumber|"MT6897"

[LibraryClasses]
  #
  # SEC Libraries
  #
  # PlatformSecLib is the only library here on the first-boot path: it runs at
  # the SEC entry and disables the watchdog before the fringe of DXE.
  #
  PlatformSecLib|MT6897Pkg/Library/PlatformSecLib/PlatformSecLib.inf

  #
  # The MT6789 GpioImplLib / ClockImplLib / PmicWrapperImplLib / MsdcImplLib
  # mappings are deliberately NOT declared.
  #
  # EDK II compiles every library instance a DSC maps, whether or not a module
  # in the build consumes it. Their sources are the MT6789 register tables,
  # none of which has been validated against this SoC, so declaring them would
  # (a) build and then silently ship another SoC's GPIO/clock/PMIC/MSDC
  # descriptions the moment a driver starts using them, and (b) drag their
  # upstream warnings into this build under -Werror.
  #
  # The sources stay in the package (Library/*ImplLib) so a future MT6897
  # implementation can replace them in place; reinstate a mapping only together
  # with a validated implementation and the driver that needs it.
  #
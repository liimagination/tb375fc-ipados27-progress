//
//  WARNING: copied from Mu-Silicium Silicon/MediaTek/MT6789Pkg.
//  NOT validated for MT6897 (TB375FC). Only PlatformSecLib is on the
//  first-boot path; this library is kept so the DSC LibraryClasses resolve,
//  and it must be re-derived from this SoC's registers before it is used.
//
#include <Library/PmicWrapperImplLib.h>

MTK_PMIC_WRAPPER_PLATFORM_INFO gPlatformInfo = {
  .RegMap = {
    [PmicWrapperInitDone2]     = 0x0,
    [PmicWrapperWacs2Cmd]      = 0x880,
    [PmicWrapperWacs2VldClr]   = 0x8A4,
    [PmicWrapperWacs2RData]    = 0x8A8,
    [PmicWrapperSwinf2WData31] = 0x884,
    [PmicWrapperSwinf2RData31] = 0x894,
  },
  .ArbCapabilities = TRUE,
};

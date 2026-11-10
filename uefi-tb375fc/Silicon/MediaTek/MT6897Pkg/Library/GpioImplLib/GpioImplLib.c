//
//  WARNING: copied from Mu-Silicium Silicon/MediaTek/MT6789Pkg.
//  NOT validated for MT6897 (TB375FC). Only PlatformSecLib is on the
//  first-boot path; this library is kept so the DSC LibraryClasses resolve,
//  and it must be re-derived from this SoC's registers before it is used.
//
#include <Library/GpioImplLib.h>

MTK_GPIO_PLATFORM_INFO gPlatformInfo = {
  .SetOffset     = 0x4,
  .ResetOffset   = 0x8,
  .DirOffset     = 0x0,
  .DataOutOffset = 0x100,
  .DataInOffset  = 0x200,
  .ModeOffset    = 0x300,
  .MaxPin        = 189,
};

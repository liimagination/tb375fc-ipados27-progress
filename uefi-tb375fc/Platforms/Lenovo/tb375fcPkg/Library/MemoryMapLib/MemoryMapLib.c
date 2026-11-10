/**
  Lenovo TB375FC (MediaTek MT6897 / Dimensity 8300) static memory map.

  Nothing in this file is inherited from another board: every address below
  comes from this unit. Sources, referred to as E1..E4:

    E1  hw-tmp/iomem.txt                live /proc/iomem (root adb, Android 16)
    E2  hw-tmp/decoded/live-fdt.dts     live device tree (dtc of /sys/firmware/fdt)
    E3  inputs/manifest.json            stock vendor_boot.img header parse
    E4  inputs/manifest.json +
        boot-and-dtbo-report.json       stock boot.img header parse

  DRAM is 8 GiB from 0x40000000 (E1: "40000000-4807ffff : System RAM" is the
  first interval, the map runs to 0x23fffffff).

  LK hands the image over at the kernel_addr of the stock vendor_boot header
  (E3: kernel_addr 0x40000000, ramdisk_addr 0x66f00000, tags_addr 0x47c80000,
  page_size 4096, cmdline "bootopt=64S3,32N2,64N2"), cross-checked against the
  live tree (E2 /chosen: linux,initrd-start 0x66f00000,
  linux,initrd-end 0x68b246ed). LK decompresses the gzip kernel field to
  0x40000000 and branches there, so the firmware volume starts at that address:

    0x40000000  FD_BASE, from Resources/Configs/tb375fc.toml [uefi_fd].base
    0x40400000  end of the 4 MiB FD == PcdCPUCoresStackBase
    0x40400000  UEFI stack, 0x40000
    0x40440000  DXE heap, up to the LK DTB reservation at 0x47c80000

  E1 proves the window is free: inside "40000000-4807ffff : System RAM" the
  only reserved runs start at 0x47c80000.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryMapLib.h>

//
// The FD window must start at the LK kernel_addr of the stock vendor_boot
// header (E3). Its length comes from PcdFdSize, and the UEFI stack begins where
// it ends, so nothing below is pinned to a particular FD size.
//
#define TB375FC_FD_BASE            0x40000000ULL
#define TB375FC_FD_AREA_END        0x47C00000ULL
#define TB375FC_STACK_SIZE         0x00040000ULL

STATIC
EFI_MEMORY_REGION_DESCRIPTOR
gMemoryDescriptor[] = {
  // Name, Address, Length, HobOption, ResourceType, ResourceAttribute, MemoryType, ArmAttribute

  //
  // --- DDR, exactly as Linux reports it in hw-tmp/iomem.txt ---------------
  //
  // The first three addresses are rewritten at run time from
  // PcdFdBaseAddress / PcdFdSize, so only they carry a placeholder.
  // Each Conv entry mirrors one "System RAM" interval of /proc/iomem; each
  // Reserv entry mirrors an interval Linux lists as "reserved" or the live
  // tree declares under /reserved-memory. The MMU map therefore reproduces
  // the map Android runs with.
  //

  // 0x40000000-0x4807ffff (System RAM)
  // The three UEFI entries below are rewritten from PcdFdBaseAddress /
  // PcdFdSize at run time; the numbers here are only placeholders that the
  // sort step needs. Their real extent is FD_BASE .. 0x47c00000, whatever the
  // configured FD size is.
  {"UEFI FD",            0x40000000, 0x00800000, AddMem, SYS_MEM, SYS_MEM_CAP, BsCode, WRITE_BACK},    // FD_BASE / FD_SIZE
  {"UEFI Stack",         0x40800000, 0x00040000, AddMem, SYS_MEM, SYS_MEM_CAP, BsData, WRITE_BACK},    // FD end
  {"DXE Heap",           0x40840000, 0x073C0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK},    // FD end + stack .. 0x47c00000
  {"DDR Kernel Tail",    0x47C00000, 0x00080000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN}, // 0x47c00000-0x47c7ffff
  // LK DTB + tags. E3: dtb_addr 0x47c80000, dt payload 411812 bytes.
  // E1: "47c80000-47d15fff : reserved". The entry spans the whole 4 MiB LK
  // DTB mblock so the DXE heap cannot reach it.
  {"LK DTB",             0x47C80000, 0x00400000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // 0x47c80000-0x4807ffff
  // 0x48080000-0x4817ffff (reserved, E1)
  {"AEE Debug KInfo",    0x48080000, 0x00010000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 /reserved-memory/mblock-1
  {"PStore",             0x48090000, 0x000E0000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 /reserved-memory/mblock-2
  {"MiniRdDump",         0x48170000, 0x00010000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 /reserved-memory/mblock-3

  // 0x48180000-0x48400fff (System RAM, E1)
  {"DDR Mid 0",          0x48180000, 0x00280000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN}, // 0x48180000-0x483fffff
  // 4 KiB hole the kernel does not report in /proc/iomem; the device tree is
  // the source: E2 /reserved-memory/mblock-6-crypto_hw 0x48401000/0x1000.
  {"Crypto HW",          0x48401000, 0x00001000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},

  // 0x48402000-0x487fffff (System RAM, E1)
  {"DDR Mid 1",          0x48402000, 0x003FE000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN}, // 0x48402000-0x487fffff

  // 0x48800000-0x489fffff (reserved, E1) - E2 /reserved-memory/mblock-4-BL31-reserved
  {"BL31",               0x48800000, 0x00200000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},

  // 0x48a00000-0x6fffdfff (System RAM, E1) up to the LK ramdisk window
  {"DDR Mid 2",          0x48A00000, 0x1E500000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN}, // 0x48a00000-0x66efffff
  // LK boot ramdisk. E2 /chosen: linux,initrd-start 0x66f00000,
  // linux,initrd-end 0x68b246ed; E3 ramdisk_addr 0x66f00000.
  {"Boot Ramdisk",       0x66F00000, 0x04000000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // 0x66f00000-0x6aefffff
  {"DDR Mid 3",          0x6AF00000, 0x04FFE000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN}, // 0x6af00000-0x6fffdfff

  // 0x6fffe000-0x6fffefff (reserved, E1) - E2 mblock-12-security_dice_common
  {"Security Dice",      0x6FFFE000, 0x00001000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},
  // 0x6ffff000-0x6fffffff (System RAM, E1)
  {"DDR Mid 4",          0x6FFFF000, 0x00001000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},

  // 0x70000000-0x74ffffff (reserved, E1) - E2 mblock-24-tee-reserved
  {"TEE Reserved",       0x70000000, 0x05000000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},

  // 0x75000000-0x7bffffff (System RAM, E1)
  {"DDR High 0",         0x75000000, 0x07000000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},

  // 0x7c000000-0x7d22ffff (reserved, E1) - E2 mblock-35-adsp_image_reserved
  {"ADSP Reserved",      0x7C000000, 0x01230000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},

  // 0x7d230000-0x7ebfffff (System RAM, E1)
  {"DDR High 1",         0x7D230000, 0x019D0000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},

  // 0x7ec00000-0x7fdfffff (reserved, E1)
  {"GPU MPU Table",      0x7EC00000, 0x00600000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-37-me_GPUmputab_PMA
  {"GPU Reserved",       0x7F200000, 0x00500000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-13-me_gpu_reserved
  // 0x7f700000-0x7fdfffff (System RAM, E1)
  {"DDR High 2",         0x7F700000, 0x00700000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},

  // 0x7fe00000-0x7fe6ffff (System RAM, E1)
  {"DDR High 3",         0x7FE00000, 0x00070000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},
  // 0x7fe70000-0x7ffeffff (reserved, E1) - E2 mblock-43-me_GPUEB_SHARED
  {"GPUEB Shared",       0x7FE70000, 0x00180000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},

  // 0x7fff0000-0x7fffdfff (System RAM, E1) with a reserved 4 KiB hole
  {"DDR High 4",         0x7FFF0000, 0x0000B000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN}, // 0x7fff0000-0x7fffafff
  {"EMI MBIST Buf",      0x7FFFF000, 0x00001000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-9-emi_mbist_buf

  // 0x80000000-0x800fffff (reserved, E1) - E2 mblock-31-mm_ccu_reserved
  {"MM CCU",             0x80000000, 0x00100000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},

  // 0x80100000-0x8cbfffff (System RAM, E1)
  {"DDR High 5",         0x80100000, 0x0CB00000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},

  // 0x8cc00000-0x8ed1ffff (reserved, E1)
  {"CONNSYS EMI",        0x8CC00000, 0x00D00000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-38-consys_emi_reserved
  {"WiFi Reserve",       0x8D900000, 0x01400000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-39 shared-dma-pool wifi
  {"SCP EMI",            0x8ED00000, 0x00020000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-40-scp_emi_reserved

  // 0x8ed20000-0x8ed4ffff (System RAM, E1) - 192 KiB between two reservations
  {"DDR High 6",         0x8ED20000, 0x00030000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},

  // 0x8ed50000-0x8f8fffff (reserved, E1)
  {"APU RV Coredump",    0x8ED50000, 0x002B0000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-33
  {"SCP Share",          0x8F000000, 0x00898000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-30-reserve-memory-scp_share
  {"DDR High 7",         0x8F898000, 0x00068000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN}, // 0x8f898000-0x8f8fffff

  // 0x8f900000-0x8fffffff (reserved, E1)
  {"APU SAPU",           0x8F900000, 0x00100000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-34-apu_sapu_apurvgzctrlshm
  {"APU RV Secure",      0x8FA00000, 0x00600000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-32-apu_apusys-rv_secure

  // 0x90000000-0x9f06efff (System RAM, E1)
  {"DDR High 8",         0x90000000, 0x0F06F000, AddMem, SYS_MEM, SYS_MEM_CAP, Conv,   WRITE_BACK_XN},

  // 0x9f06f000-0x9fffefff (reserved, E1)
  {"SSPM AP Shared",     0x9F06F000, 0x00110000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-45-sspm_ap-shared
  {"MET Res Ram",        0x9F17F000, 0x00C00000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-28-debug_met_res-ram
  {"SSPM Reserved",      0x9FD7F000, 0x00280000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_BACK},    // E2 mblock-21-SSPM-reserved

  //
  // --- Framebuffer handed over by LK -------------------------------------
  //
  // LK reserves the whole 62.56 MiB window and reports its base:
  //   E2 /chosen: atag,videolfb-fb_base_l = 0xfc16f000, atag,videolfb-fb_base_h = 0,
  //              atag,videolfb-vramSize  = 0x3e90000,
  //              atag,lcmname = "nt36532_dsi_vdo_144hz_tianma_lcm_drv".
  //   E2 /reserved-memory/mblock-14-framebuffer: <0 0xfc16f000 0 0x3e90000>.
  //   E1: "fc16f000-ffffefff : reserved".
  //
  // That window is NOT a single buffer. It is three 2944x1840x32 frames:
  //   frame size = 2944 * 1840 * 4 = 0x14AA000 (21,667,840 bytes)
  //   3 * 0x14AA000 = 0x3DFE000, plus a 0x92000 tail to the 0x3E90000 end.
  //
  // MEASURED ON THE UNIT (beacon6 and beacon7, 2026-11-07): the panel does
  // not scan frame 0. It scans frame 1, at 0xfc16f000 + 0x14AA000.
  //   beacon6 painted frame 0 red, frame 1 green, frame 2 blue with 1/2/3
  //   marker squares; the panel showed green on top with frame 1's two
  //   squares, and blue underneath starting at 95% with frame 2's squares.
  //   beacon7 then painted colour bars into frame 1 only and the panel showed
  //   them upright and correctly placed. Frame 0 was never visible.
  //
  // Programming the reported base therefore leaves the menu drawing into a
  // buffer that is never scanned out - which is exactly why every firmware
  // build so far rendered a black screen while every GOP call succeeded.
  //
  // The window is covered by four descriptors with NO gaps, because a hole in
  // the memory map leaves DXE trying to manage pages it has no descriptor for
  // (measured on 2026-11-07: declaring only frames 0 and 1, i.e. stopping at
  // 0xFEAC3000, produced an immediate data abort with FAR=0, ISS=0x45).
  //
  //   0xFC16F000  0x014AA000  frame 0  (LK leaves its logo here; not scanned)
  //   0xFD619000  0x029E6000  frame 1 + frame 2 + tail -> "Display Reserved"
  //   0xFFFFF000  -           end of LK's window
  //
  // "Display Reserved" starts at frame 1 because that is the frame the panel
  // actually scans (beacon6/beacon7), and it runs to the end of the window so
  // that the four descriptors tile the whole 0x3E90000 reservation.
  //
  // SimpleFbDxe and FrameBufferSerialPortLib both find this region by the
  // literal name "Display Reserved", so both draw from frame 1.
  {"Display Frame 0",    0xFC16F000, 0x014AA000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_THROUGH_XN},
  //
  // The name here must stay "Display Reserved": FrameBufferSerialPortLib looks
  // its console surface up by that literal string, and if the lookup fails the
  // serial port never initialises, which takes the whole ConOut/ConIn stack
  // down with it and hangs BDS on the vendor logo (measured on 2026-11-07 by
  // renaming the region to "Display Scanout").
  //
  // The console therefore does share this buffer with the menu. That conflict
  // is handled on the menu side instead, by yielding the panel once the UI owns
  // it and by logging before drawing rather than after (see UniXNULogYieldPanel
  // in Main.c and the ordering in Menu.c).
  //
  {"Display Reserved",   0xFD619000, 0x029E6000, AddMem, MEM_RES, SYS_MEM_CAP, Reserv, WRITE_THROUGH_XN},

  //
  // --- Register windows ---------------------------------------------------
  //
  // Addresses and sizes are the reg properties of the corresponding /soc
  // nodes in E2; the trailing comment names the node.
  //
  {"GIC Distributor",    0x0C400000, 0x00040000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /interrupt-controller reg[0]
  {"GIC Redistributor",  0x0C440000, 0x00200000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /interrupt-controller reg[1]
  {"Top Ck Gen",         0x10000000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /soc/topckgen@10000000
  {"Infra Cfg AO",       0x10001000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /soc/infracfg-ao@10001000
  {"Pinctrl",            0x10005000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /soc/pinctrl@10005000
  {"EMI",                0x10219000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /soc/emi@10219000
  {"UART0",              0x11001000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /soc/serial@11001000
  {"UART1",              0x11002000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /soc/serial@11002000
  {"UFS Host",           0x112B0000, 0x00003000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /soc/ufshci@112b0000
  {"UFS AO Cfg",         0x112B8000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /soc/ufshci@112b0000 reg[1]
  {"UFS PDN Cfg",        0x112BB000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /soc/ufshci@112b0000 reg[2]
  {"Display MMSYS",      0x14000000, 0x00020000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /soc/dispsys-config@14000000
  {"SPMI Master",        0x1CC04000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /soc/spmi@1cc04000 (pmif)
  // The hardware block is 0x100 bytes, but ArmConfigureMmu/updateRegionMapping
  // rejects any region whose start OR length is not 4 KiB aligned
  // ("regionStart:0x1c00a000 or RegionLength:0x100 are not page aligned",
  // "failed to configure MMU! status = invalid parameter", observed on the
  // first runnable firmware). The mapping is rounded up to one 4 KiB page;
  // 0x1C00A000-0x1C00AFFF does not collide with the Keypad at 0x1C00E000.
  {"WatchDog Timer",     0x1C00A000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}, // /soc/watchdog@1c00a000 (reg 0x100, page-rounded)
  {"Keypad",             0x1C00E000, 0x00001000, AddDev, MMAP_IO, UNCACHEABLE, MmIO,   NS_DEVICE}  // /soc/kp@1c00e000
};
VOID
GetMemoryMap (
  OUT EFI_MEMORY_REGION_DESCRIPTOR **MemoryDescriptor,
  OUT UINT8                         *MemoryDescriptorCount)
{
  EFI_PHYSICAL_ADDRESS FdBase   = FixedPcdGet64 (PcdFdBaseAddress);
  UINT64               FdEnd    = FdBase + FixedPcdGet32 (PcdFdSize);
  UINT64               HeapBase = FdEnd + TB375FC_STACK_SIZE;

  //
  // The FD window comes from Resources/Configs/tb375fc.toml ([uefi_fd]); it
  // must stay inside the LK kernel window at the kernel_addr of the stock
  // vendor_boot header, below the LK DTB reservation at 0x47c80000.
  //
  ASSERT (FdBase == TB375FC_FD_BASE);
  ASSERT (FdEnd > FdBase);
  ASSERT (FdEnd == FixedPcdGet64 (PcdCPUCoresStackBase));
  ASSERT (HeapBase < TB375FC_FD_AREA_END);

  for (UINTN Index = 0; Index < ARRAY_SIZE (gMemoryDescriptor); Index++) {
    EFI_MEMORY_REGION_DESCRIPTOR *Region = &gMemoryDescriptor[Index];

    if (AsciiStrCmp (Region->Name, "UEFI FD") == 0) {
      Region->Address = FdBase;
      Region->Length  = FixedPcdGet32 (PcdFdSize);
    } else if (AsciiStrCmp (Region->Name, "UEFI Stack") == 0) {
      Region->Address = FdEnd;
    } else if (AsciiStrCmp (Region->Name, "DXE Heap") == 0) {
      Region->Address = HeapBase;
      Region->Length  = TB375FC_FD_AREA_END - HeapBase;
    }
  }

  // Pass Data
  *MemoryDescriptor      = gMemoryDescriptor;
  *MemoryDescriptorCount = ARRAY_SIZE (gMemoryDescriptor);
}
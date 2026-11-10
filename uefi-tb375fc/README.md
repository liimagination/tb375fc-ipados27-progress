# uefi-tb375fc — Mu-Silicium platform package for Lenovo TB375FC

Lenovo Xiaoxin Pad Pro 12.7 (2025), model **TB375FC**, Android product `yili`,
SoC **MediaTek MT6897 / Dimensity 8300**. This directory is the firmware side of
the UniXNU port: it produces `SILICIUM_UEFI.fd`, which `./unixnu bootimg` packs
as the gzip kernel field of an Android boot **v4**, 4096-byte-page, **zero
ramdisk** image.

Milestone of this stage: the TB375FC-specific UEFI/UniXNU menu comes up on the
device. Nothing here claims XNU boots, and nothing here claims iPadOS 27 -
`UniXNU/docs/zh/porting/arm64e.md` states iPadOS 27 is arm64e-only and arm64e is
limited to upstream-supported devices (currently sheng/SM8550). MT6897 is not on
that list.

---

## 1. Boot path (Plan A: FD as the gzip kernel, no bootloader change)

Measured on this unit, not assumed:

| Fact | Value | Source |
| --- | --- | --- |
| vendor_boot header | v4, page 4096 | `../inputs/manifest.json` |
| `kernel_addr` | **0x40000000** | stock `vendor_boot.img` header |
| `ramdisk_addr` | 0x66f00000 | same header |
| `tags_addr` | 0x47c80000 | same header |
| cmdline | `bootopt=64S3,32N2,64N2` | same header |
| `linux,initrd-start` | 0x66f00000 | `../hw-tmp/decoded/live-fdt.dts` /chosen |
| `linux,initrd-end` | 0x68b246ed | same |
| stock boot.img | v4, header_size 1584, kernel 16,410,321 B, ramdisk 0 | `../boot-and-dtbo-report.json` |

LK decompresses the boot image kernel field to `kernel_addr` and branches to its
first byte. Because the kernel field is `gzip(BootShim + FD)`, the FD lands at
`0x40000000 + sizeof(BootShim)`, **not** at 0x40000000 - which is why a BootShim
is still needed and why `[boot_shim] requires_kernel_header = true` stays set.

### Why this board needs its own BootShim

`BootShim/BootShim.S` is a board-local override, **not** the upstream
Mu-Silicium file. Two independent reasons, each checkable from source:

**1. The upstream header cannot satisfy `scripts/unixnu/bootimg.py`.**
Upstream declares, at image-header offset 16, `.quad FD_SIZE`. `bootimg.py:38`
computes `struct.unpack_from('<QQ', shim, 8)` and compares it against
`(base, len(shim) + size)`. Since `len(shim)` is the offset of `_Payload` and is
necessarily non-zero, `FD_SIZE` can never equal `FD_SIZE + len(shim)`, so the
upstream shim fails the check unless `requires_kernel_header` is 0. The override
declares `.quad FD_SIZE + (_Payload - _Head)`, which is exactly that quantity.

**2. On this board the copy destination is the shim's own code.**
LK decompresses the kernel field to the vendor_boot `kernel_addr`, and
`[uefi_fd].base` is set to that same address (0x40000000), so the shim runs from
`[0x40000000, 0x40000000+len(shim))` and the FD is destined for
`[0x40000000, 0x40000000+FD_SIZE)` - the two ranges overlap by construction. The
upstream shim copies **forwards** (`ldp x2,x3,[x4],#16`) and the first store
already lands on instructions it is still executing. The override copies
**backwards** and, before doing so, relocates the copy routine above the whole
decompressed image, publishes it with `dc cvac`/`ic iallu`, then jumps into it.
On every other Mu device `kernel_addr != FD_BASE`, so the overlap never arises
there and the upstream shim is correct.

Reproducible measurements (`tools/check-true-upstream-shim.sh` and
`tools/check-bootshim.sh`; both need only clang + llvm-nm/objcopy):

| | upstream Mu-Silicium shim | this board's shim |
| --- | --- | --- |
| `shim.bin` size | **112 bytes** | **336 bytes** |
| `_Payload` offset | `0x70` (112) | `0x150` (336) |
| `shim[8:16]` (load address) | `0x40000000` OK | `0x40000000` OK |
| `shim[16:24]` (image size) | `0x800000` = FD_SIZE only | `0x800150` = FD_SIZE + 336 |
| `bootimg.py` requires | `FD_SIZE + 112` = `0x800070` | `FD_SIZE + 336` = `0x800150` |
| verdict | **fails by exactly 112 bytes** | passes |
| `shim[56:60]` | `ARM\x64` OK | `ARM\x64` OK |

The 112-byte delta is the point: upstream hard-codes `FD_SIZE` where the loader
expects `FD_SIZE + len(shim)`, so it can only ever pass on platforms that set
`requires_kernel_header = false`.

Honest limit: the relocating-copy structure is taken from the Lenovo TB365FC
port (`UniXNU/docs/zh/devices/tb365fc.md` - same vendor, same MediaTek LK, same
Plan A), and **has not been executed on a TB375FC**. The arithmetic above is
proven; the runtime behaviour is not.

```
0x40000000  FD_BASE, [uefi_fd].base, and PcdFdBaseAddress
0x40800000  end of the 8 MiB FD == PcdCPUCoresStackBase
0x40800000  UEFI stack, 0x40000
0x40840000  DXE heap, up to the LK DTB reservation at 0x47c80000
0x47c80000  LK DTB + tags (reserved)
0x66f00000  LK ramdisk window (reserved)
0xfc16f000  framebuffer, 0x3e90000 bytes (reserved, handed over by LK)
```

`../hw-tmp/iomem.txt` proves the FD window is free: inside
`40000000-4807ffff : System RAM` the first reserved run is at 0x47c80000.

## 2. Files

```
Resources/Configs/tb375fc.toml     UniXNU build/pack input ([uefi_fd], [boot_image])
Resources/DTBs/tb375fc.dtb         firmware device tree (magic + total size validated)
Resources/Scripts/mkbootimg.py     vendored from Mu-Silicium @964a886f (bootimg.py requires it)
BootShim/BootShim.S                board-local shim (relocating copy, see above)
Silicon/MediaTek/MT6897Pkg/        SoC package; MT6789Pkg structure, TB375FC PCDs
Platforms/Lenovo/tb375fcPkg/
  tb375fc.dec  tb375fc.dsc  tb375fc.fdf
  Include/APRIORI.inc  Include/DXE.inc
  Library/MemoryMapLib/            static memory map, every line sourced
  Library/KeypadDeviceLib/         KPD -> Simple Text Input (the only menu input)
references/                        read-only reference material, not built
```

## 3. Required integration (docs/zh/porting/uefi-firmware.md)

All of these are in `tb375fc.dsc` [Components] **and** `tb375fc.fdf`:

| Module | Where |
| --- | --- |
| `UniXNUPkg/Application/UniXNU/UniXNU.inf` | DSC Components + FDF FvMain |
| `UniXNUPkg/Driver/UniXNUDxe/UniXNUDxe.inf` | DSC Components + FDF FvMain |
| `UniXNUPkg/Driver/UniXNURamDiskDxe/UniXNURamDiskDxe.inf` | DSC + FDF + **APRIORI** |
| `DarwinBootPkg/Driver/DarwinBoot.inf` | DSC Components + FDF FvMain |
| `MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf` | DSC + FDF + APRIORI |
| `FatPkg/EnhancedFatDxe/Fat.inf` | DSC + FDF + APRIORI |
| `MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf` | DSC + FDF + APRIORI |
| `HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf` | DSC [LibraryClasses] |
| `FdtLib|MdePkg/Library/BaseFdtLib/BaseFdtLib.inf` | DSC [LibraryClasses] |

GOP comes from `SiliciumPkg/Drivers/SimpleFbDxe` reading the `"Display Reserved"`
region; Simple Text Input comes from `KeypadDeviceDxe` + `KeypadDxe` +
`tb375fcPkg/Library/KeypadDeviceLib`.

`UniXNURamDiskDxe` is in APRIORI DXE because it publishes the block device that
PartitionDxe and Fat consume, and `UniXNUDxe`'s Depex needs the Variable
architecture, which APRIORI already orders.

## 4. What is verified vs. not

Verified against this unit's own evidence:

- LK handoff addresses (`../inputs/manifest.json`, live `/chosen`).
- DRAM base/size, every reserved-memory carveout and every MMIO window in
  `Library/MemoryMapLib/MemoryMapLib.c` (`../hw-tmp/iomem.txt` +
  `../hw-tmp/decoded/live-fdt.dts`).
- GIC: GICD 0x0c400000 (256 KiB), GICR 0x0c440000 (2 MiB).
- 8 CPUs (4x A510, 4x A715), `enable-method = "psci"`, `arm,psci-1.0`, method smc.
- Framebuffer 0xfc16f000, 0x3e90000 bytes, panel Tianma NT36532 DSI video mode
  144 Hz (`atag,lcmname = "nt36532_dsi_vdo_144hz_tianma_lcm_drv"`).
- Watchdog at 0x1c00a000 (used by `PlatformSecLib`), keypad at 0x1c00e000.

**Not verified - read before trusting a boot:**

1. `PcdFrameBufferWidth/Height` = 1840x2944. That is the Android-reported
   physical resolution; the ATAG carries only the buffer address and size, not
   the geometry. SurfaceFlinger reports `ORIENTATION_270`, so the panel is
   rotated and the first image may come out skewed. `SimpleFbDxe` derives pitch
   as `Width * 4`; if the real pitch differs the picture will be wrong. The
   first boot decides this - do not "fix" it by guessing.
2. `KeypadDeviceLib`: Volume Up = KPD bit 17, Volume Down = bit 0. Those bit
   numbers are inferred from `mediatek,hw-rstkey = 0x11` (17) and
   `mediatek,hw-init-map[0] = 0x72` (114 = KEY_VOLUMEDOWN) in the live tree; no
   key press has been observed yet. The power key lives on the MT6363 PMIC and
   is **not** driven by this firmware.
3. `Silicon/MediaTek/MT6897Pkg/Library/{Gpio,Clock,PmicWrapper,Msdc}ImplLib` are
   byte copies of the MT6789 implementations. They exist so the DSC
   `[LibraryClasses]` resolve; none of them is in the FV and none is validated
   for MT6897. Only `PlatformSecLib` is on the first-boot path.
4. `[uefi_fd] size = 0x00800000` (8 MiB). Sized for the full module set rather
   than measured: the FD must equal this length exactly on every build, because
   both `scripts/unixnu/firmware.py` and `scripts/unixnu/bootimg.py` hard-check
   it. Tighten it to a measured FD once a build reports one; when it changes,
   `PcdCPUCoresStackBase` (= base + size) and the FDF `FD_SIZE`/`FD_BLOCKS`
   change with it. `MemoryMapLib.c` asserts the relation at run time.
5. `atag,boot`, `atag,masp`, memory-ssmr sizes and the SMEM windows are in the
   live tree but not modelled here; they sit above 0xa0000000 and outside the
   windows this firmware maps.

## 5. Build

```sh
# from a UniXNU checkout, with board.toml [firmware] pointing here
./unixnu build firmware --uefi ../tb375fc-port/uefi-tb375fc
./unixnu bootimg --uefi ../tb375fc-port/uefi-tb375fc --dtb <board.dtb>
```

The builder runs EDK II directly:

```
build -p tb375fcPkg/tb375fc.dsc -a AARCH64 -b DEBUG -t CLANGPDB \
      -D FD_BASE=0x40000000 -D FD_SIZE=0x400000 -D FD_BLOCKS=0x400
```

with `WORKSPACE` and `PACKAGES_PATH[0]` set to this checkout. Expected product:
`Build/tb375fcPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd`, exactly 0x400000 bytes.
The EDK II platform name in `tb375fc.dsc` is itself `tb375fcPkg` (PCD names are
not package-qualified), which is what makes the FV land under `Build/tb375fcPkg`.

Optional platform defines (each defaults to the safe value in `tb375fc.dsc`):

| Define | Default | Effect |
| --- | --- | --- |
| `ENABLE_SECUREBOOT` | 0 | image verification |
| `ENABLE_ADVANCED_LOG` | **1** | pulls `AdvancedLogDxe` (needs AdvLoggerPkg + MsGraphicsPkg from `Common/Mu`); the only log channel, see below |
| `ENABLE_FIRMWARE_DTB` | 1 | publishes `Resources/DTBs/tb375fc.dtb` via DtPlatformDxe |
| `USE_CUSTOM_DISPLAY_DRIVER` | 0 | 0 = SimpleFbDxe republishes the LK scanout |

### Why the Advanced Logger is on

The live bootargs carry `mtk_printk_ctrl.disable_uart=1` and `console=tty0`,
and the tablet exposes no UART header, so **UART is not an available log
channel**. With `ENABLE_ADVANCED_LOG=0` a first boot that stops before the menu
produces a black screen and no other evidence. With it on, the DXE log ring is
exported by UniXNU's Tools menu as QR code pages on the panel, which is the only
way to read the firmware's own account of a failure. The cost is a dependency on
`Common/Mu` (AdvLoggerPkg + MsGraphicsPkg); pass
`-D ENABLE_ADVANCED_LOG=0` to build without it.

## 6. Offline checks before anything touches the device

Three artifacts, in increasing order of what they prove.

**a. Package self-check** — do the referenced files exist in the real trees?

```sh
wsl -d Ubuntu-24.04 -u root -e bash /mnt/c/.../tb375fc-port/tools/selfcheck-platform.sh
```

Checks every INF and `!include` path in `tb375fc.dsc`/`tb375fc.fdf` against
`mu_basecore release/202511` and the Mu-Silicium checkout, every `.dec`
namespace, the `libfdt` sources `BaseFdtLib` needs, the UniXNU package inputs,
and `dtc`/`fdtput`/`fdtget`. Override the trees with `MU=`, `EDK2=`, `UNIXNU=`.

**b. Runtime contract** — will LK actually accept the built image?

```sh
python3 tools/verify-boot-contract.py --uefi <Mu-Silicium checkout> \
        --fd Build/tb375fcPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd \
        --image boot.img --dtb Resources/DTBs/tb375fc.dtb
```

1. Compiles `BootShim/BootShim.S` with the exact flags `bootimg.py` uses and
   asserts `shim[8:16] == FD_BASE`, `shim[16:24] == len(shim) + FD_SIZE`,
   `shim[56:60] == "ARM\x64"`, and that `nm -n` puts `_Payload` at exactly
   `len(shim)` (a literal pool or padding there would silently break
   `bootimg.py`'s `_Payload` check).
2. Asserts the FD on disk is **exactly** `[uefi_fd].size`.
3. Re-reads the boot image: header v4, `header_size` 1584, page 4096,
   `ramdisk_size` 0, and the kernel field inflating to exactly `shim + FD`
   followed by exactly the device tree.

Verified end-to-end with a synthetic 8 MiB FD: all three checks pass, and a
deliberately short FD (one page missing) fails both check 2 and check 3.

**c. Image geometry only** — re-read an existing image without the FD:

```sh
python3 ../tools/mkbootimg_tb375fc.py verify boot.img --kernel Image.gz-dtb --dtb board.dtb
```

Note that `mkbootimg_tb375fc.py pack` will not append the device tree twice: if
the kernel input already ends with exactly that tree (the `Image.gz-dtb` form)
it says so and appends nothing.

## 6b. How UniXNU assembles `{tree}` — required for the build to resolve packages

`scripts/unixnu/firmware.py` sets

```
PACKAGES_PATH = {uefi}:{tree}/package:{tree}/drivers/DarwinBoot:{edk2}
```

`{uefi}` is this checkout, so `MT6897Pkg/`, `MediaTekPkg/`, `SiliciumPkg/`,
`tb375fcPkg/` and `BootShim/` all resolve from it. `UniXNUPkg` and
`DarwinBootPkg` come from the UniXNU checkout and must be present under
`{tree}` with those exact directory names:

```
{tree}/package/UniXNUPkg/...              <- UniXNU/package/UniXNUPkg
{tree}/drivers/DarwinBoot/DarwinBootPkg/...<- UniXNU/drivers/DarwinBoot/DarwinBootPkg
```

``unixnu tree` normally builds that tree; on Windows the symlinks it creates
fail with `WinError 1314`, so the directory has to be assembled by hand:

```sh
TREE=/root/work/tb375fc-tree
mkdir -p "$TREE/package" "$TREE/drivers"
cp -a <UniXNU>/package/UniXNUPkg          "$TREE/package/"
cp -a <UniXNU>/drivers/DarwinBoot         "$TREE/drivers/"
```

Without this, `build` stops with an unresolved `UniXNUPkg/...` or
`DarwinBootPkg/...` INF, which reads like a platform bug but is a tree-assembly
one.

## 6b-2. The firmware device tree

`Resources/DTBs/tb375fc.dtb` is what the FDF embeds (`ENABLE_FIRMWARE_DTB=1`)
and what the boot image carries. It is generated from
`Resources/DTBs/dts/tb375fc.dts`, whose values come from the live FDT
(`/sys/firmware/fdt` on the unit).

The `.dts` does `/include/ "mt6897.dtsi"`, and that file is **not** in this
checkout - it lives in `UniXNU-mt6897-common/dts/mt6897.dtsi`. Rebuild it with
the include path supplied explicitly:

```sh
dtc -i <UniXNU-mt6897-common>/dts -I dts -O dtb \
    -o Resources/DTBs/tb375fc.dtb Resources/DTBs/dts/tb375fc.dts
```

Verified on 2026-11-06: that command reproduces the checked-in
`tb375fc.dtb` **byte for byte** (2,428 bytes). Running `dtc` without `-i` fails
with `Couldn't open "mt6897.dtsi"`, so a build that relies on the prebuilt blob
is fine, but anyone regenerating it needs the include path.

## 6c. Build result (recorded 2026-11-06)

First successful build of this platform package. Numbers below are from the build
that produced them, not from a later rebuild.

| Artifact | Value |
| --- | --- |
| Firmware volume | `Build/tb375fcPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd` |
| FD size | **8,388,608 bytes** (`0x800000`), equal to `[uefi_fd].size` |
| FD SHA-256 | `6101a25292c8abf9dfab4f8bc331a7cb88f704994cf1ba68eb0c8ead01240c44` |
| FV occupancy | 644,120 used of 8,388,608 (**7 %**) |
| Boot image | **618,496 bytes** |
| boot.img SHA-256 | `2a453c080415b3d1a6509583b0331a496130a725c3e9cb3bb0e393ba9164dff6` |
| boot.img kernel field | 609,977 bytes = gzip(336-byte shim + 8,388,608-byte FD) + 2,428-byte DTB |
| Build time | 68 s, `rc=0` |

The boot image was packed twice by two independent scripts and produced the same
SHA-256, so the pipeline is deterministic for these inputs.

All six modules required by `docs/zh/porting/uefi-firmware.md` are in the
firmware volume, and the two checks that decide whether LK will accept the image
pass: `len(FD) == [uefi_fd].size`, and the BootShim ARM64 header describes
`shim + FD` exactly (see `tools/verify-boot-contract.py`).

Two deferred items are worth recording here rather than silently carrying:

- **FD size is padded, not sized.** Only 7 % of the 8 MiB window is used. The
  size was chosen for headroom while the module set settled, and can be tightened
  to 1 MiB once boot is proven. Changing it means changing
  `Resources/Configs/tb375fc.toml`, `PcdCPUCoresStackBase` and nothing else -
  `MemoryMapLib.c` derives the rest and asserts the relation at run time.
- **The CpuDxe `TestBit` warning workaround lives in the build script, not the
  DSC.** A per-INF `[BuildOptions]` line does not reach the compiler with
  CLANGPDB in this BaseTools version; see the note at the top of
  `tb375fc.dsc` and `tb375fc-port/tools/build-fd.sh`.

## 6d. Not verified

Everything above proves the image **builds to spec**. Nothing below has been
observed on a TB375FC, and none of it may be treated as working until it is:

1. **The firmware has never been booted on this device.** No image produced by
   this package has run on hardware.
2. **Framebuffer geometry is assumed.** `PcdFrameBufferWidth/Height` = 1840x2944
   comes from Android's reported resolution, and `SimpleFbDxe` derives stride as
   `width * 4` = 7360. The ATAG carries only the buffer address (0xfc16f000) and
   size (0x3e90000), not the geometry. The panel is rotated
   (`ORIENTATION_270`), so the first image may be skewed or wrong-pitched.
3. **The panel identity is a string match, not an electrical check.**
   `atag,lcmname = "nt36532_dsi_vdo_144hz_tianma_lcm_drv"` is what LK recorded;
   nothing here confirms the rail/GPIO/DSI wiring, and this firmware never touches
   DSI anyway.
4. **Whether SimpleFbDxe can actually take over the surface LK left at
   0xfc16f000** is unproven. It locates the region by the literal name
   "Display Reserved"; if LK's handover differs from what the ATAG suggests, the
   GOP will fail or paint into the wrong memory.
5. **Whether this LK accepts a self-built `fastboot boot` image** is unproven on
   TB375FC. The geometry matches the stock `boot.img` (v4, page 4096, ramdisk 0,
   empty cmdline) and the same Plan A path is documented working on TB365FC, but
   that is precedent, not evidence for this device.
6. **There is no usable log channel until the menu appears.** UART is disabled
   (`mtk_printk_ctrl.disable_uart=1`) and the tablet has no UART header. The
   Advanced Logger ring exists, but reading it requires reaching UniXNU's Tools
   menu, which is exactly what a failed boot would prevent.

## 7. Testing rules

**Nothing is flashed.** Bring-up uses `fastboot boot` only, and only with the
user's explicit approval for each attempt. `fastboot flash`, `erase`, `unlock`
and `oem` are out of scope for this stage. No efuse / nvram / proinfo / persist /
OTP / serial / IMEI / calibration partition is touched.

Only the last boot stage actually observed on hardware may be claimed as done.

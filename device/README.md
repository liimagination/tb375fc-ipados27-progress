# UniXNU TB375FC device repository

Lenovo Xiaoxin Pad Pro 12.7 (2025), model **TB375FC**, Android product `yili`, MediaTek
**MT6897** (Dimensity 8300). 8 GiB DRAM, 1840x2944 panel, UFS storage.

This is a format 2 UniXNU device repository. The current milestone is **reaching the
UniXNU UEFI menu on the device with a RAM-booted image**; XNU, storage and userspace
bring-up follow after that.

> **OS profile scope.** iPadOS 27 images are arm64e and UniXNU restricts arm64e to supported
> devices (currently sheng / SM8550), and no MT6897 SoC support is published upstream. The
> bootstrap profile used here is the shared arm64 iPadOS 17.7.11 configuration; it identifies
> the build inputs, not a compatibility claim for this board.

## Read first

| Task | Document |
| --- | --- |
| Every port | [Porting](https://unixnu.com/porting/), [device repository](https://unixnu.com/porting/device-repository), [component catalog](https://unixnu.com/reference/components) |
| Hardware | [Device tree](https://unixnu.com/porting/device-tree), [matching](https://unixnu.com/drivers/device-tree-matching) |
| Images | [UEFI](https://unixnu.com/porting/uefi-firmware), [payload](https://unixnu.com/concepts/payload), [prelink](https://unixnu.com/concepts/prelink) |
| Verification | [Building](https://unixnu.com/building), [bring-up](https://unixnu.com/porting/bring-up) |
| Records | [Porting records](https://unixnu.com/porting/records) |

## Established hardware inputs

Every value below is measured, not assumed. Full evidence table: `../findings.md`.

- **LK handoff** (stock `vendor_boot.img` header v4): `kernel_addr 0x40000000`,
  `ramdisk_addr 0x66f00000`, `tags_addr 0x47c80000`, `page_size 4096`,
  `cmdline "bootopt=64S3,32N2,64N2"`. Live `/chosen` confirms
  `linux,initrd-start = 0x66f00000`, `end = 0x68b246ed`.
- **DRAM**: `/memory reg = <0x0 0x40000000 0x2 0x0>` — 8 GiB at 0x40000000.
- **Low window**: 0x40000000–0x47c80000 (124.5 MiB) is free of reserved regions; the UEFI FD
  and its stack live there. Reserved below 4 GiB from 0x48080000: aee_debug_kinfo, pstore,
  minirdump, crypto_hw, BL31.
- **GIC**: GICD 0x0c400000–0x0c43ffff, GICR 0x0c440000–0x0c63ffff. `/psci` is
  `arm,psci-1.0` with `method = "smc"`.
- **CPUs**: 8 cores, MIDR measured per core — cpu0-3 `0x411fd461` (Cortex-A510 r1p1),
  cpu4-7 `0x411fd4d0` (Cortex-A715 r1p0).
- **Framebuffer reservation**: `reg = <0x0 0xfc16f000 0x0 0x3e90000>` (62.6 MiB).
- **Panel**: 1840x2944; Android reports `primary_display_orientation = ORIENTATION_270`.

## Build

```sh
# From a UniXNU checkout
./unixnu defconfig tb375fc_defconfig -D ../tb375fc-port/device
./unixnu fetch -D ../tb375fc-port/device
./unixnu status --json -D ../tb375fc-port/device
./unixnu tree -D ../tb375fc-port/device
python3 scripts/porting-record.py check --device ../tb375fc-port/device
```

Firmware and boot image are built against `../uefi-tb375fc`. Because `example = true`
remains set, `build` and `bootimg` refuse to run until the menu has been confirmed on
hardware and the payload inputs are measured.

## Layout

```text
board.toml               format 2 board configuration
porting.toml             per-area porting record
feeds.toml               MT6897 SoC source pin
configs/tb375fc_defconfig
dts/tb375fc.dts          board DTS (live FDT derived)
patches/xnu/series       board kernel patches (none yet)
proprietary-files.txt    firmware input hashes
package/                 board kit and package recipes
```

## Safety

The device bootloader is unlocked. Only `fastboot boot` (RAM boot) is used during
bring-up; no `flash`, `erase`, `unlock` or `oem` command is issued. Unique/calibration
partitions (efuse, nvram, proinfo, persist, OTP) are never read into a repository.

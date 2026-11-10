# TB375FC UniXNU UEFI bring-up

This is a host-side preparation area for a **RAM-boot-only** TB375FC UEFI bring-up. It does not contain a ready-to-boot image yet.

## Verified target

- Lenovo Xiaoxin Pad Pro 12.7 (2025), model `TB375FC`, Android product codename `yili`.
- SoC: MediaTek MT6897 (Dimensity 8300); Android also reports board/platform identifiers `MT8792` / `mt6897`.
- Firmware package: `TB375FC_ZUX_1.1.04.016`, package version string `TB375FC_CN_OPEN_USER_M21.814_V_ZUI_17.0.04.016_ST_250501_SW_P98300DA2_V016_WIFI_LENOVO_PERIDOTV_USR_MP`.
- Active slot reported through ADB: `_a`.
- User reports the bootloader was already unlocked. ADB boot properties returned `flash.locked=1` and `vbmeta.device_state=locked`; verify live fastboot variables before any boot attempt.

## Source checkouts

Sibling folders in the parent work directory:

- `../UniXNU/` — UniXNU main repository and submodules.
- `../android_device_lenovo_TB375FC/` — community Android device tree; includes a stock-derived DTB and boot-image layout notes.
- `../edk2-mtk-reference/` — archived MediaTek EDK2 implementation, supporting other older devices; not a TB375FC port.
- `../mt6897-uefi-reference-sparse/` — sparse source checkout for the Xiaomi 14T (degas, MT6897) UEFI project. Reuse its SoC-level research only; its board firmware is not for TB375FC.

The user's original firmware package remains at its supplied D: drive path. The helper below extracts only the generic DTB payload from `vendor_boot.img`; it does not access NVRAM, efuse, calibration, or unique-device partitions.

## Current offline artifacts

- `PLAN.md` is the staged from-scratch port plan.
- `boot-and-dtbo-report.json` contains a corrected read-only parse of the stock `boot.img` v4 header and `dtbo.img` table.
- `dtbo-entries/` contains the single extracted DT overlay and its node inventory. It advertises three NT36532 panel variants (BOE, Tianma, Tianma paper-like); the actual active variant has not yet been identified.
- `tools/inspect_android_boot.py` and `tools/extract_dt_table.py` are local read-only inspection helpers.

The boot header's valid parse gives a 16,410,321-byte kernel, zero ramdisk, v4 header size 1584, and 4096-byte page alignment. The kernel begins with the legacy LZ4 magic. This identifies stock format only; fastboot custom-boot acceptance and payload compatibility remain unverified.

## Build status and blockers

1. No TB375FC-specific Mu-Silicium/EDK2 platform package or UniXNU device repository exists yet.
2. The Xiaomi 14T UEFI project is a promising same-SoC reference, but panel, framebuffer reservation, touch, input, boot image settings, and DRAM map must be re-derived for this Lenovo board.
3. Bootloader state is disputed by the available signals: the user says it is unlocked, while Android boot properties returned locked values. Verify live fastboot variables before attempting a custom image. No unlock, reboot, or partition write has been performed.
4. The installed Ubuntu WSL has Python and `make`, but lacks the EDK2/UniXNU build toolchain. `sudo` requires a password, so no system packages have been installed.
5. `fastboot boot` acceptance has not been checked; that check requires entering the bootloader and should happen only after a TB375FC-matched image is ready.

## First target

Build a device-matched UEFI image with UniXNU's DXE/application integration, then verify the package offline. The first on-device attempt should be a one-time RAM boot and stop at the UniXNU menu. Do not write boot, lk, preloader, vbmeta, or userdata partitions for this phase.

## Safe inputs needed from the stock package

- Exact `vendor_boot.img` DTB and header geometry.
- `boot.img` header geometry and load addresses.
- `dtbo.img` overlay inventory and hardware identifiers.
- Scatter partition geometry for non-unique partitions.
- Stock LK/preloader versions for boot-chain analysis only; do not repurpose images from other SKUs.

Avoid `efuse.img`, `nvram`, `proinfo`, `persist`, OTP, and any serial-number or calibration payloads.

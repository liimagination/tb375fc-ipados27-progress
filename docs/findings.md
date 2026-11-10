# TB375FC UniXNU 移植调查记录

## 设备与固件

- 目标：Lenovo Xiaoxin Pad Pro 12.7 (2025)，TB375FC，代号 yili；用户称 bootloader 已解锁。
- ADB 属性识别到 MediaTek MT6897（Dimensity 8300），另有 MT8792 板级标识；8 GiB RAM，slot `_a`。
- 用户提供固件：`TB375FC_ZUX_1.1.04.016`，版本串 `TB375FC_CN_OPEN_USER_M21.814_V_ZUI_17.0.04.016_ST_250501_SW_P98300DA2_V016_WIFI_LENOVO_PERIDOTV_USR_MP`。
- `vendor_boot.img` 是 header v4，页大小 4096，vendor ramdisk 26,913,632 bytes，DTB payload 大小 411,876 bytes，地址 `0x47c80000`。
- DTB payload 外层是 MediaTek Dtb Table（magic `0xd7b7ab1e`），表内唯一 entry 的 FDT 为 411,812 bytes；已提取到 `tb375fc-port/dtb-TB375FC-ZUX-1.1.04.016.dtb`。SHA-256：`fa26d193cba3503e31bad2f478fb4b38d63d73ee91c720e5afd1ae6aa2f081e0`。
- ADB 中 `ro.boot.flash.locked=1` 等属性和用户所述解锁状态不一致；这些系统属性不能代替进入 bootloader 后的 live fastboot 状态。

## UniXNU 上游流程映射

用户给的 migration 文档用于迁移已有设备仓库结构；新设备应按 [新设备移植步骤](https://unixnu.com/zh/porting/steps) 与 [SoC 支持](https://unixnu.com/zh/porting/soc-support) 文档进行。上游 bring-up 顺序大体是收集设备资料、建立共享 SoC 支持与板级配置、准备 UEFI/DTB/固件，再按平台和内存、存储、显示、输入等逐项 bring-up，最后接入 UniXNU EFI 组件、构建、启动并依据串口/屏幕日志迭代。

## 参考实现

- UniXNU 上游主仓库与子模块位于 `UniXNU/`。
- `soundrust5/Port-woa-uefi-Xiaomi-14T` 是 MT6897/Dimensity 8300 Ultra 的同 SoC UEFI 参考；其公开状态报告有 UEFI、SMP、显示和触控进展，UFS 尚不完整，USB 未完成。它针对 degas/Xiaomi 14T，板级地址、屏幕、触控、存储配置均不可直接套用。
- Lenovo TB375FC Android 社区设备树 `android_device_lenovo_TB375FC/` 提供 panel、触控和 stock boot 布局线索；其中预置 DTB 来自不同版本，不能替代本次固件提取的 DTB。
- `edk2-mtk-reference/` 为旧机型的 EDK2 代码参考，不能当作 MT6897 支持已完成的证据。

## 建议实施顺序

1. 解析本机 DTB 与 DTBO、scatter、boot/vendor_boot 头和板级外设标识；排除唯一/校准分区。
2. 以 Xiaomi 14T 的 MT6897 EDK2/UEFI SoC 支持为代码参考，建立新的 TB375FC 平台包，并从 TB375FC 的 DTB/固件确认内存保留区、显示、触控、UFS、USB 连接。
3. 加入 UniXNU 的 EFI DXE/应用集成，先完成能够显示菜单的最小路径；不要在 UFS/USB 尚未 bring-up 时假设系统安装可用。
4. 按本机 boot 镜像格式构建并离线审阅专用镜像。
5. 核对 live fastboot 解锁状态与 `fastboot boot` 支持后，临时启动，只观察是否进入 UEFI/UniXNU 菜单并收集日志；不刷分区。

## 当前限制

- 没有 TB375FC/MT8792 的现成 UniXNU 设备仓库或可用 UEFI 镜像。
- 需要自行做板级移植；“复用”范围是同 SoC 驱动和经验，不能跨机型直接用 Xiaomi 镜像。
- 当前已提取 DTB，尚未安装/配置编译工具链，尚未构建，也未重启设备。
- 不接触 `efuse.img`、NVRAM、proinfo、persist、OTP、序列号、IMEI 或校准数据。

---

# 【2026-11-06 Lead 实测新增证据】

设备已 root（KernelSU，`su` → uid=0），因此拿到了完整 live 硬件证据。原始文件在 `tb375fc-port/hw-tmp/`：
`live-fdt.dtb`（613,734 bytes，SHA-256 `41f658da5ec61963ca3848bd7f77ac56cb0b1128d95191520f2314840ee9fabc`，从 `/sys/firmware/fdt` 导出）、
`decoded/live-fdt.dts`（dtc 反编译 17,210 行）、`iomem.txt`、`cmdline.txt`、`partitions-by-name.txt`、`midr.txt`、`cpuinfo.txt`。

## 1. LK 交接地址（决定性）—— stock vendor_boot.img header v4

| 字段 | 值 |
| --- | --- |
| magic | `VNDRBOOT` |
| header_version | 4 |
| page_size | 4096 |
| **kernel_addr** | **0x40000000** |
| **ramdisk_addr** | **0x66f00000** |
| **tags_addr** | **0x47c80000** |
| dtb_size / dtb_addr | 0 / 0x0（v4 表内另放） |
| cmdline | `bootopt=64S3,32N2,64N2` |
| vendor_ramdisk_size | 26,913,632 |

live DT `/chosen` 交叉验证：`linux,initrd-start = <0x66f00000>`、`linux,initrd-end = <0x68b246ed>`（ramdisk 实际 26,915,565 B）。

**结论：TB375FC 的 LK 交接地址与 UniXNU 已支持的同厂商机型 TB365FC 完全一致**（0x40000000 / 0x66f00000 / 0x47c80000，同一 `bootopt=64S3,32N2,64N2`）。
因此 `docs/zh/devices/tb365fc.md` 的 **Plan A**（UEFI FD 作为 Android boot v4 镜像的 gzip kernel，LK 解压到 0x40000000 后跳 SEC 入口，无需 BootShim）在 TB375FC 上可直接沿用同一约定，FD base 取 **0x40000000**。

## 2. 内存与保留区（来自 live FDT /memory 与 /reserved-memory）

- `/memory reg = <0x0 0x40000000 0x2 0x0>` → **DRAM 基址 0x40000000，大小 0x2_0000_0000 = 8 GiB**（0x40000000–0x2_4000_0000）。
- 实测 MemTotal 7,857,684 kB（约 7.5 GiB），与 8 GiB 一致。
- **低区可用窗口（UEFI FD 摆放在此）**：0x40000000 起，到 0x47c80000（LK DTB 区）为止，共 0x7_c80_000 ≈ 124.5 MiB 无保留区。
- 低区必须避开的保留块：

| 区间 | 大小 | 名称 |
| --- | --- | --- |
| 0x48080000 | 0x10000 | aee_debug_kinfo |
| 0x48090000 | 0xe0000 | pstore（ramoops，cmdline 也声明 ramoops.mem_address=0x48090000） |
| 0x48170000 | 0x10000 | minirdump |
| 0x48401000 | 0x1000 | crypto_hw |
| 0x48800000 | 0x200000 | BL31-reserved |
| 0x47c80000 | （DTB + tags） | LK 传入 DTB / tags |

- 高区大块保留（不可用于 payload/内核窗口）：tee-reserved 0x70000000+0x5000000、adsp_image 0x7c000000+0x1230000、aee_lk 0x7e000000+0x800000、me_gpu 0x7f200000+0x200000、mkp 0x3e200000(高位)+0x200000、gz 0x2_3e400000+0x1a00000、SCP 0xbf300000+0xb00000、SSPM、APU/APUSYS 系列（0x8e…–0x9f…）、consys/wifi DMA（0x8cc00000+0xd00000、0x8d900000+0x1400000）、adsp_shared 0xf8000000+0x530000、platform_mtksmmu_protpgd 0xf9e00000+0x2200000、framebuffer 0xfc16f000+0x3e90000。
- **framebuffer**：`reg = <0x0 0xfc16f000 0x0 0x3e90000>`（约 62.6 MiB，位于 4 GiB 顶端下方），屏幕 1840x2944。

## 3. CPU / GIC / timer

- MIDR 实测（root 读取 `/sys/devices/system/cpu/cpuN/regs/identification/midr_el1`）：
  - cpu0–3：`0x411fd461` → Cortex-A510（part 0xd46，rev 1）
  - cpu4–7：`0x411fd4d0` → Cortex-A715（part 0xd4d，rev 1）
  - 8 核，2 个 cluster（A510 x4 + A715 x4）。
- `/proc/iomem`：**GICD 0x0c400000–0x0c43ffff（256 KiB）、GICR 0x0c440000–0x0c63ffff（2 MiB）**。
- `/psci`：`compatible = "arm,psci-1.0"`，`method = "smc"`。
- 显示/存储控制器（iomem 实测）：DSI 0x1400d000、0x1420d000；dispsys-config 0x14000000；mipi-tx 0x11e50000/0x11e60000；disp_pwm0 0x1100e000；UFS host 0x112b0000、ufsphy 0x112a0000；SD/eMMC 0x11240000；USB0 mac 0x11201000、usb-phy 0x11e40000/0x11e43000；serial 0x11001000–0x11004000（4 个 8250 口）。

## 4. 分区（`/dev/block/by-name/` 实测存在）

boot_a/boot_b、init_boot_a/b、vendor_boot_a/b、dtbo_a/b、lk_a/b、preloader_a/b、super、userdata、metadata、persist、nvram、nvdata 等（完整清单见 `hw-tmp/partitions-by-name.txt`）。

## 5. 仍不一致 / 待确认

- `ro.boot.hardware = mt8792`、live DT `model = "MT8792"`/`compatible = "mediatek,MT6897"`：SoC 标识同时出现两个名字，需在 porting 记录里写清楚（沿用上游习惯：board.platform=mt6897）。
- 设备帧率为 60/90/120 Hz 可变；`ro.surface_flinger.primary_display_orientation = ORIENTATION_270`，物理面板需按原生方向处理。
- 面板变体（BOE / Tianma / Tianma paper NT36532）尚未从 DT 里唯一确定，待从 DT 的 dsi panel 节点或 i2c 探测结果确认。

## 6. LK framebuffer 交接（决定 GOP 能否直接接管）

从 live DT `/chosen` 解出的 ATAG（全部是 LK 传给内核的原始参数）：

| ATAG | 值 | 含义 |
| --- | --- | --- |
| `atag,videolfb-fb_base_l` | `0xfc16f000` | framebuffer 物理基址低 32 位 |
| `atag,videolfb-fb_base_h` | `0x0` | 高 32 位 → **FB 基址 = 0xfc16f000** |
| `atag,videolfb-vramSize` | `0x3e90000` = 62.6 MiB | framebuffer 保留大小 |
| `atag,videolfb[4] ` | `0xe903` | **面板高度 59651？** 不可信，见下 |
| `atag,videolfb` 末尾 ASCII | `nt36532_dsi_vdo_144hz_tianma_lcm_drv` | **LCM 驱动名 → 实机面板 = Tianma NT36532** |

- 与 `/reserved-memory/mblock-14-framebuffer = <0x0 0xfc16f000 0x0 0x3e90000>` 完全一致，互相印证。
- `atag,videolfb = <0xf016fc 0x00 0x1000000 0x70170000 0xe903 ...>`：
  按 MediaTek LK 的 `struct lcm`/`video_fb` 布局，前 5 个 cell 分别是 lcm 相关字段与分辨率编码，
  `0x70170000` 与 `0xe903` 的组合不是直接的 width/height，需要用 LK 结构体定义解；
  **当前只确认基址与 vramSize，宽高/stride/像素格式仍需从运行中的 Android DRM 侧读**。
- `bootargs` 里有 `mtk_printk_ctrl.disable_uart=1`、`console=tty0`、`root=/dev/ram`、`fastboot_enable=1`、`bootopt=64S3,32N2,64N2`、
  `ramoops.mem_address=0x48090000 ramoops.mem_size=0xe0000`（与 mblock-2-pstore 一致）。

其他 LK 参数：
| 键 | 值 |
| --- | --- |
| `atag,boot` | `<0x10000000 0x2080041 0x0 0x2000000>` |
| `atag,ufs` | `<0x3000000>` |
| `mrdump,cblock` | `<0x48082000 0x8000>` |
| `log_store` | `<0xdf1100 0x10000>` |
| `ram_console` | `<0xd01100 0x80000 0x2000000 0xc00e0000>` |

**面板变体结论**：DTBO 里的三个 NT36532 变体（BOE / Tianma / Tianma paper）中，
LK 的 LCM 驱动名 `nt36532_dsi_vdo_144hz_tianma_lcm_drv` 指向 **Tianma NT36532（144 Hz DSI video mode）**，
对应 live DT `/soc/dsi@1400d000/panel2@1`，compatible `"tianma,nt36532,dsi,vdo"`。

## 7. GIC 中断单元数偏差（重要）

厂商 DT 的 GIC 节点用 `#interrupt-cells = <4>`（每个中断描述符多一个恒 0 的 cell），
标准 `arm,gic-v3` 绑定是 `<3>`。UniXNU/Silicium 的 GIC 消费者按标准 3-cell 解析，
所以共享 DTSI 采用标准 3-cell 形式，并在注释和 memory-map.md 里记录这个 deliberate deviation。
厂商 DTB 的 CPU 节点名（`cpu@0..3`、`cpu@100..102`、`cpu@200`）也与 `reg` 不一致，共享 DTSI 用 `reg` = 节点名后缀的正常形式。

## 8. `./unixnu build firmware` 的真实契约（读 scripts/unixnu/firmware.py 后确认）

UniXNU 不走 Mu-Silicium 的 build_uefi.py，而是**直接调 EDK2 原生 build**：

1. 读 `board.toml [firmware]` 与 `<uefi>/Resources/Configs/<config>.toml`。
2. 可选 `submodules`：`git -C <uefi> submodule update --init --depth 1`。
3. 可选 `preflight`：从构建树运行脚本，环境变量 `UNIXNU_UEFI`、`UNIXNU_DEVICE`。
4. defines：`FD_BASE=hex(base)`、`FD_SIZE=hex(size)`、`FD_BLOCKS=size/0x1000` + `[firmware.defines]`。
5. `PACKAGES_PATH` = `{uefi}:{tree}/package:{tree}/drivers/DarwinBoot:{edk2}`（`{uefi}`=Mu-Silicium checkout）。
6. BaseTools：若 `<edk2>/BaseTools/Source/C/bin/GenFw` 不存在则 `make -C BaseTools/Source/C`。
7. `build -p <dsc> -a AARCH64 -b DEBUG -t CLANGPDB -n <jobs> -D FD_BASE=... -D FD_SIZE=... -D FD_BLOCKS=...`
   在 cwd=`<uefi>`、WORKSPACE=`<uefi>` 下执行。
8. 硬校验：`<uefi>/<fw.fd 模板>{target}_CLANGPDB/FV/SILICIUM_UEFI.fd` **必须存在且字节数 == size**。

推论：
- `uefi-tb375fc/` 目录本身要具备 Mu-Silicium checkout 的形状：`BootShim/BootShim.S`、`Resources/Configs/<name>.toml`、
  `Resources/Scripts/mkbootimg.py`、`Platforms/Lenovo/tb375fcPkg/{tb375fc.dsc,tb375fc.fdf}`，并且能作为 WORKSPACE 使用。
- 两个可行路线：**(a)** uefi-tb375fc 作为 Mu-Silicium 的完整 checkout（clone 后把我们的平台包放进去）；
  **(b)** uefi-tb375fc 只放我们自己的文件，构建时用 symlink/复制把 Mu-Silicium 的 BootShim/Resources/Scripts 引进来。
  需要与 build-toolchain 确认哪条更稳。
- `[uefi_fd] size` 必须与 FDF 的 `FD_SIZE`/`FD_BLOCKS` 完全一致，否则 build 直接报错。

## 9. SoC 层完成（task-1 completed，已独立复核）

`UniXNU-mt6897-common/` 结构：AGENTS.md、README.md、soc.toml、configs/mt6897.config、dts/mt6897.dtsi、
patches/xnu/series、kits/README.md、docs/{memory-map,hardware-inventory}.md、tools/{check-dtsi.sh,verify_dtsi.py,gen-docs.sh}、inputs/live-hw.json。

Lead 独立复核：`bash tools/check-dtsi.sh` 通过（dtc exit 0，8 项断言全部从 live DT 读取并比对，非硬编码）。

### 最终硬件清单（可直接引用）

| 项 | 值 |
| --- | --- |
| DRAM | base 0x40000000, size 0x2_0000_0000 (8 GiB), end 0x2_4000_0000，单连续 bank 无洞 |
| FD 窗口 | 0x40000000..0x47C80000 = 0x07C80000 = **124.50 MiB**，窗口内无保留区 |
| 低区保留 | aee_debug_kinfo 0x48080000+0x10000 / pstore 0x48090000+0xE0000 / minirdump 0x48170000+0x10000 / crypto_hw 0x48401000+0x1000 / BL31 0x48800000+0x200000 |
| framebuffer | 0x0FC16F000 + 0x3E90000（62.6 MiB，ends 0x0FFFFF000，**非 no-map**） |
| CPU MPIDR | cpu0-3 = 0x000/0x100/0x200/0x300 (A510, MIDR 0x411fd461)；cpu4-7 = 0x400/0x500/0x600/0x700 (A715, MIDR 0x411fd4d0)；全部 psci |
| GIC | GICD 0x0C400000+0x40000, GICR 0x0C440000+0x200000, #redistributor-regions=1 |
| timer | PPI13/14/11/10 → INTID 29/30/27/26，clock-frequency 13,000,000 Hz |
| PSCI | arm,psci-1.0 / method="smc" |
| 面板 | tianma NT36532（panel2@1），1840x2944，340dpi，ORIENTATION_270 |
| 保留区总数 | **49 个**（45 固定 reg + 4 动态 size/alloc-ranges） |

### 关键控制器（SPI → INTID = SPI+32）

| 控制器 | 基址 | 大小 | 中断 |
| --- | --- | --- | --- |
| UFS host | 0x112B0000 | 0x3000 | SPI168/177..184 → INTID 200/209..216 |
| UFS PHY | 0x112A0000 | 0x10000 | — |
| DSI0 | 0x1400D000 | 0x1000 | SPI368 → INTID 400 |
| DSI1 | 0x1420D000 | 0x1000 | SPI412 → INTID 444 |
| mipi-tx0 / 1 | 0x11E50000 / 0x11E60000 | 0x1000 | — |
| dispsys-config | 0x14000000 / 0x14200000 / 0x14400000 / 0x14600000 | 0x1000 | — |
| disp-mutex0 | 0x14001000 (+14201000/14401000/14601000) | 0x1000 | SPI354→386 / SPI386→418 |
| disp_pwm0 | 0x1100E000 | 0x1000 | SPI254 → INTID 286 |
| serial0..3 | 0x11001000 / 0x11002000 / 0x11003000 / 0x11004000 | 0x20 | SPI250..253 → INTID 282..285 |
| SD/eMMC | 0x11240000 | 0x1000 | SPI166 → INTID 198 |
| USB mtu3 / xhci | 0x11201000 / 0x11200000 | — | SPI160→192 / SPI161→193 |
| PCIe | 0x112F0000 | 0x4000 | SPI240（disabled） |

### 三个风险（Lead 已确认并记录）

1. **面板变体是"名字匹配"不是电气实测**：结论来自 LK 写入 `/chosen/atag,videolfb` 的 LCM 驱动名
   `nt36532_dsi_vdo_144hz_tianma_lcm_drv`。信号强（LK 是真正给面板上电的组件），但在真正点屏前不得宣称显示已支持。
2. **atag 里没有 framebuffer 几何**：`atag,videolfb` 60 字节 = 20 字节头 + ASCII。
   逐字节验证：没有 32 位字等于 1840 / 2944 / 7360 / 11776 / 5520 / 8832。
   → GOP 只能拿到 buffer 地址 + 大小；几何必须来自 panel timing 或板级配置。
3. **ssmr-reserved-cma-memory 是动态分配**：size 0xA000000、align 16 MiB、alloc-ranges 0x80000000+0xC0000000，
   可能落在 0x80000000..0x140000000 任意 16 MiB 对齐处。UEFI 若在高区放固定地址必须排掉。

### 仍未测到（9 项，不得当成已知）

1. 14 个 CPU idle state 的 arm,psci-suspend-param（厂商 DT 未发布，已选择不建模而非编造）
2. CNTP/CNTV 到底对应哪个 PPI（已知的只是 binding 顺序）
3. framebuffer stride / bpp / buffer 数量
4. panel 电气 timing 与 DSI 时钟
5. DRAM rank/channel/interleave 几何
6. LK DTB 块在 0x47C80000 的确切长度（/proc/iomem 显示 0x47c80000-0x47d15fff）
7. 0x40000000..0x40010000 是否真空闲
8. 任何 UniXNU 组件在 MT6897 上的行为（soc.toml components.include = []）
9. 尚未启动过任何东西

## 10. UEFI 平台包已交付（uefi-tb375fc）

| 文件 | 大小 | 说明 |
| --- | --- | --- |
| Platforms/Lenovo/tb375fcPkg/tb375fc.dsc | 9,637 | SiliciumPkg.dsc.inc → MT6897Pkg.dsc.inc → PCD → LibraryClasses → Components |
| Platforms/Lenovo/tb375fcPkg/tb375fc.fdf | 3,519 | FvNameGuid 独立；FD 用 $(FD_BASE)/$(FD_SIZE)/$(FD_BLOCKS) |
| Platforms/Lenovo/tb375fcPkg/Include/{APRIORI,DXE}.inc | 2,461 / 2,840 | BootRamdiskDxe 已换为 UniXNUPkg/Driver/UniXNURamDiskDxe |
| Platforms/Lenovo/tb375fcPkg/Library/MemoryMapLib/ | 13,789 / 731 | 59 条描述符，每条带 E1/E2 来源注释 |
| Platforms/Lenovo/tb375fcPkg/Library/KeypadDeviceLib/ | 8,385 / 931 | KPD 0x1c00e000，Vol- bit0 / Vol+ bit17 |
| Silicon/MediaTek/MT6897Pkg/ | 13 个文件 | MT6789Pkg 结构，GIC 改 0x0C400000/0x0C440000 |
| BootShim/BootShim.S | 4,750 | 定制版（上游 779 B） |
| Resources/Configs/tb375fc.toml | 2,906 | base 0x40000000 / size 0x400000 / v4 / page 4096 |
| Resources/DTBs/tb375fc.dtb | 2,428 | Lead 用 dtc 编译（含 SoC DTSI + framebuffer 节点） |
| Resources/Scripts/mkbootimg.py | 27,032 | 从 Mu-Silicium @964a886f 原样 vendor |

### Lead 独立验证 1：BootShim 满足 bootimg.py 三条硬约束 —— PASS

`tools/check-bootshim.sh` 实测（clang --target=aarch64-none-elf, -DFD_BASE=0x40000000 -DFD_SIZE=0x400000 -DREQUIRES_KERNEL_HEADER=1）：
- `shim.bin` = 336 bytes
- `nm -n` 中 `_Payload` offset = 0x150 = 336 —— **恰好等于 shim.bin 长度**，无 literal pool/padding ✅
- `d[56:60]` = `41 52 4d 64` = `ARM\x64` ✅
- `d[8:16]` = 0x40000000 ✅
- `d[16:24]` = 0x400150 = 336 + 0x400000 ✅
- `d[:4]` = `810a0010`（合法 AArch64 分支）✅

### 构建环境事实（build-toolchain 实测）

- **git 直连不通**（解析到 198.18.x.x 透明代理假 IP，pack 传输卡死）。
- **codeload tarball 极快**：17.6 MB/s，Mu-Silicium 67 MB / 3.8s；mu_basecore 108 MB 一次成功。
- git 走镜像可用：`git -c url."https://ghfast.top/https://github.com/".insteadOf="https://github.com/"` 实测 clone 成功。
- **edk2 = microsoft/mu_basecore @ cfb93724a06ff86ff5ba971413368a7809ee42a5（release/202608）**，
  注意与 Mu-Silicium .gitmodules 里的 release/202511 不同；UniXNU 用 feeds.toml 的 pin。
- BaseTools 编出 GenFw/GenFv/GenFfs/GenSec/VfrCompile（`make -C BaseTools/Source/C -j28` 11 秒，
  rc=2 仅因缺 brotli 子模块，不影响出 FD）。
- 嵌套子模块准确 commit（GitHub API 查得）：
  - `MdePkg/Library/BaseFdtLib/libfdt` → devicetree-org/pylibfdt @ `cfff805481bdea27f900c32698171286542b8d3c`
  - `MdePkg/Library/MipiSysTLib/mipisyst` → MIPI-Alliance/public-mipi-sys-t @ `370b5944c046bab043dd8b133727b2135af7747a`
  - `MdeModulePkg/Library/BrotliCustomDecompressLib/brotli` → google/brotli @ `e230f474b87134e8c6c85b630084c612057f253e`
  - `SecurityPkg/DeviceSecurity/SpdmLib/libspdm` → DMTF/libspdm @ `f55cf6d48ec69b4ac60a63903e9c6a2cb0fd155d`
- **libfdt 铺放路径必须是两层**：`BaseFdtLib/libfdt/libfdt/fdt.c`（BaseFdtLib.inf 里 `DEFINE FDT_LIB_PATH = libfdt/libfdt`）。

### PACKAGES_PATH 缺口与解法（已确认）

UniXNU 只给 `{uefi}:{tree}/package:{tree}/drivers/DarwinBoot:{edk2}`。
DSC 里 `SiliciumPkg/...`、`MediaTekPkg/...`、`MT6897Pkg/...` 都要在 PACKAGES_PATH 项下能找到。
解法：在 Mu-Silicium checkout 根建 symlink（构建便利，不进 Git）：
```
SiliciumPkg   -> Silicon/Silicium/SiliciumPkg
MediaTekPkg   -> Silicon/MediaTek/MediaTekPkg
MT6897Pkg     -> Silicon/MediaTek/MT6897Pkg
```

### 未决
- ENABLE_ADVANCED_LOG 首启是否开启（Lead 建议开，因为 UART 不可用、无串口排针，黑屏即盲飞）。
- `[uefi_fd] size = 0x400000`（4 MiB）是否够；不够可调 8 MiB，窗口有 124.5 MiB 富余。
- 首次编译尚未执行。

## 11. BootShim 定制的算术证明（Lead 独立复现）

uefi-platform 给了两条独立理由，理由 A 我完整复现：

| shim | shim.bin | _Payload | d[8:16] | d[16:24] | bootimg.py 要求 |
| --- | --- | --- | --- | --- | --- |
| **上游 Mu-Silicium** | 112 B | 0x70 | 0x40000000 ✅ | **0x800000**（= FD_SIZE） | len(shim)+size = 0x800070 ❌ |
| **TB375FC 定制版** | 336 B | 0x150 | 0x40000000 ✅ | 0x800150 | = len(shim)+size ✅ |

上游 `.quad FD_SIZE` 与 bootimg.py 的 `len(shim)+size` 恒差一个 shim 长度（112 字节），
**因此上游 shim 在任何 `requires_kernel_header=1` 的设备上都必然过不了检查** ——
这解释了为什么它只在 `requires_kernel_header=0` 的设备上工作。

理由 B（本板特有）：`kernel_addr == FD_BASE == 0x40000000`，解压后 FD 起点与 shim 执行起点重合，
上游的正向拷贝 `ldp/stp [x4],[x5]` 第一次 store 就落在自己还在执行的指令上。
其他 Mu 设备 `kernel_addr ≠ FD_BASE`，所以不受影响。

诚实边界（已写入 uefi-tb375fc/README.md）："拷贝例程先搬到镜像上方"这一结构取自 TB365FC（同厂商同 LK 同 Plan A），
**未在 TB375FC 上实际运行验证**。算术部分已证，运行时行为未证。

复现脚本：`tools/check-true-upstream-shim.sh`、`tools/check-bootshim.sh`。

## 12. 三个平台决定（uefi-platform 提议，Lead 同意）

1. **BootShim 定制**：保留。理由如上，有可复现算术证据。
2. **ENABLE_ADVANCED_LOG = 1**：开。UART 不可用（`mtk_printk_ctrl.disable_uart=1` + 平板无串口排针），
   不开 logger 首启黑屏就是纯盲飞。代价是需要 Common/Mu 子模块（AdvLoggerPkg + MsGraphicsPkg）。
3. **FD size 4 MiB → 8 MiB（0x00800000，FD_BLOCKS 0x800）**：接受。
   连带三处同步：`PcdCPUCoresStackBase = 0x40800000`、MemoryMapLib "UEFI Stack" 0x40800000、
   "DXE Heap" 0x40840000 + 0x073C0000（正好到 0x47C00000）。
   MemoryMapLib 里有 `ASSERT(FdEnd == PcdCPUCoresStackBase)` 作安全网。

## 13. edk2 版本决策（已拍板）

**用 mu_basecore release/202511 @ 7e2a44a7129856365a8d71eff83de38877b47e66，不用 feeds.toml 的 202608。**

决定性证据：

| edk2 | `ArmBaseLib.inf` 位置 | 与 Mu-Silicium DSC 匹配 |
| --- | --- | --- |
| cfb93724 (202608, feeds.toml pin) | `MdePkg/Library/ArmLib/ArmBaseLib.inf` | ❌ |
| 7e2a44a7 (202511, Mu-Silicium @964a886f gitlink) | `ArmPkg/Library/ArmLib/ArmBaseLib.inf` | ✅ |

`SiliciumPkg.dsc.inc:135` 写死 `ArmLib|ArmPkg/Library/ArmLib/ArmBaseLib.inf`。

**合规性**：UniXNU 本身支持换 edk2 —— `cli.py` 有 `--edk2 DIR` / `UNIXNU_EDK2`，
`docs/zh/building.md` 第 203 行写明 `--edk2 DIR | Mu_Basecore 仓库`。所以这是走官方输入通道，不是绕过。

**明确否掉**：给 edk2 打 symlink 补丁（`ln -s ../../MdePkg/Library/ArmLib ArmPkg/Library/ArmLib`）——
会让 edk2 树变成非标准状态，且无法判断还有多少处类似路径差异。

## 14. 🎉 里程碑：FD 与 boot.img 均已产出并通过契约校验（Lead 独立复核）

### FD（固件卷）

| 项 | 值 |
| --- | --- |
| 路径 | `/root/work/Mu-Silicium/Build/tb375fcPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd` |
| 字节数 | **8,388,608 = 0x800000**，与 `[uefi_fd].size` 精确一致 ✅ |
| SHA-256 | `6101a25292c8abf9dfab4f8bc331a7cb88f704994cf1ba68eb0c8ead01240c44` |
| FV 占用 | FVMAIN_COMPACT 644,120 / 8,388,608 字节（7% Full，7.7 MB 余量） |
| 构建耗时 | 68 秒，rc=0 |
| 编入模块数 | **178 个 .inf** |

已确认编入的关键模块（Lead 从构建日志 `Building ... X.inf` 逐条核对）：
UniXNU ✅ / UniXNUDxe ✅ / UniXNURamDiskDxe ✅ / DarwinBoot ✅ / DiskIoDxe ✅ / Fat ✅ /
PartitionDxe ✅ / SimpleFbDxe ✅ / KeypadDxe ✅ / KeypadDeviceDxe ✅ / AdvancedLogDxe ✅ /
SmBiosTableDxe ✅ / Sec ✅ / DxeMain ✅ / ArmGicDxe ✅ / CpuDxe ✅ / TimerDxe ✅ /
MemoryMapLib ✅ / KeypadDeviceLib ✅ / PlatformSecLib ✅ / QrEncoderLib ✅ / FrameBufferSerialPortLib ✅

### boot.img（Android boot v4 零 ramdisk）

| 项 | 值 |
| --- | --- |
| 路径 | `/root/work/bootimg/boot.img`（已复制到 `tb375fc-port/out-tb375fc/boot.img`） |
| 字节数 | **618,496** |
| SHA-256 | `2a453c080415b3d1a6509583b0331a496130a725c3e9cb3bb0e393ba9164dff6` |
| magic | `ANDROID!` ✅ |
| header_version | 4 ✅ |
| header_size | 1584 ✅（与 stock 一致） |
| pagesize | 4096 ✅ |
| ramdisk_size | **0** ✅（与 stock boot.img 一致） |
| cmdline | 空 ✅（与 stock 一致） |
| kernel_size | 609,977 |
| gzip 载荷 | 8,388,944 = **shim 336 + FD 8,388,608** ✅ |
| 尾部 DTB | 2,428 字节，与 `Resources/DTBs/tb375fc.dtb` 逐字节相同 ✅ |
| shim load | 0x40000000 ✅ |
| shim image_size | 0x800150 = 336 + 0x800000 ✅ |
| shim magic | `ARM\x64` ✅ |

**Lead 独立复核**：用独立 Python 脚本解包并逐项断言（`tools/verify-bootimg-check.sh`），
全部通过；与 uefi-platform 的 `tools/verify-boot-contract.py` 交叉一致。

### 构建中解决的真实问题（累计 12 个）

1. FDF 不在 packages path → 加 `Mu-Silicium/Platforms/Lenovo`
2. `ArmBaseLib.inf` 路径 → edk2 换 release/202511 @ 7e2a44a7
3. `MsCorePkg/MemoryTypeInformationChangeLib` 缺失 → 补 Common/Mu = mu_plus @ b39b6499
4. PACKAGES_PATH typo（Silicium→Silicon）
5. 8 个 UTF-8 BOM 文件（EDK II MetaFileParser 零容忍 → KeyError BASE_NAME）→ Lead 从源头剥离
6. `MipiSysTLib/mipisyst` 缺失（MdePkg.dec:33 无条件引用）→ Lead 铺 202511 gitlink 版本
7. WSL 无 `python` 只有 `python3` → symlink
8. `ClockImplLib.c` 被 PowerShell 截断（43,248 应为 83,059）+ BOM → 用上游完整版覆盖
9. `MsdcDxe` 等 5 个 MT6789 遗留驱动的 `-Werror,-Wunused-but-set-variable` →
   **结构性地移出构建**（新建平台本地 `MediaTekPkg.dsc.inc`，`[Components]` 置空），不是加 -Wno
10. `PcdStorageIsEMMC` F001（DEC 不在 FDF 依赖图）→ 从 DSC 删除
11. `RamManagerLib` 用 MT6789 EMI 位域算 DRAM 大小（会静默算错）→ 退回 `RamManagerLibNull`
12. 上游 `ArmPkg/Drivers/CpuDxe/CpuDxe.c:305` 的 `TestBit` 警告 + `-Werror` →
    BaseTools 的 per-INF BuildOptions 到不了编译器，实际修法是在 CONF 的 tools_def.txt 副本里把
    `-Wno-unused-but-set-variable` 插回 `-Werror` 前（`-Werror` 对其他诊断仍有效）

### 关键设计决策（均已验证）

- **BootShim 必须定制**：上游 shim 的 `.quad FD_SIZE` 与 bootimg.py 要求的 `len(shim)+FD_SIZE` 恒差 112 字节，
  在任何 `requires_kernel_header=1` 的设备上必然失败。TB375FC 定制 shim 336 字节，三项检查全过。
- **FD size = 8 MiB**：FV 实际只占 644 KB（7%），但改 size 需同步 toml / `PcdCPUCoresStackBase` /
  MemoryMapLib 的 `ASSERT(FdEnd == PcdCPUCoresStackBase)`，首启成功后再收紧是零风险优化。
- **ENABLE_ADVANCED_LOG = 1**：UART 被 `mtk_printk_ctrl.disable_uart=1` 关闭且平板无串口排针，
  Advanced Logger + GOP 二维码是唯一可用的日志通道。
- **不改上游**：`../UniXNU` 保持 `7307c26e` 只读；所有板级改动在 `tb375fc-port/` 与 `UniXNU-mt6897-common/`。

## 15. 实机验证尝试 1 —— fastboot boot 不被支持（2026-11-06 07:5x）

### 执行结果（原文）

```
fastboot boot boot.img
Sending 'boot.img' (604 KB)                        OKAY [  1.996s]
Booting                                            FAILED (remote: 'unknown command')
fastboot: error: Command failed
```

**根因：这个 LK 的 fastboot 命令表里没有 `boot` 命令**，不是镜像格式问题
（镜像已被成功接收：`Sending 'boot.img' (604 KB) OKAY`，失败发生在 `Booting` 阶段）。

### fastboot live 状态（`getvar all` 实测）

| 变量 | 值 |
| --- | --- |
| product | P98300DA2 |
| unlocked | **yes** |
| secure | **no** |
| flash_lock | no |
| current-slot | a |
| slot-count | 2 |
| slot-successful:a / b | yes / no |
| slot-retry-count:a / b | 1 / 0 |
| max-download-size | 0x8000000 (128 MiB) |
| partition-size:boot_a / boot_b | **0x4000000 (64 MiB)** |
| partition-size:vendor_boot_a | 0x4000000 (64 MiB) |
| partition-size:dtbo_a | 0x800000 (8 MiB) |
| partition-size:super | 0x240000000 (9 GiB) |
| partition-size:userdata | 0x38d87f8000 (≈227 GiB) |
| protocol_version | 2 |
| modelname | TB375FC |
| hwboardid | MT8792_8+256_20 |
| build.display.id | TB375FC_CN_OPEN_USER_M21.814_A16_ZUXOS_1 |
| version-bootloader | P98300DA2_fa07c1604_202601221851 |

设备随后 `fastboot reboot` 成功回到 Android，**未写入任何分区**。

### 关键结论：本机没有"只 RAM 启动"的路径

`fastboot boot` 是唯一不写分区的测试手段，而它在这块 LK 上不存在。
所以要在实机上验证自建固件，**只能刷写 boot_a**（这正是 UniXNU 文档里 TB365FC 的做法：
"the image was flashed to this unit's boot_a only"）。

### 已准备好的回滚（只读备份，已验证）

| 文件 | 字节数 | SHA-256 |
| --- | --- | --- |
| `tb375fc-port/rollback/boot_a.stock.img` | 67,108,864 | `1ad2cb450587a101ec6fcb7e0db7cf0b3f9b9ff978cf972a7b52a909cb815357` |
| `tb375fc-port/rollback/boot_b.stock.img` | 67,108,864 | `1dd83dbf780847e8afbdb61dc2987adb4cc93539a3145e253bab638663171d96` |

用 root adb `dd if=/dev/block/by-name/boot_a` 直接读出，与设备端 sha256 逐字节核对一致。

**重要**：设备上 boot_a 的 sha256 `1ad2cb45…` 与固件包里的 `boot.img`（`57cce7b3…`）**不同**，
说明设备运行的版本（ZUXOS A16 `M21.814_A16_ZUXOS_1`）比用户提供的固件包（`M21.814_V_ZUI_17.0.04.016`）更新。
**因此固件包不能当作回滚源，只有这份 adb 备份可以。**

### 刷写方案（待授权）

```
fastboot flash boot_a boot.img      # 618 KB 写入 64 MiB 分区
fastboot reboot
```
失败恢复：
```
fastboot flash boot_a boot_a.stock.img
fastboot reboot
```

风险评估：
- boot_a 有逐字节备份 + fastboot 仍可用 → 回滚路径完整
- 兜底：TIK-5 / SP Flash Tool + 原厂固件包（用户本机已有）
- 潜在风险：AVB/vbmeta 校验（但 `secure: no`、`unlocked: yes`、且我们的镜像与 stock 一样是 `signature_size = 0`）

## 16. 实机验证尝试 2 —— 刷 boot_a 后无限重启（2026-11-06 08:0x）

### 现象

用户报告：**只显示厂商 logo，卡一屏，无限重启**。

### 执行记录

```
fastboot flash boot_a boot.img
Sending 'boot_a' (604 KB)   OKAY [2.098s]
Writing 'boot_a'            OKAY [0.198s]     ← 写入成功
fastboot reboot             OKAY
之后 60s+ 内 adb 与 fastboot 都不出现 → 不在 Android，也不在 bootloader
```

### 结论

- LK **接受了**镜像（下载与写入都 OKAY，没有 "invalid boot image" 报错）
- 但固件**没有产生任何画面输出**（只有 LK 自己的厂商 logo），随后被复位 → 说明失败发生在
  SEC 入口到 PEI 早期之间，或者 LK 解压/跳转阶段
- 无可见日志（UART 被 `mtk_printk_ctrl.disable_uart=1` 关闭，平板无排针）

### 回滚过程（已成功）

```
fastboot flash boot_a rollback/boot_a.stock.img   (64 MiB, 205s)
Writing 'boot_a'  OKAY
fastboot reboot
→ 20 秒后 Android 启动
```

验证：`sha256sum /dev/block/by-name/boot_a` = `1ad2cb450587a101ec6fcb7e0db7cf0b3f9b9ff978cf972a7b52a909cb815357`
（与备份逐字节一致）；`ro.boot.verifiedbootstate` 回到 `green`；`sys.boot_completed=1`。
**设备完好，无残留改动。**

### boot 镜像结构对比（关键发现）

| 字段 | 我们的 boot.img | 设备 stock boot_a | 固件包 boot.img |
| --- | --- | --- | --- |
| kernel_size | 609,977 | 16,898,197 | 16,410,321 |
| ramdisk_size | 0 | 0 | 0 |
| header_size | 1584 | 1584 | 1584 |
| header_version | 4 | 4 | 4 |
| **signature_size** | **4096** | **0** | **0** |
| 尾部是否附 DTB | **是**（+2,428 B） | **否** | **否** |

→ **两个明确的结构偏差**：
1. mkbootimg 默认给 v4 加了 4096 字节签名块，stock 是 0
2. `unixnu bootimg.py` 无条件在 gzip 流后追加 DTB，而 stock v4 的 kernel 字段**只有内核本身**
   （DTB 在 vendor_boot 里，不在 boot.img）

### 待验证的假设（按概率排序）

1. **附加 DTB 破坏了 LK 的解压/校验**（stock 无附加数据，kernel_size 应等于 gzip 长度）
2. **signature_size=4096 触发 LK 的签名校验路径**（stock 是 0，`secure: no` 本应跳过）
3. **gzip 不受支持**：stock 用 lz4_legacy，LK 可能只实现了 lz4（尽管 tb365fc 用 gzip 成功过）
4. **FD 本身在 SEC/PEI 早期失败**：PlatformSecLib 是 MT6789 的，watchdog 未禁用会周期性复位
5. **BootShim 拷贝逻辑**在本 LK 的装载布局下不成立

### 设备当前状态

- boot_a = 原厂内容（sha256 已核）
- Android 正常运行
- 所有分区除 boot_a 曾短暂被写入外，无其他改动
## 17. 实机验证尝试 3-6 —— 固件已能执行到打印横幅

### 突破链（每次单变量隔离）

| # | 镜像 | 结果 | 结论 |
| --- | --- | --- | --- |
| 3 | beacon2.img（144B，填 20 MiB） | logo 被从顶部涂掉，只剩底部半截安卓图标 | 我们的 AArch64 代码在跑，framebuffer 可写 |
| 4 | beacon3.img（全屏 62.6 MiB 不透明） | 全屏白 | 整块 framebuffer 可写，地址/步长正确 |
| 5 | shim + 144B beacon | 全屏白 | BootShim 逻辑正常 |
| 6 | shim + 8 MiB payload | 厂商 logo 无限重启 | BootShim 处理 8 MiB 时失败 |
| 7 | **lz4(FD) 无 shim** | **5-6 行文字（花掉），不再重启** | **固件真的起来了** |

### 第 7 次的详细结论

fd_only.img（647,168 B，kernel = lz4(FD) 原样，无 shim、无附加 DTB、signature_size=0）：
- LK 接受了没有 ARM64 Image header 的 payload（证实不需要 shim）
- 屏幕从厂商 logo 变为文字输出，说明 ClearFrameBuffer() 与 PrintFirmwareVersion() 都执行了
- 不再无限重启 → CPU 存活，无 data abort/复位
- 停在横幅后约 5-6 行处

印证 TB365FC 移植记录原话：
> No Boot Shim is needed because kernel_addr is the FD base.

**unixnu bootimg 无条件拼接 shim 的做法对这个 LK 是错的**：shim 把 FD 推到 0x40000150，
再让 shim 搬回来，而 shim 的搬迁目标 0x40870000（FD_BASE+FD_SIZE+0x70000）在真实启动时不可用。

### FD 入口链（离线验证，正确）

- FD[0:4] = 0e 28 00 14 → B #0xA038
- FV header 完整：_FVH @0x28，FvLength=0x800000，header_length=0x48
- FFS file[0] @0x48：type 0x03（EFI_FV_FILETYPE_SECURITY_CORE），size 0xFFB8 → SEC core
- 0xA038：bl PlatformAssemblyInitialize（→0x9050，空实现 ret）
- 0xA03C：mrs x0, CurrentEL / cmp / b.eq / b.eq / b . —— 与 ModuleEntryPoint.S 的 EL1_OR_EL2 宏逐条吻合
- 0xA054：mrs x0, sctlr_el1（SPAN 位设置）
→ 入口链完全正确

### 当前卡点

固件在 SecMain() 的 PrintFirmwareVersion() 之后停住。后续顺序：
InitializeDebugAgent → SaveAndSetDebugTimerInterrupt → InitializeMemory → PlatformPeim → DecompressFirstFv → LoadDxeCoreFromFv

文字花掉的原因：FrameBufferSerialPortLib 的渲染参数与实际扫描输出不完全匹配。
- GetFontScale() = min(W,H)/426 = 1840/426 = 4
- GlyphWidth = (8+1)*4 = 36 px，GlyphHeight = (16-4)*4 = 48 px
- 每行 1840/36 = 51 字符，屏幕 2944/48 = 61 行
- 控制台状态（XPos/YPos）存放在 FbBase + FbLength - 8，被 ClearFrameBuffer() 清零

### 待办

1. 拿到屏幕照片判断停在哪条日志
2. 若确认为 InitializeDebugAgent / InitializeMemory 卡住，针对性修复
3. 修正 framebuffer 文字渲染参数
## 18. 🎉 里程碑：TB375FC 专用 UniXNU UEFI 固件已在实机完整启动

### 屏幕日志实证

修复 MMU 页对齐后，实机屏幕输出：
- 大量 DEBUG 日志行（可正常阅读）
- 最后一行：`failed to find an operating system!`

该消息来自 `SiliciumPkg/Library/BootManagerLib/BootManager.c:213`：
`PlatformBootManagerUnableToBoot()` → `Print (L"No Operating System Found! ...")` → Stall 10s → `ResetSystem(EfiResetShutdown)`

### 这证明了什么

| 阶段 | 状态 |
| --- | --- |
| LK 解压 lz4 kernel | ✅ |
| LK 跳转到 payload | ✅ |
| SEC 入口 `_ModuleEntryPoint` | ✅ |
| SEC 打印横幅 / SerialPort | ✅ |
| `MemoryPeim` / MMU 配置 | ✅（修复后） |
| PEI → DXE 转换 | ✅ |
| DXE 驱动派发（Variable/GraphicsConsole/GOP/Fat/DiskIo…） | ✅ |
| BDS 启动管理 | ✅ |
| **图形输出 / framebuffer 控制台** | ✅ |

**唯一剩余问题：没有注册到可启动项。**

### 三个关键修复（每个都有实机证据）

1. **不能加 BootShim**（最根本）
   - `unixnu bootimg` 强制 `kernel = gzip(shim + FD)`，FD 被抬到 0x40000150
   - shim 需要把 FD 搬回 0x40000000，其搬迁例程目标 `0x40870000`（FD_BASE+FD_SIZE+0x70000）在真实启动时不可用
   - 实测：shim+144B payload → 正常；shim+8MiB payload → 无限重启
   - 正解：**kernel = lz4(FD) 原样**，LK 解压后正好落在 0x40000000，`FD[0]` 的 `B #0xA038` 直接进 SEC
   - 与 TB365FC 移植记录一致：「No Boot Shim is needed because kernel_addr is the FD base」

2. **framebuffer 几何是横向 2944×1840，不是 1840×2944**
   - `ro.surface_flinger.primary_display_orientation = ORIENTATION_270`
   - 用 1840×2944 时文字旋转 90° 且被斜切（每行只覆盖 7360/11776 字节）
   - 改为 `PcdFrameBufferWidth=2944`、`PcdFrameBufferHeight=1840` 后**文字完全可读**

3. **MemoryMapLib 的 WatchDog 区域长度必须页对齐**
   - 原值 `0x1C00A000 + 0x100` → 实机报错：
     `updateRegionMapping regionStart:0x1c00a000 or RegionLength:0x100 are not page aligned`
     `failed to configure MMU! status = invalid parameter`
   - 改为 `0x1000`（覆盖 0x1C00A000..0x1C00B000，不冲突 KPD 0x1C00E000）
   - 修复后 MMU 配置通过，SEC 得以继续到 DXE

### 剩余问题

BDS 阶段没有可启动项 → `UniXNUDxe` 未成功注册 UniXNU 应用。待查方向：
1. `UniXNUDxe: application <guid> not found in any FV` → `PcdUniXNUApplicationFile` GUID 与实际应用 GUID 不符
2. `UniXNUDxe: cannot write Boot0000: <status>` → 变量存储问题（PcdEmuVariableNvModeEnable / NVRAM）
3. 应用已注册但 payload 卷缺失（本镜像 ramdisk_size=0）导致 UniXNU 应用启动即退出

`VariableRuntimeDxe` 已确认在 `Include/DXE.inc` 第 10/24 行，驱动链不缺。
## 19. 🏆 里程碑达成：UniXNU 菜单在 TB375FC 实机显示

### 实机日志（用户读出，2026-11-06 10:1x）

```
UNIXNU: menu: shown 4 entries, default 0 'exit', timeout 10000 ms, input yes, rotate 2000 ms

Error: Image at 0x46AD3000 start failed: not found
unable to locate any 964E5B22-6459-11D2-8E39-00A0C969723B handles - code = Not Found
```

### 判定

这对应 `docs/zh/porting/bring-up.md` **阶段 2（UniXNU）** 的检查点：
- ✅ 日志以 `UNIXNU:` 开头
- ✅ 菜单列出条目（4 个）
- ✅ 输入设备可用（`input yes`）
- ✅ 有默认项与超时机制

`964E5B22-6459-11D2-8E39-00A0C969723B` = `EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID`。
菜单找不到 FAT 卷 —— 因为本镜像 `ramdisk_size = 0`，没有 payload 卷，
符合 `docs/zh/porting/bring-up.md` 的「最小测试：空 ramdisk，UniXNU 使用内置配置并列出可移动加载器」。

### 至此已完成的整条启动链

| 层 | 状态 | 关键证据 |
| --- | --- | --- |
| LK（MTK bootloader）加载 boot_a | ✅ | flash + reboot 正常 |
| lz4 解压到 0x40000000 | ✅ | beacon 测试 |
| FD 入口 `B #0xA038` → SEC | ✅ | 离线反汇编 + 实机日志 |
| SEC：PlatformSecLib、栈、异常向量 | ✅ | 横幅打印 |
| SEC：MemoryPeim / MMU | ✅ | 修复页对齐后通过 |
| PEI → DXE | ✅ | DXE 驱动全部派发 |
| GOP / framebuffer 控制台 | ✅ | 文字、BMP logo、图形全部正常 |
| BDS | ✅ | 启动管理运行 |
| **UniXNU 菜单** | ✅ | **shown 4 entries** |
| payload 卷 / 真实启动项 | ❌ 下一步 | 需要 FAT ramdisk |
| DarwinBoot / XNU | ❌ 后续 | |

### 三个已修复的固件缺陷（都有实机报错原文）

1. **不能加 BootShim** — `unixnu bootimg` 强制 `gzip(shim+FD)`，shim 需把 FD 从 0x40000150 搬回 0x40000000，
   但其搬迁目标 0x40870000 在真实启动时不可用（实测 shim+144B 成功、shim+8MiB 无限重启）。
   正解：`kernel = lz4(FD)` 原样，LK 解压后正好落 0x40000000。
2. **framebuffer 是 2944×1840 横向** — `ORIENTATION_270`；用 1840×2944 会导致文字旋转 90° 且斜切。
3. **MemoryMapLib WatchDog 区域须页对齐** — `0x1C00A000+0x100` → `failed to configure MMU! status = invalid parameter`；改 `0x1000` 后通过。

### 下一步

1. 让菜单停留更久 / 可交互（当前 10s 超时后选 `exit`）
2. 构建 payload FAT 卷（`[payload] volume = "TB375FCESP"`, size 192 MiB），让菜单出现 `xnu` 项
3. payload 卷需要放进 boot 镜像的 ramdisk 字段，并由 `UniXNURamDiskDxe` 暴露为块设备
## 20. 菜单渲染问题定位过程（2026-11-06 10:2x-10:3x）

### 观察序列与推断

| 构建 | 屏幕 | 推断 |
| --- | --- | --- |
| 原始（console=auto, timeout=10） | 一行 `UNIXNU: menu: shown 4 entries...`，无菜单 | 菜单可能画了后被日志清屏擦掉；或 UI 未绘制 |
| 本地覆盖 `timeout = -1` | 仍只有那一行 | 确认固件停在菜单循环等待输入 |
| 强制 `console = "text"` | 仍只有那一行 | 文字模式也没画 → 不能排除 ConOut 问题 |
| **把 `menu: shown` 日志移到 `DrawFrame` 之前** | **全黑，连那行字都没有** | **UI 确实进入了 graphics 模式并 `FillRect` 涂黑了整屏**，但条目/标题未绘制 |

### 关键结论

最后一个实验排除了「日志清屏」这个假说：日志已前置，画完仍然是黑屏 ——
说明 `UniXNUUiDrawFrame()` 的**背景填充生效**（把日志行覆盖掉了），
但随后的标题/条目/状态行绘制没有产生可见内容。

即：`Ui->Mode == UniXNUUiGraphics` 成立，GOP 可用，`EfiBltVideoFill` 可用，
但 `EfiBltBufferToVideo` 的文本/图形绘制没有出现在屏幕上。

### 当前最可疑方向

1. `Ui->Scale` 在 landscape 下 = `FB_HEIGHT/540` = 1840/540 = **3**，
   `GlyphHeight = 19*3 = 57`，`GlyphWidth = 8*3 = 24`；若有坐标越界，`FillRect` 会静默裁剪，
   而 `Blt` 直接调用不过界检查。
2. `DrawGlyph` 用 `AllocatePool` 分配的行缓冲 + `EfiBltBufferToVideo` 逐个字形输出，
   在本板 framebuffer 布局下可能整体偏移（例如 GOP 的 PixelsPerScanLine 与我们的 width*4 不一致）。
3. `SimpleFbDxe` 的 `Mode->Info->PixelsPerScanLine` 与 `HorizontalResolution` 若不一致，
   `EfiBltBufferToVideo` 会按 Blt 缓冲区语义绘制，导致内容画到屏幕外。

### 本次会话新增的本地覆盖（不进上游 checkout）

`/root/work/local-unixnu/`（构建时通过 `UNIXNU_ROOT=` 指向）：
- `package/UniXNUPkg`：UniXNUPkg 的本地副本
  - `Library/UniXNULib/Config.c`：内置配置 `timeout = 10` → `-1`（有输入设备时永久等待）
  - `Library/UniXNULib/Menu.c`：`UniXNULog("menu: shown ...")` 移到 `UniXNUUiDrawFrame()` 之前
- `drivers/DarwinBoot`：软链到上游

上游 `UniXNU/` 保持 commit `7307c26e...` 未改动。
## 21. 菜单渲染排查：结论与待办（2026-11-06 10:4x）

### 已确认的硬事实

| 事实 | 证据 |
| --- | --- |
| 固件完整启动到 BDS | 屏幕出现 UniXNU 启动 logo、警告三角、`Failed to Find an Operating System!` |
| 图形栈可用 | BMP 解码 + GOP BLT 渲染出启动 logo 与警告图形 |
| UniXNU 应用运行 | 日志 `UNIXNU: menu: shown 4 entries, default 0 'exit', timeout ... , input yes` |
| 按键驱动初始化 | 日志 `KPD base 0x1C00E000` |
| Advanced Logger 初始化 | 日志 `Advanced Logger Info = 472EA058, Max = 473EA000` |
| 无 payload 卷 | 日志 `UNIXNURAMDISK: no entry argument HOB` |

### 菜单不显示的排查链（4 次刷写，逐步排除）

| 构建 | 改动 | 屏幕结果 | 排除/结论 |
| --- | --- | --- | --- |
| A | 原始 | 一行 `menu: shown` 日志，无菜单 | 日志在绘制之后，可能触发控制台整屏 ZeroMem 擦掉菜单 |
| B | 日志移到绘制之前 | 全黑，日志也没了 | 排除「日志顺序」假说；说明 `UniXNUUiDrawFrame` 之后屏幕被清空 |
| C | 关闭 logo（`[ui] logo=false`） | 全黑 | 排除「552×552 logo 渲染越界」假说 |
| D | 在横向路径注入**红色**全屏、纵向路径注入**蓝色**全屏 | 全黑 | **红蓝都没出现** → 代码没走到任何一条图形路径 |
| E | 关闭控制台整屏清屏（改为覆盖最后一行） | 全黑 | 连日志都没有 → 应用在打印之后很快失去屏幕输出 |

### 当前最可信的判断

D 是最有力的证据：**红蓝探针都没有出现**，说明 `UniXNUUiDrawFrame()` 从未执行到图形绘制分支。
结合 `Ui->Mode` 的三态判定逻辑：
- 进入 graphics 需要 GOP 获取成功（`ConsoleOutHandle` 上取，失败则 `LocateProtocol`）
- 进入 text 需要 `gST->ConOut != NULL` 且 `QueryMode` 成功
- 否则 headless（不绘制任何内容）

⇒ 应用运行在 headless 或 text 状态，GOP 对它不可见。

### 下一步（比继续盲刷更有效）

**先建立可靠的日志通道**，再改 UI。当前唯一日志通道是 framebuffer 控制台，而它会与 UI 争用同一块显存；
这就是为什么每次调整 UI 都会把日志一起弄没。

建议按顺序做：
1. **在 `UniXNUUiInit` 里把 mode 结果画成颜色条**（不走 GOP 时也画），或用 `SerialPortWrite` 直接写一段固定文本到 framebuffer，
   确认 `Ui->Mode` 的实际取值。这是纯诊断，一次刷写即可收敛。
2. **确认 GOP 在 app 上下文里是否存在**：
   - BDS 的 `PostGopSetup` 会把 GOP 安装到 `gST->ConsoleOutHandle`；
   - 但 `PlatformBootManagerBeforeConsole` 里的 `EfiBootManagerUpdateConsoleVariable(ConOut, &DisplayDevicePath, NULL)`
     可能触发 ConPlatformDxe 断开/重连，导致 app 启动时 GOP 短暂不可见；
   - 验证方法：在 app 里 `LocateHandleBuffer(ByProtocol, GOP)` 计数并画成颜色。
3. 若确认 GOP 不可见，改为**在 app 里直接从 `EFI_DTB_TABLE` 或固定地址取 framebuffer**，不依赖 GOP 绘制菜单。

### 已保存的本地诊断补丁（都在构建树，上游未改）

`/root/work/local-unixnu/package/UniXNUPkg/Library/UniXNULib/`：
- `Config.c`：内置配置 `timeout = 10` → `-1`；`[ui] logo = false`
- `Menu.c`：`menu: shown` 日志移到 `UniXNUUiDrawFrame()` 之前
- `Ui.c`：注入 `mProbeRed`（横向）与 `mProbeBlue`（纵向）全屏探针

`/root/work/Mu-Silicium/Silicon/Silicium/SiliciumPkg/Library/FrameBufferSerialPortLib/FrameBufferSerialPortLib.c`：
- `AdvanceNewLine()`：到屏底不再 `ZeroMem` 整块 framebuffer，改为覆盖最后一行

### 设备状态

已完整恢复：`boot_a` / `boot_b` sha256 与备份逐字节一致，`verifiedbootstate = green`，`sys.boot_completed = 1`。
## 2026-11-07 官网及现场复核
- 官方 migration 是已有仓库格式迁移指南；菜单目标按 UEFI 固件/bring-up 文档执行。
- 官网 arm64e 当前只列 sheng/SM8550。TB375FC 的 iPadOS 27 尚未获得验证，不等于证明硬件无法移植。
- 现场 Android 已启动，ADB root，slot _a，verifiedbootstate=green。

## 2026-11-07 新证据：ARGB scanout / GOP Reserved 字节
已读 tb375fc-port/HANDOFF-FAILED-2026-11-07.md。该记录指出 alpha=0 的 beacon2 显示黑，alpha=FF 的 beacon7 可见；这不能单凭白闪就证明完整菜单曾可见。
源码：UniXNULib/Ui.c PIXEL 宏 Reserved=0；Logo.c mInk Reserved=0；edk2-202511 FrameBufferBltLibVideoFill 仅合并 RGB masks，丢弃最高字节；BGR BufferToVideo 使用 CopyMem 原样复制。故菜单输出未满足交接所述的 ARGB scanout 条件。
独立单变量实验：仅 SimpleFbDxe 写入成功后将目标像素高字节 OR 0xFF；不修改 UI、日志、BDS 时序、内存表。原工程未改。主机实际 GOP wrapper 通过 GCC ASan/UBSan：三种写操作、读操作、失败返回、stride/padding/矩形边界、RGB 保留与非 BGR 格式。尚未实机验证。

2026-11-07 实机 alpha 实验：用户确认已进入菜单，音量键/电源键均无响应。菜单显示检查点通过，输入未通过。发现 KeypadDeviceLib 的音量减 release 分支先清 mVolumeDownTime 再检查 >0，导致永不发送事件；电源键在现有实现中未接入。另需检查 BDS 过早启动与 ConIn 连接。

2026-11-07 输入单变量实验：音量加短按无效。源码 BdsEntry.c 在约720行调用 PlatformBootManagerBdsEntry，而连接默认控制台约955行、AfterConsole约972行。板级 BdsEntry 同步 StartImage 进入永久菜单，阻止后续控制台连接。新隔离树 tb375fc-input-test 仅将 StartUniXnuApplication 从 BdsEntry 移到 AfterConsole；保留 alpha 修复，暂不叠加音量减状态机修复。当前设备不在 ADB/fastboot，等待手动进入可连接模式。

2026-11-07 控制台时序实验实机结果：用户确认菜单正常显示，音量加仍不移动。启动时序修改未解决输入。下一版仅新增菜单底部只读 KPD 五个 MEM 寄存器 RAW/CHG、ConIn 存在状态和物理输入实例计数；不再推断位号，先取得真实变化。

2026-11-07 按键硬件证据：用户观察 RAW 第一个值由 FFFF 变为 FDFE，至少确认 bit0 随音量减变化（另有bit9变化，暂不推断）。IN=1 DEV=2 已由照片确认。修复音量减 release 在清零前发送队列事件。实际提取状态机通过 GCC ASan/UBSan 测试：短按、1秒确认、松开重复、空闲、3秒关机、再次按键。仅修改板级音量减状态机，保留诊断文字及已验证显示。

2026-11-07 实机成功：用户确认‘音量减好了’。当前 boot_a 为 menu-voldown-fix/boot-voldown.img（SHA256 7fb5224e64b372d9dae5768de7ca878757eaa7e3707c7ccfc9816f48e5ed6d70）。确认菜单显示与音量减导航通过；1秒按住松开确认尚未获得用户结果，音量加/电源未适配。显示修复：GOP 写入目标矩形 alpha FF；输入修复：release 清零前入队。后续不要退回不含 alpha 修复的原始构建。

2026-11-07 菜单里程碑通过：用户确认音量减短按导航可用，按住约1秒松开能进入 Tools。显示、导航、确认三项实机验证完成。仅确认 UEFI/UniXNU 菜单，尚未启动 DarwinBoot/XNU/iPadOS。音量加/电源未完成。当前 boot_a 保留可用实验版。

2026-11-07 BSP照片取证：EL1，MPIDR=81000000，ISAR1=0010111101211052，ISAR2=0000000000000000，MMFR0=0000022200101122，TCR=0000000280803518。字段解码 APA=5 API=0 GPA=1 GPI=0；TGran4=0，TG0=0 TG1=2。启动核声明地址与通用PAC，当前EL1，4KiB granule受支持且TCR的TG0为4KiB。实际PAC执行、五组密钥可写、全核及EL0尚未测试，不将此记录成iPadOS27兼容通过。

2026-11-07 PAC照片：KEY=10/10 PAC=0 TRAPS=0 ESR=0 CLEAN=Success。五组密钥半寄存器同值读写核对通过；PAC签名变化且认证还原的组合条件未通过，不能判定CPU不支持。用户报告survey及active两版音量减回归；三版KeypadDeviceLib.c SHA256均69e363ed3874ab30120d0a1d2f025e20058a09374aaf021c1212368b07a02052，故不是源码遗漏。候选隔离：恢复成功版Menu.c中原KPD采样/500ms更新和CountInputDevices路径；PAC仅首个tick运行和显示一次。增加SCTLR/EnIA读数，不启用PAC。

用户指出诊断两行交替显示，照片证实，不能当成正常。UiDrawStatus两行Y间距Ch+Ch/3，几何不重叠。候选修复：PAC结果仅首tick绘制，KPD轮询保留但停止其status2覆盖；SimpleFbDxe写目标矩形后增加WriteBackDataCacheRange，MemoryFence仅排序不保证DMA可见。尚待实机验证，不宣称根因已确定。

2026-11-07 用户反馈‘修好了’，照片确认两行稳定可见。新读数KEY10/10 PAC0 TRAPS0 CLEANSuccess ENIA0 SCTLR=300D180D（照片转录）。EnIA0下原组合测试不能证明PAC不可用。下一候选在单一asm区间临时EnIA=1，分别记录SIGN/AUTH/RESTORE；启用故障跳过、PAC故障先恢复、恢复故障报告RESTORE0。保留当前显示/输入候选，不变更密钥值。

2026-11-07 用户确认临时启用PAC检查结果符合预期：ENIA1 SIGN1 AUTH1 RESTORE1 TRAPS0 CLEAN=Success（KEY先前10/10）。启动核心EL1原生PAC签名/认证与原SCTLR恢复实机通过。其他核心/EL0尚未测试，仍不能宣布完整arm64e资格。

Payload发现版实机未出现新标题。下一诊断仅只读显示EntryHob、x0/FDT合法性、测得固定DTB处header状态、chosen initrd范围、区内TB375FCESP FAT签名及SFS数。尚无payload发现成功证据。
27输入：四项原始组件提取完成；Apple参考LZFSE固定提交构建成功。官方kernelcache函数解码80,035,840字节，SHA256 38e6d9fa5f789b8228f4617ef4c6d2a3fd8ab564abdc9b62920f1db53fc1780a与27 profile pin一致。此为Apple原始输入，不是本板内核。

2026-11-07 payload诊断照片：HOB=1 X0=47C80000 X0FDT=1 FIXEDFDT=0 FS=0；INITRD=66F00000..68B246E9 HEAD=184C2102 FAT=0。入口HOB/FDT交接已成功。HEAD的小端字节02 21 4c 18为legacy LZ4，当前chosen仍为原厂ramdisk，未见自带2MiB FAT。判断boot ramdisk未被该init_boot设备装入当前initrd（符合AOSP Android13+generic ramdisk位于init_boot的设计），不能再用修改菜单解决。标准后续需备份当前init_boot后制作对应ramdisk镜像，涉及新增分区写入时须具体说明并确认；也可研究固件自带payload交付，但不能把FAT嵌入小FD当成完整27内核路径已解决。

2026-11-07 已恢复boot_a原镜像（fastboot传输204.995s，写入及重启成功），Android sys.boot_completed1、verifiedbootstategreen、root可用。只读备份init_boot_a/b各8MiB，设备端块哈希与PC文件一致：a=175134b44de8ca3577acefb14aed68ecff26b23465bc024d471af159d95ce3f3；b=2daeb1f36095b44b318410b3f4e8b5d989dcc7bb023d1426c492dab0a3053e74。init_boot_a原ramdisk2554718字节。制作2MiB FAT init_boot_a候选，保留原header页除ramdisk_size/signature_size，删除候选尾部AVB元数据，要求unlocked。候选SHA256048398d9169e8239ee7f89e94e82d0a74fcab9d879ce7c4eb9cb262ac72ef118。尚未写init_boot，等待明确确认。

2026-11-07 官方bring-up阶段3配置发现实机通过：用户确认标题已变为TB375FC payload FAT detected。原因已收敛：boot v4中的自带ramdisk未被装入chosen initrd；改从init_boot_a交付2MiB FAT，UEFI在vendor前缀后的完整FAT切片发布只读块设备，外部配置加载成功。当前boot_a及init_boot_a均为实验版；Android回滚需恢复两者。XNU/DarwinBoot内核交接尚未开始。

2026-11-07 下载网络：Windows本地代理127.0.0.1:7897已监听但系统ProxyEnable=0，curl必须显式--proxy。WSL无法访问其自身127.0.0.1:7897；WSL直连Git在忽略/root/.gitconfig的ghfast改写后取得指定LLVM commit，checkout按需取14891个对象。旧180秒checkout限时不足，已改1800秒并保留对象。Windows代理压缩包备用持续增长，尚未校验完成。

LLVM PAC研究：上游PR136828仍draft且说明authenticates-null-values仅解析未实现，不能直接套PR当语义完成。查Swift官方Linux发行包作为候选编译器，下载隔离D盘，尚未验证兼容性。

Swift6.4.2 Ubuntu24.04官方包1124819616字节下载完成，Swift官方发布key指纹52BB7E3DE28A71BE22EC05FFEF80A866B47A981F签名Good。隔离提取clang21实测仍只接收1-3参数，XNU builtin测试也为0，候选排除（未替换现有编译器）。源码分析固定LLVM58c46: PointerAuthQualifier已有AuthenticatesNullValues位，CGPointerAuth传递位但emitPointerAuthResign仍无条件null passthrough，单改parser不够。需补syntax、动态读写/转换/copy、常量/聚合初始化的null语义及IR回归。重新获取LLVM独立源码（原setup在成功后删除了源码和对象），既有已安装工具链不改。
2026-11-07 panic诊断版实机：用户报告先绿条后全屏花屏，无蓝紫条或panic文字确认。照片中DarwinBoot video base=0xFD619000, display=1, row=11776,width=2944,height=1840,depth=32；加载physical base可辨0x48004000，args可辨0x48048000，entry照片换行/重叠需慎读，不确定值不作结论。DarwinBoot实例是指向Windows组件仓库的symlink，任何加载器改动应先复制解除symlink，避免污染共享原仓库。上游建议green-bar阶段隔离entry/MMU，不把花屏当成功。
用户报告MMU后6秒版一直花屏，无自动重启。结论仅为检查点信号未观察到，不能直接断言MMU之前停止；已向用户纠正。公开案例复核：LittleStar-OuO/UniXNU-tb365fc README明确UEFI菜单成功、XNU尚未构建/启动（MT6835，17profile），不是已跑通iPadOS案例；gauguin BRINGUP.md确认入口蓝条和pre-MMU紫条但post-MMU未证，缺条不证明未执行。无已发现同TB375FC全屏花屏已解决案例。下一候选一套菜单含cache清理后MMU关闭前/关闭后/kernel首入口三个计时重启点；关闭kernel原绘图探针，使测试不依赖显存写入。
USB检查：Win32_SerialPort仅列蓝牙COM4/COM5，USB PnP报告Android ADB Interface但adb devices为空、fastboot识别HA227LJ3。当前镜像没有MT6897 USB device/CDC调试后端，不能把安装主机debug_terminal当作设备已可抓日志。官方guide/debugging明确giulia CDC/GSC1协议需要板级实现。XNU12377原生内核已完成构建，不需重新拉源；当前适配阻塞在交接与可观察性。
三检查点A实机反馈：停在绿条随后自动重启。用户未提供精确时长，因此不记录为实测6秒；现象与A预设重启路径一致，支持已到达缓存维护完成、关闭MMU前的检查点，确认此固件状态下计时/PSCI重启路径可工作。不能据此推断关MMU后或XNU入口。下一步同镜像选择B，无需再次刷写。
三检查点B实机：保持花屏，无自动重启；A此前绿条后重启。因此关注MMU/cache关闭与其后代码/重启路径，不把XNU C启动当故障起点。源码发现：DarwinEnter只对内核owned window DC CIVAC，未覆盖自身PE代码；选定EDK2 CoreLoadImage调用InvalidateInstructionCacheRange，ArmCacheMaintenanceLib实现DC CVAU仅至PoU后IC IVAU，故自身代码PoC可见性没有在handoff中保证。候选增加DarwinEnter完整范围DC CVAC至PoC，并在UEFI服务退出前AT S1E1R/PAR_EL1核验代码每页VA==PA，失败返回错误。此为基于证据的假设修正，不宣称硬件根因已证实。
用户实机确认code-clean版B/C都能自动重启且无花屏。相同kernel/ADT/payload下，补齐DarwinEnter自身代码PoC可见性后B由失败转通过，C首内核入口也通过；identity guard必然已通过。支持缓存修正有效并证实EL1关MMU后loader及kernel首入口可执行。精确时长未测。C替换首分支后仅运行reset stub，尚未执行原XNU bootstrap，不等于内核初始化或iPadOS已启动。
新实机画面已出现可读XNU data abort：PC=0xfffffff007a89aa0，x0=0，ESR=0x96000005，FAR=0。用实际kernel symbolizer定位为IODTGetICellCounts+0x34，指令ldr x16,[x0]，非PAC认证故障。调用栈包含IODTMapInterruptsSharing（LR=0xfffffff007a86b5c）、IODeviceTreeAlloc、IOPlatformExpertDevice::init、ConfigureIOKit，证明已进入XNU IOKit初始化，非仅首入口。源码IODT gIODTPHandleKey="AAPL,phandle"，当前实际ADT生成器仅输出FDT名phandle；FindPHandle不能登记GIC，interrupt-parent解析NULL。官网marble-milacy及MiLacy/UniXNU-marble README记录相同函数同原因，支持修正ADT字段而非绕过内核空指针。
2026-11-08新图：image4 interface not available @kern_trustcache.c:592，首frame0xfffffff0078bc304精确符号为trust_cache_runtime_init。当前源码该处检查img4if==NULL，之后还检查amfi/TrustCache.version。说明ADT修正后越过IODT空指针，进入信任缓存初始化；当前--skip-prelink原生内核未接原始AppleImage4/AMFI组件。OS27 profile列有相应roots与early security init。原kernelcache实际Mach-O filetype12 MH_FILESET，native为2 MH_EXECUTE；实际冻结DarwinBoot.c已有fileset解析，旧README所写不支持已过时。RELEASE-LTO比较参考尚未生成，官方reference builder有xcrun依赖，后续需适配现有Linux工具链。未新增刷写，未恢复Android。WSL曾重启使D盘ext4 bind丢失，本次仅按build-kernel27.py已验证前缀重挂载（不执行编译），marker检查通过。

2026-11-08 主机续修：RELEASE-LTO比较内核已完成，arm64e/iOS27、无未定义符号，SHA d1be4f12df221991d0bcd6615fe954013280a6b15f12cc0f94652795633e4a8b；它仅用于符号配对，不可当作启动内核。四安全组件严格预链接在缺失 _unixnu_kernel_stack_shift 处停止。核对实际 stack.c 后发现当前16KiB栈、4KiB页、三保护页，现有分配路径并不明确满足fixture声称32KiB对齐的前提，因此没有添加未经证明的常量或更改保护页。先隔离AppleImage4+corecrypto的真实依赖闭包。修复预链接器在组件未入选时仍要求其per-site原生符号的问题；入选位置仍严格检查，63项host测试通过（新增未选/入选/全选缺符号回归）。当前仅主机运行严格Image4预链接，无allow-missing、无新增刷写或重启。

2026-11-08 本轮完成：定位原生MH_EXECUTE缺LC_SEGMENT_SPLIT_INFO，原因现有standalone模式XNU_KERNEL_CHAINED_FIXUPS=0。经官方cli.prepare/patches.prepare恢复目标refer链接后，复用对象、以XNU_KERNEL_CHAINED_FIXUPS=1完成原生重链接。输出prelink27/native-fileset/kernel.macho，23,693,928 bytes，SHA256 1c6e73c93f4a4f0e92315506d00dadeeab7cefdca8e82baf03d927a5a86339d1。arm64e/iOS27/MH_EXECUTE、可执行入口0xfffffff00753a488、433137条有效split-segment记录、chained fixups、无未定义符号。原kernel27-native和所有实机候选未覆盖；WSL原始mach备份于/root/work/tb375fc-prelink27/native-threaded-backup。脚本tools/build-native-fileset27.py可复现该重链接，日志qualification在输出目录。
严格Image4+corecrypto闭包重试已完成，11项依赖包含CoreTrust和IOCryptoAcceleratorFamily；56处未解析引用/39个唯一原始地址，无stub，工具正确拒绝生成candidate。summary、完整report、map、log、unresolved-targets.json已保存prelink27/image4。其余阶段不再是编译错误，而是原始安全组件的ABI配对；不能仅根据相邻地址名称猜测绑定（部分reference-order邻近命名明显跨到recount等其他函数族）。AMFI栈布局接口仍未修改或验证。XnuPrelink63测试再次通过，既保留未选组件过滤，也验证入选missing报错和正确site绑定。全部主机进程已结束；本轮未新增设备操作。下一步对39目标逐个以原始机器码、调用者、PAC/vtable及源接口证明配对，之后重新严格预链接；完整iPadOS启动仍未完成。
`n2026-11-08 继续ABI核对：已准备逐目标原始反汇编、PAC diversity交集和原生checked-shape候选收集脚本。仅收集证据，不自动写入配对；邻近符号名不得充当身份。Capstone安装在任务私有/root/work/tb375fc-prelink27/python，不改变全局Python环境。
`n2026-11-08 39目标review完成并保存reviewed-binding-evidence.json。38个函数按源码/原始机器码/PAC/调用流命名，1个const OSBoolean*绑定kOSBooleanTrue（指向静态singleton，非GOT二级指针）。SHA接口为SHA1Init/SHA1Update，不是先前仅从邻近SHA512名称猜测的SHA384；IOService三指令wrapper为waitForMatchingService，不是相邻addMatchingNotification。原TXM storage-data wrapper绑定同ABI的kernel_image4_storage_data，不能用genericabort填充。严格预链接重试启动，尚未认定成功。

2026-11-08 ABI推进：40个函数identity+1个kOSBooleanTrue常量指针已审核并固定原始函数SHA。严格Image4闭包缺失从56降7后降至0，无stub。首个严格完整集合95,125,504B SHA ccef5d6ed833536179a851142256975568b8cab29be22c55d86783ac29787097，但实际DarwinBoot parser拒绝：root subtype c0000002 vs native member80000002，private生产代码line264。没有删除检查或刷写。
LLVM24 MachObjectWriter.cpp明确硬编码arm64e user ABI0、不支持kernel标记；native编译已有-mkernel和-fptrauth-function-pointer-type-discrimination。修私有ld64使显式-kernel输出正确kernel ABI0，未知version仍拒绝，不改全局工具链。首次make漏跟踪templateheader依赖，smoke如实失败；触发OutputFile.cpp重编后通过：user subtype80000002、kernel c0000002、函数指针真实AUTH discriminator分别0/2981、ABI1输入被拒绝。patch/smokequalification已保存prelink27。现复用对象重链接，再严格预链接并运行原生产parser。旧native及首个严格输出保留。

2026-11-08 平台 ABI 绑定推进：新增26个经审查接口，14个原始函数除BTI外与native完整机器码相同；其余按源码、成员偏移、dispatch slot、PAC、参数及返回值验证。共享override PAC必须用完整函数体区分基类/派生类。原始PAC也可能包含多个地址，因此改用该原始目标存在且native目标唯一的断言，不谎称原始集合唯一。四组件严格检查missing由162目标226引用降为136目标197引用；仍拒绝输出完整candidate，未使用allow-missing或stub。63项现有工具测试通过。证据prelink27/security/reviewed-platform-bindings.json，复现tools/review-platform-bindings27.py。README及EN/ZH组件文档更新。设备无操作；当前不需要用户输入。下一步审查剩余136目标（包括数据/虚表与复杂平台方法）。

2026-11-08 继续适配：平台第二批9方法（完整源码/机器码/PAC/参数/返回流），5个metaClass常量指针+2个gMetaClass直接对象（getter及类名初始化交叉验证），6个类身份已确认且全槽位数和认证布局完全一致的虚表。严格预链接由136目标197引用降为114目标165引用；candidate_written=false，stubbed空。68测试通过。证据security/reviewed-platform-batch2.json、reviewed-metaclass-pointers.json、reviewed-platform-vtables.json。队列已刷新114项，缓存3hit，4.612秒。中断注册original多出ap-wake-interrupts初始化helper并更新全局，native源码缺该逻辑，未盲绑；PMLog/睡眠包装仍待证明。虚表getMetaClass位置按地址点+0x38，即符号基址+0x48核对，首次用符号+0x38读到release后已纠正，最终全部身份断言通过。未重编内核，未执行设备命令。下一批可核对OSMetaClassBase虚表13槽唯一布局及thread_block包装；仍需严格证据，未写入这些待审项。

2026-11-08 新最小首启镜像已完成并获用户明确授权刷入：内核1f8a71624ac210d9c023e320eaa7d083c4bc887b72b72e81cb9c6b0a12995b90，MAC启动钩子实际调用/slot0x1e8/PACe4fe验证通过。元类精确构造参数审查11项无错，并确认此前自动RootParent虚表实为IOService，已纠正。Image4/corecrypto严格最小闭包0missing/0stub、生产parser通过，95,125,504字节，SHA1817068d295e5112749c171fe53e7b1fe217dc8664218d4a4e8d980a41efa24f。独立载体FAT128MiB/FD32MiB，FD40000000..42000000，栈42000000..42040000，堆至47c00000，低于LK DTB47c80000；内核窗口动态从UEFI可用内存分配。保留按键/菜单源码；扩容FAT解析ASan/UBSan通过。EDK2使用上一版edk2-202511修正首次路径错误；大GUID section超过16MiB使用扩展头，打包审核已适配。候选boot19,345,408字节，SHA59841a20bf65e63fdacb242c4efe9f61aadbcf708dbf21de1067083c33ef9fa7，v4零ramdisk，回退为上一版UniXNU而非stock Android。实机只读确认HA227LJ3/P98300DA2/slot a/boot_a64MiB后仅flash boot_a；发送59.193s、写入0.200s均OKAY、exit0；随后fastboot reboot OKAY。等待用户菜单及XNU画面，尚未宣称越过Image4 panic。全安全链92是旧native91ce上的最后统计，不能当作当前新native1f8的最终数量；本轮优先准备最小实机测试。
# 2026-11-08 接手审计：XNU payload 运输路径

## 修正后的状态判断

- `image4-first-start/nopayload-flash-result.json` 记录了实机同槽、仅删除 FDF 内 payload FREEFORM 后 SEC 断言消失并进入 UniXNU 菜单。32 MiB FD 与 91.75 MiB early heap 未改变；FVMAIN 从 130.93 MiB 降到 2.92 MiB。说明上一轮的 SEC 卡点已解决，不需要再压缩 FD/FAT 来解决 SEC。
- 这次菜单启动镜像为 boot v4，boot kernel 字段约 736 KiB，ramdisk 0；它只证实固件菜单可显示。没有证据证明大 XNU FAT payload 到达 UEFI。
- 历史 `initboot-payload/qualification.json` 的 payload FAT 为 2 MiB，记录仅“FAT payload/config discovery”；它与 95,125,504-byte 最小 Image4 内核不是同一个负载，也装不下该内核。
- 官网 `docs/zh/guide/boot.md` §vendor-boot-payload 明确：GKI 设备 boot 不带 ramdisk，较大 payload 放 `vendor_boot` 的 `vendor_ramdisk`，boot 使用 `--ramdisk none`；官网 `docs/zh/porting/uefi-firmware.md` §payload-discovery 要求 LK FDT `/chosen/linux,initrd-start/end` 交给 `UniXNURamDiskDxe`，由 FAT 驱动发布卷。与此一致的官方在线页：[UEFI 固件](https://unixnu.com/zh/porting/uefi-firmware) 与[启动镜像](https://unixnu.com/zh/guide/boot)。
- 实机已测 initrd `/chosen` 范围 `0x66f00000..0x68b246ed`，约 28.14 MiB；低于 95.1 MB 内核所需 FAT 容量。`iomem` 中 `0x70000000..0x74ffffff` 为 reserved，不能假设把 initrd 起点不变地加大就安全。
- 更正：虽然 `live-fastboot-backup/vendor_boot_a.img` 因 fastboot 不支持 fetch 而为 0 字节，但 D 盘仍有用户提供的 ZUI 17.0.04.016 原厂包。其 `vendor_boot.img` 为 64 MiB、SHA256 `935af3d217a57f9f9b39ec90772eacd1df77a92cefb73d1d4d14aeec5dd7db54`，header v4 / page 4096 / header 2128 bytes / ramdisk address `0x66f00000` / vendor ramdisk 26,913,632 bytes / DTB offset 26,918,912 / DTB 411,876 bytes / 2 个 table entries（platform 25,874,495 bytes；recovery 1,039,137 bytes）/ table entry 108 bytes / bootconfig 0。读取工具 `tools/parse_vendor_boot.py` 之前错误地按 Android `ANDROID!` boot header 解读；现已改为只读 AOSP vendor_boot v3/v4 布局解析，并将解析结果存到 `vendor-boot-v4016-report.json`。
- 该包是 ZUI 17.0.04.016（2025-05），而设备最后采集的系统指纹为 Android 16 / ZUXOS `OS4.0.0.7.XAKCN`。因此该文件只可用于格式和历史布局分析，不能作为当前活动槽 `vendor_boot` 的默认刷写基底。ADB root 曾采集到的分区内容并非可用 vendor_boot 备份；`fastboot fetch` 不支持。

## 下一步门槛

后续按用户最新指示回到 Image4 阶段，逐项修复与实测，不再先扩展 vendor_boot 路线。

## DXE payload consumer 纠正与当前候选

- 更新的检查发现 `/root/work/tb375fc-image4-test27/unixnu/package/UniXNUPkg/Driver/UniXNURamDiskDxe/UniXNURamDiskDxe.c` 中存在 `PublishEmbeddedPayload()`，通过数值化 EFI GUID `4238B7F0-0271-4B91-A5A1-827375FC0027` 调用 `EFI_FIRMWARE_VOLUME2_PROTOCOL.ReadSection()` 并发布嵌入的 FAT。此前 handoff 仅用字符串 `grep 4238B7F0`，漏掉了 C 结构化 GUID，因此“无消费者”判断错误。
- `UniXNURamDiskDxeEntry` 先调用 `PublishEmbeddedPayload()`，只有 `EFI_NOT_FOUND` 才回退到 FDT `/chosen` initrd；所以无需为了此板此阶段重打 vendor_boot。可按原实现把 payload 放回 FDF RAW section，但必须把 DXE consumer 改为识别压缩格式并解压。
- 对实际 128 MiB FAT 执行 LZMA-Alone host 压缩，压缩输出 21,123,379 bytes（约 20.14 MiB；不是 4.6 MiB）。嵌入后 FVMAIN 约为 payload 压缩体积 + 固件其余约 2.93 MiB，远小于 91.75 MiB early heap；由于压缩 section 仍超过 16 MiB FFS 阈值，`FwVol.c` 大 FFS 修复仍是必需输入。
- `MemoryMapLib.c` 描述了全局实测 8 GiB DRAM，`0x90000000..0x9f06efff` 等高位区作为 UEFI conventional memory，故 DXE 阶段分配展开 FAT 与后续 DarwinBoot kernel window 有可用区域；实际连续分配和保留范围仍需在设备/loader 报告中确认，不能仅以总 RAM 大小代替实测。
- 当前下一步仅实现/构建“压缩 FDF payload → DXE 解压 → 相同 FAT 哈希与卷身份 → 菜单配置项可见”阶段。通过后再启动已选中的最小 Image4 XNU entry，单独记录是否越过 `image4 interface not available`。不把 menu 或 FAT discovery 记成 XNU 成功。


## Image4 stepwise payload candidate (2026-11-08)

The currently authorized direction is Image4 with one stage tested at a time. The immediate historical block was payload transport/SEC FV size, not a demonstrated AES failure. An isolated copy now stores the 128 MiB FAT as an 18,570,799-byte LZMA-Alone RAW section. `UniXNURamDiskDxe` decompresses only after DXE allocation, enforces a 128 MiB output ceiling/exact size and a 32 MiB scratch ceiling, validates FAT metadata, then installs read-only Block I/O. The original 128 MiB payload SHA256 is `675f3100ac4eb54723ba4435d2a4c49ea2429d17a3b0ae2f7ed64930a8e847f6`; the host decompression round-trip matches it.

Host build passed: 32 MiB FD SHA256 `28117da8525218c03d964dd7a969622a11d70093ffbf95e337d3dc4889f39405`; FVMAIN is 21,647,360 bytes and compact-FV use is 19,501,432/33,554,432 bytes. Candidate: `tb375fc-port/image4-lzma-step/boot-image4-test.img`, SHA256 `f5b8b6a9a36afa19f31e95166d9e6b008641bdbd8a3f2b6be9b18454368d2c41`, 19,607,552 bytes, v4 with zero ramdisk. Package audit confirms the compressed payload is in the FV, decompresses exactly, production DarwinBoot parser passes, and boot header/range checks pass. `qualification.json` says not device-written/not hardware-tested.

Do not characterize the earlier `image4 interface not available` panic as AES without a kernel-side AES trace. No current image has yet reached XNU on hardware. Next test boundary is boot_a → SEC/menu → DXE LZMA/FAT payload discovery; if that passes, then test the Image4/XNU entry and capture panic/debug evidence.

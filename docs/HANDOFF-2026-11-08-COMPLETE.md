# TB375FC / UniXNU / iPadOS 27：完整工作交接记录

更新时间：2026-11-08（Asia/Singapore）  
设备：Lenovo 小新 Pad Pro 12.7 (2025)，TB375FC / yili，MediaTek MT6897  目标：最终在平板上进入 iPadOS 27 可见界面

> 这份文档按可核验产物和实机记录整理。`构建成功`、`固件菜单显示`、`XNU 进入`、`iPadOS 界面出现`是四个不同状态，不能互相替代。当前没有达到 iPadOS 桌面/界面。

## 1. 当前结论

- **最近一次确认的菜单实机测试通过。** 去掉 FDF 中未压缩的 128 MiB FAT 后，TB375FC 能进入 UniXNU 菜单。记录：`image4-first-start/nopayload-flash-result.json`。
- **当前最新的 LZMA payload 候选只完成主机验证，未刷到设备。** 它把 128 MiB FAT 压缩到 18,570,799 B 放进固件 RAW section，DXE 解压后发布 FAT。实机还没验证其 SEC、DXE 分配、菜单 payload 发现过程。
- **曾经在另一版镜像上实际进入 XNU 的启动流程。** 可读 panic 为 `image4 interface not available @kern_trustcache.c:592`，首帧符号解析为 `trust_cache_runtime_init`。该镜像未提供相应原版 AppleImage4/AMFI 运行时组件闭包，所以不能据此说 AES 有问题，也不能说当前 Image4 已经修复。
- **PAC 已在 BSP 的 EL1 范围做过实测，结果通过。** 寄存器调查、PAC key 读写和 PACIA/AUTIA 均有记录；其他 CPU 核和 EL0 仍未测。此前把 PAC 也笼统写成未验证是不准确的。
- **iPadOS 27 资格仍有上游阻塞。** UniXNU 官网当前 arm64e 页面要求 SoC 支持列入上游，而当前列表只有 SM8550/sheng（开发中）；本地 MT6897 支持不等于上游已接纳。即便 PAC 硬件可用，仍不能声称 TB375FC 已满足完整 arm64e/iPadOS 27 支持条件。
- 用户刚说明平板没电。最近主机检查 `adb devices` 与 `fastboot devices` 都没有设备，当前无法继续实机测试。充电后再进入 bootloader 并确认设备身份。

## 2. 证据状态图例

| 状态 | 含义 |
| --- | --- |
| 实机通过 | 设备屏幕/日志或 fastboot 结果实际观察到，并有结果记录 |
| 主机通过 | 构建、解析、静态检查、sanitizer 或打包核验通过；不代表设备运行 |
| 实机失败 | 设备实际停在该阶段，记录了 panic/画面/重启 |
| 未验证 | 没有足够证据，或测试仅覆盖了部分核/阶段 |
| 未实现 | 目标子系统尚无能工作的实现 |

## 3. 硬件和设备事实

| 项目 | 当前记录 |
| --- | --- |
| 机型 | Lenovo TB375FC，小新 Pad Pro 12.7 (2025)，Android 产品代号 yili |
| SoC | MT6897 / Dimensity 8300；Android 属性曾显示 `ro.soc.model=MT8792`，板级平台显示 `mt6897`，本项目按 MT6897 取证 |
| CPU | 原厂设备树 8 核：4× Cortex-A510 + 4× Cortex-A715，PSCI 启动 |
| GIC | 原厂设备树声明 GICv3；地址与中断节点属于设备树证据，不能当成全部寄存器已实测 |
| DRAM | 设备树/Android 取证记录为 8 GiB；可用区与 reserved 区需按 live DT/iomem 处理，不能假定一段连续低内存 |
| Android/固件身份 | 取证时 Android 16；本地旧固件包标记 ZUI 17.0.04.016，不能不经核对就作为当前系统镜像刷回 |
| fastboot 身份（历史） | 序列号 `HA227LJ3`，product `P98300DA2`，曾报告 unlocked=yes、slot a |
| boot_a | 历史 fastboot 查询为 64 MiB；重新测试前需重新查询当前设备和分区容量 |
| LK/boot | 实测验证过 Android boot header v4、4096-byte page、ramdisk=0 的启动包装 |

主要原始输入和解析产物：`inputs/`、`dtbo-entries/`、`hardware-live/`、`device/evidence/`、`boot-and-dtbo-report.json`、`tb375fc-dtb-inventory.json`、`vendor-boot-v4016-report.json`。`live-fastboot-backup/vendor_boot_a.img` 为 0 字节占位文件，不是有效分区备份；设备 fastboot 不支持 `fetch`。

## 4. 完成的工作

### 4.1 设备资料和移植框架

- 采集并保存 live FDT、cmdline、iomem、分区线索、CPU 信息等；原厂 DTB/DTBO 已离线解析。
- 建立 `tb375fc-port/device/` 和本地 `UniXNU-mt6897-common/` 工作树，含板级 DTS、配置、硬件清单等。
- UniXNU CLI 的 defconfig/status/tree/build 工作主要在 WSL 进行；Windows 下 symlink/EDK2 工具问题已绕过。
- 本地 SoC 支持尚不等于 UniXNU upstream 支持。MT6897 资源、寄存器、中断和驱动需要长期审查并固定版本。

### 4.2 UEFI 和 UniXNU 菜单

- Mu-Silicium/EDK2 的 TB375FC 固件平台可构建，UEFI FD 基址使用 `0x40000000`，当前实验使用 32 MiB FD。
- 菜单版本能在实机显示；用户确认音量减可导航，长按音量减可确认进入 Tools。某一版的音量键策略为短按导航、长按约 700 ms 确认。
- 菜单底部诊断文字跳动问题已做修复；显示参数曾记录 framebuffer `0xFD619000`、2944×1840、rowbytes 11776、32 bpp。
- 电源/锁定键并未证明实现；音量加也未验证。按键和菜单源码在 LZMA payload 候选构建中保持原样（qualification 记录了源文件哈希）。
- 菜单显示与 UniXNU 能启动 XNU 是不同阶段。

### 4.3 payload 发现测试

- 旧 `init_boot_a`/`boot_a` 试验曾使用约 2 MiB FAT；用户确认菜单识别到 `TB375FC` payload FAT 标题。
- 该试验只证明小卷/配置发现，不包含当前 95 MB 内核，无法证明完整 XNU 文件可读或启动。
- 此试验曾涉及 `boot_a` 和 `init_boot_a`；记录指出恢复原系统时两者都要还原。不要把它与当前 LZMA 固件内嵌方案混为一谈。

### 4.4 DarwinBoot → XNU 的历史实机阶段

这些是不同时间、不同镜像的逐步诊断：

1. 早期画面先出现绿色标记，随后整屏花屏；仅凭花屏不能断定 XNU 已经正常进入。
2. 通过 A/B checkpoint 将故障收窄：MMU 关闭前的绿色阶段会重启；MMU 关闭后的 checkpoint 曾花屏且无预期重启。随后修正 loader checkpoint 的缓存维护/identity guard 实验后，B、C checkpoint 都能自动重启且不再花屏。这证明 loader 测试点能走到，不代表 XNU 已完整启动。
3. 恢复 XNU bootstrap 后出现可读 Kernel data abort：`IODTGetICellCounts(...)+0x34`，`ldr x16,[x0]`，`x0=0`。原因定位为生成 ADT 的 GIC 节点只有通用 `phandle`，没有 XNU 读取的 `AAPL,phandle`。
4. 修复 ADT phandle 后，错误前进到 `image4 interface not available @kern_trustcache.c:592`；首帧定位到 `trust_cache_runtime_init`，即 AMFI trust cache 初始化时 `img4if == NULL`。它是 Image4/TrustCache 运行时接口缺失信号，并非 AES 诊断。
5. 后续内核/组件闭包工作针对该运行时依赖推进，但**当前 LZMA 候选还没有再次在设备上运行 XNU**，不能宣布 panic 已修复。

关键硬件记录：`xnu-handoff-checkpoints/hardware-result.json`、`xnu-handoff-code-clean/hardware-result.json`、`xnu-code-clean-boot/hardware-result.json`、`xnu-adt-phandle-fix/hardware-result.json`、`xnu-panic-test/hardware-result.json`。文件夹内 flash-result 记录对应不同历史 boot_a 写入，不能据此推断今天设备上的分区内容。

### 4.5 PAC 与 arm64e 实测范围

用户指出 PAC 之前验证过，记录确实支持这个说法：

- 用户提供的 arm64e survey 屏幕参数：BSP `CurrentEL=EL1`，`MPIDR=0x81000000`，`ISAR1=0x0010111101211052`，`ISAR2=0`，`APA=5`、`GPA=1`，`TGran4=0`，`TCR_EL1=0x0000000280803518`。
- PAC active 测试：BSP EL1 访问 key registers 与 PAC 指令的试验记录为 `KEY=10/10`、`AUTH=1`、`SIGN=1`、`RESTORE=1`、`ENIA=1`、`TRAPS=0`、`CLEAN=Success`。这证明该测试上下文中的 EL1 PAC 路径可用。
- 覆盖范围明确为 **BSP only / EL1**。其他 CPU、EL0、所有系统运行状态下的 PAC 能力还未通过官网所列的完整核验。
- 记录：`arm64e-survey/hardware-result.json`、`pac-enable/hardware-result.json`，实验产物目录有对应 qualification。

所以不要说“PAC 完全没测过”，也不要说“完整 arm64e 资格已通过”。正确摘要是：**BSP 的 EL1 PAC 已实测通过；全核、EL0 与上游 SoC 资格尚缺证据。**

### 4.6 XNU 构建与 prelink/ABI

- 有一个原生 arm64e XNU 27.0 / deployment iOS 27 的 `MH_EXECUTE` 构建产物：`kernel27-native/qualification.json`，大小 22,248,464 B，SHA256 `5c7f78fdfe1e801ac0ebfaeab70aaf4b8bc742a505e93db3c3b9735245560e3b`，入口 `0xfffffff00753a488`，undefined symbols 为空。**这是内核构建证据，不是实机启动或 iPadOS 用户态证据。**
- 另有后续 native fileset 产物：23,694,376 B，SHA256 `1f8a71624ac210d9c023e320eaa7d083c4bc887b72b72e81cb9c6b0a12995b90`，arm64e subtype `0xc0000002`，chained fixups、433,159 split references、undefined symbols 空；AMFI stack shift 和 IOKit service-start 检查符号导出。它也未实机验证。
- kernelcache 原始输入在记录中 SHA256 `38e6d9fa5f789b8228f4617ef4c6d2a3fd8ab564abdc9b62920f1db53fc1780a`；profile 使用 iPadOS 27.0.1 / build 24A446 / XNU 12377.121.6 的工作流。
- 已审查 native OS 补丁：AMFI 栈粒度/保护页处理、IOService start 的 MAC 检查。prelink ABI 工作有逐函数机器码、PAC、参数/返回行为、metaClass 和 vtable 核对；工具曾通过 72 项测试。早期“完整安全组件链零缺失”的说法不成立；严格最小 closure 曾单独实现 0 missing / 0 stub，但完整安全链还存在未审目标，且统计对应不同内核版本时不能混用。
- 当前 Image4 candidate 的实际内核文件哈希是 `1817068d295e5112749c171fe53e7b1fe217dc8664218d4a4e8d980a41efa24f`，95,125,504 B。它通过生产 DarwinBoot parser；strict 最小 Image4/corecrypto closure 的主机预链接报告为 0 missing / 0 stub。**主机 parser/prelink 通过不等于 XNU 在该设备运行通过。**

## 5. 当前最新的 LZMA payload 候选

这是本交接时应继续追踪的最新 host candidate，不是此前闪过的镜像。

| 项目 | 值 |
| --- | --- |
| 输出目录 | `tb375fc-port/image4-lzma-step/` |
| boot 文件 | `boot-image4-test.img`，19,607,552 B |
| SHA256 | `f5b8b6a9a36afa19f31e95166d9e6b008641bdbd8a3f2b6be9b18454368d2c41` |
| Android header | v4，4096-byte pages，ramdisk=0 |
| boot_a 历史容量 | 64 MiB；新测试前需 fastboot 重新核实 |
| FD | 32 MiB，base `0x40000000` |
| FD SHA256 | `28117da8525218c03d964dd7a969622a11d70093ffbf95e337d3dc4889f39405` |
| payload FAT | 128 MiB 原始卷 SHA256 `675f3100ac4eb54723ba4435d2a4c49ea2429d17a3b0ae2f7ed64930a8e847f6` |
| LZMA | 18,570,799 B；build input 在 WSL `.../Mu-Silicium/Resources/FirstXnu/payload.lzma`，SHA256 `c6433796ec84ab26d9972ecbc225b9373db0e9fe073f4a3896a3eed9aa6941ec` |
| FVMAIN | 21,647,360 B；FVMAIN_COMPACT 占用 19,501,432 / 33,554,432 B |
| XNU kernel | 95,125,504 B，SHA256 `1817068d295e5112749c171fe53e7b1fe217dc8664218d4a4e8d980a41efa24f` |
| ADT | SHA256 `88c473ea60474e44f71cc554d02e014c75f82db80b4fdac778abd09d2dda0d5a` |
| 设备状态 | `device_written=false`，`hardware_tested=false` |

方案细节：`UniXNURamDiskDxe` 确实会通过数值 GUID `4238B7F0-0271-4B91-A5A1-827375FC0027` 从 FV RAW section 取负载；先前只 grep 文本 GUID，漏掉了 C 里的 GUID 数值结构，因而旧 handoff 把该文件错误说成“无消费者”。现在在隔离副本中恢复这个 consumer：FDF 放压缩 LZMA RAW；DXE 读取后要求输入长度合理、展开大小精确为 128 MiB、scratch 不超过 32 MiB，解压后验证 FAT 卷标/扇区边界，再发布只读 Block I/O。

- EDK2 自带 LZMA 解压器用于 DXE；构建日志 `/root/work/tb375fc-image4-test27/build-lzma.log`，构建 rc=0。
- 主机压缩/解压往返哈希一致，package audit 验证 FV 内 LZMA 数据、展开 FAT、FV compression 层、生产 DarwinBoot kernel parser、kernel window 范围、Android v4 头和零 ramdisk。
- 内核映射窗口需求 95,141,888 B，固件配置分配预算 268,435,456 B；这是主机预算/解析结果，运行时连续可用内存尚未在设备确认。
- 32 MiB FD 中保留的私有 PrePi `FwVol.c` 大 FFS 头修复被编入构建；该候选中 RAW FFS 大于普通 24-bit FFS 尺寸，需依赖扩展头修复。当前 host build 和 packaging 通过，但这条 SEC→DXE 路径尚未实机运行。
- 可查看 `image4-lzma-step/README.md`、`image4-lzma-step/qualification.json`。隔离开发树是 WSL `/root/work/tb375fc-image4-test27`；共享 edk2 `/root/work/edk2-202511` 未改。

## 6. 刷写和回退边界

- 最后有明确记录的 boot_a 写入为 `image4-first-start/nopayload-ab-test.img`（2026-11-08 11:28 UTC）：序列号 HA227LJ3、product P98300DA2、slot a；写入成功后用户确认进入 UniXNU 菜单。
- 之后的 LZMA candidate **没有刷写**。因设备后来没电，今天的实际 slot/boot_a 内容无法通过 USB 核实；不要把最后日志当成当前 live 状态。
- 原厂 boot 回退文件：`rollback/boot_a.stock.img`，记录 SHA256 `1ad2cb450587a101ec6fcb7e0db7cf0b3f9b9ff978cf972a7b52a909cb815357`。旧 UniXNU 菜单/测试回退：`image4-first-start/rollback-unixnu-previous.img`，SHA256 `b4f55025b747568d6b535b93069617e7653f0f14cd23284df18a02b7d50793fd`，**不是原厂 Android**。
- 这次整理文档没有执行设备操作。新 LZMA candidate 与上一次不同；后续任何新 `boot_a` 写入应先确认设备序列号/product/slot/boot_a size，再按用户当时明确授权的范围执行。不要擅自写 `system`、`userdata` 或重新分区，不要自动重启到原系统。

## 7. 当前尚未达到的目标

| 子目标 | 现状 |
| --- | --- |
| 实机启动 UEFI/UniXNU 菜单 | 已通过至少一版无 payload 镜像；LZMA 新版还未测 |
| 新版固件 DXE 解压并发现 XNU 配置 | 主机通过，实机未测 |
| DarwinBoot 接受 kernel/ADT/trust cache | 生产 parser 主机通过；新镜像实机未测 |
| Image4 `img4if`/AMFI TrustCache 运行时接口正常 | 曾经失败；当前 strict closure 未实机验证 |
| XNU 完成启动到 BSD/root | 未实现/未验证 |
| MT6897 UFS 驱动读真盘、挂载根卷 | 未完成；当前本地无已验证可用的 UFS implementation |
| iPadOS 27 System/Data 镜像 | 未制作；此前计划需要 Mac 做 APFS 系统卷镜像，但还没进入该阶段 |
| `launchd` / SpringBoard / iPadOS 界面 | 未达到 |
| 触控、GPU/Metal、Wi-Fi、蓝牙、音频、电源管理 | 未完整适配 |
| 全核/EL0 PAC 和上游 SoC 资格 | 部分未验证/未接纳 |

## 8. 接下来按顺序执行

### 阶段 A：平板重新可用后，先验证新固件 payload 路径

1. 给平板充电，进 bootloader；先只读确认 `fastboot devices`、serial、product、current-slot 和 `partition-size:boot_a`。电脑当前 adb/fastboot 列表为空。
2. 把设备当前状态和 candidate SHA256 记录进新的 flash-result，确认工具不会选错设备。
3. **先得到对这版 LZMA 镜像写 boot_a 的明确授权。** 此镜像与最后闪过的无 payload 镜像不同。只验证：SEC 是否通过、菜单是否出现、DXE 是否展开卷、菜单是否能显示正确 payload/config。此阶段不要点 XNU。
4. 若进入菜单但无 payload 项，先通过 QR/Advanced Logger 取日志，定位 `ReadSection`、LZMA Allocate/Decompress、FAT validation 和 Block I/O 发布阶段。若回到 SEC assert，检查大 FFS header 修复实际是否进入 FD 与 FVMAIN 大小。

### 阶段 B：payload discovery 通过后再测试 Image4/XNU

5. 保持已验证的固件和菜单改动不变，只选 XNU 项。记录 DarwinBoot 最后阶段、内核入口、异常/PANIC；有 UART/USB/屏幕日志就抓完整日志。
6. 若仍报 `img4if == NULL`，检查 native kernel 与 AppleImage4/AMFI/CoreTrust/TrustCache 的注册顺序、依赖闭包和 ABI 绑定。不要把普通 corecrypto/AES 符号存在当成运行时 interface 已注册。
7. 只有 XNU 到达 BSD 后，再处理 MT6897 UFS：拿同板朋友的驱动源码、精确提交、DT 参数、时钟/复位/PHY/电源/IRQ/DMA/IOMMU 和完整日志；朋友当前截图进度只到 READ(10) 成功、IOKit 存储服务注册仍有问题、根卷未挂载。不能直接把 26.5 的可执行文件当 27 的驱动或镜像。

### 阶段 C：证明目标 OS 的 arm64e 支持条件

8. 继续已有 PAC 结果：补测每个启用 CPU 核、EL0 PAC 和 EL1 key registers，逐项记录；验证 4 KiB translation 行为。
9. 完成 MT6897 SoC 共用仓库、设备配置、组件依赖和补丁审查，并争取进入 UniXNU 的支持范围。只在官方条件具备时将设备配置切换到官方 arm64e/iPadOS 27 profile。
10. 重新按 iPadOS 27 的真实 kernelcache/IPSW 和 OS ABI 构建 kernel、prelink/components、package、firmware、payload 和 userdata。当前 95 MB kernel test FAT 与 Image4 boot candidate 不是 iPadOS 安装镜像。

### 阶段 D：根卷、用户态、可见界面

11. 按官方 bring-up 顺序通过 UFS 控制器/链路、LUN、真实读盘、IOKit provider、OS/SystemOS 分区、APFS root mount。
12. 在 root volume 可挂载后，制作匹配的 iPadOS 27 System 卷和 Data 卷，适配必要的启动服务、NVRAM、launchd 和设备服务。Mac 仅在官方工具链/镜像制作确实需要时介入；MacBook 12 2017 的能力和耗时要在那一阶段实际评估，当前固件 payload 阶段不需要 Mac。
13. 验证 launchd 与界面服务启动，再逐项做触控、GPU/Metal、显示亮度、Wi-Fi、蓝牙、音频、USB、电源管理和恢复验证。只有此处看到实际 iPadOS 27 UI 并有版本/启动日志证据，才算达成用户目标。

## 9. 官方流程与资料

本项目应按 UniXNU 的设备移植和 bring-up 顺序，而不是把朋友的中途进度当作可直接移植的现成成果：

- [移植设备步骤](https://unixnu.com/zh/porting/steps)：收集设备输入、SoC/设备仓库、解析配置、准备硬件与固件、按依赖适配驱动、构建并启动。
- [arm64e OS 配置](https://unixnu.com/zh/porting/arm64e)：iPadOS 27 只有 arm64e；PAC、SoC 支持和 4 KiB 页要求；profile 和构建流程。
- [SoC 支持](https://unixnu.com/zh/porting/soc-support)：共享 SoC 仓库、DTSI、配置片段、平台操作与固定源码版本。
- [bring-up 顺序](https://unixnu.com/zh/porting/bring-up)：UEFI → UniXNU 菜单 → payload → DarwinBoot → XNU → storage/root → userspace。
- [迁移设备仓库](https://unixnu.com/zh/porting/migration)：迁移和仓库记录规范；它不等于设备已被支持。

官网在本次查阅时列出的 arm64e 设备为 sheng/SM8550，状态开发中；MT6897/TB375FC 不在该列表。官网要求任一启用 CPU 支持地址和通用 PAC、EL1 可写五组 PAC key、EL0/EL1 PAC 指令不 trap、SoC 已支持、4 KiB 一级翻译。BSP EL1 测试通过只覆盖其中一部分。

## 10. 工作路径和常用证据

Windows 工作根：`C:\Users\李昭杭\Documents\修包`  
设备项目：`C:\Users\李昭杭\Documents\修包\tb375fc-port`  
UniXNU checkout：`C:\Users\李昭杭\Documents\修包\UniXNU`  
WSL 固件隔离构建：`/root/work/tb375fc-image4-test27`  
Mu-Silicium 隔离副本：`/root/work/tb375fc-image4-test27/Mu-Silicium`  
EDK2：`/root/work/edk2-202511`（不要误用旧 `/root/work/edk2`）  
当前 LZMA 驱动源码副本：`/root/work/tb375fc-image4-test27/unixnu/package/UniXNUPkg/Driver/UniXNURamDiskDxe/UniXNURamDiskDxe.c`  
当前固件 build log：`/root/work/tb375fc-image4-test27/build-lzma.log`

主机重建时显式指定隔离树和平台路径（先不要清理 Build 目录）：

```bash
wsl -d Ubuntu-24.04 -u root -- bash -lc '
MU_ROOT=/root/work/tb375fc-image4-test27/Mu-Silicium \
EDK2_ROOT=/root/work/edk2-202511 \
UNIXNU_ROOT=/root/work/tb375fc-image4-test27/unixnu \
TREE="/mnt/c/Users/李昭杭/Documents/修包/tb375fc-port" \
WIN_ROOT="/mnt/c/Users/李昭杭/Documents/修包" \
bash /root/work/tb375fc-image4-test27/build.sh
'
```

## 11. 重要的旧记录纠正

- 旧 `HANDOFF-2026-11-08-SEC-FIXED.md` 一度断言 FDF payload FREEFORM 无消费者；这是错误的，因为代码把 GUID 写成数值结构，简单 `grep GUID` 漏掉了。已在 `/root/work/tb375fc-image4-test27/unixnu/package/UniXNUPkg/Driver/UniXNURamDiskDxe/UniXNURamDiskDxe.c` 找到 `PublishEmbeddedPayload()`，当前 LZMA 路线就是按这个 consumer 做的。
- “删除 payload 以后菜单恢复”是已实测事实；“把 payload 压缩放回 FV 后仍可过 SEC 并成功由 DXE 发布”目前只有主机构建与打包校验，不能把它当作实机事实。
- `prepi-fix` 是否仍是有效根因要按当前 LZMA FFS 的真实大头格式核对：上一版无 payload FV 中没有 large FFS，但本 LZMA payload RAW FFS 重新大于 16 MiB，因此扩展 FFS 解析修复再次成为实际依赖。
- 老 handoff 中“未构建可启动 FD/菜单”的描述已过时；本项目已经多次构建固件，且至少一个版本实机进入菜单。老文件保留作历史，不应覆盖本交接的当前状态。
- 不要把 iPadOS 26.5 UFS 截图、iPadOS 27 arm64e native kernel build、strict prelink host check、或 menu-only flash 分别说成完整 iPadOS 27 支持。

## 12. 一句话交接

**现在最靠前的可执行工作是：平板充电并进 bootloader后，经授权测试 `image4-lzma-step/boot-image4-test.img` 的 SEC→UniXNU 菜单→DXE FAT payload discovery；过了才运行 XNU，届时针对 `img4if`/trust cache 继续适配。PAC 的 BSP EL1 已通过，但全核/EL0 与 MT6897 upstream arm64e 资格仍需完成；UFS、根卷、Data、用户态和 iPadOS 27 界面都还没有。**


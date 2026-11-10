# TB375FC → UniXNU / iPadOS 移植总计划

最后更新：2026-11-06（Asia/Singapore）

## 2026-11-08 接手修正（优先于上文旧阶段状态）

用户反馈路线可能有误后，复核 `HANDOFF-2026-11-08-SEC-FIXED.md`、当前仓库和官网启动文档；随后用户指示回到 Image4 阶段并逐步测试。当前事实：

- SEC 阶段问题已通过从 FDF 移除无人消费的 128 MiB payload FREEFORM 文件而解决；实机已显示 UniXNU 菜单。
- 最新执行镜像 `nopayload-ab-test.img` 的 Android boot v4 `ramdisk_size=0`，证明菜单启动，不证明 FAT payload 或 XNU 负载可读。
- 官网规定大 payload 通常经 GKI `vendor_boot` 的 `vendor_ramdisk` 到 FDT initrd，boot 保持无 ramdisk。历史 `init_boot_a` 测试仅 2 MiB FAT，并只验证配置标题发现；95,125,504-byte 内核不能放入该卷。
- `live-fastboot-backup/vendor_boot_a.img` 是 0 字节，但 D 盘仍有用户提供的 ZUI 17.0.04.016 固件包，其 64 MiB vendor_boot 可离线分析。设备当前运行的是较新的 ZUXOS Android 16；旧包不能直接视为当前槽位的安全基底。设备 RAM 证据显示当前 ramdisk 地址附近 0x70000000–0x74ffffff 为 reserved，禁止按简单线性扩容估算安全范围。

**当前路线：**用户要求回到 Image4 阶段逐步修、逐步测。SEC 菜单已恢复。隔离构建副本现已把 128 MiB FAT 以 18,570,799 B LZMA-Alone RAW 放进 FDF，由 `UniXNURamDiskDxe` 在 DXE 解压并发布；32 MiB FD 构建和主机 payload/FV/boot 校验通过。具体候选位于 `tb375fc-port/image4-lzma-step/boot-image4-test.img`，尚未写设备。下一步测试边界是 SEC→菜单→DXE 解压→payload/config discovery；通过后再测 Image4→XNU。当前没有证据表明失败归因于 AES。

| 接手阶段 | 状态 |
| --- | --- |
| 核实 SEC 根因与菜单结果 | complete |
| 对照官网 payload/vendor_boot 规范 | complete |
| 核实 SEC 根因与菜单结果 | complete |
| 确认现有 DXE 是否消费 FDF payload | complete；找到 `PublishEmbeddedPayload` |
| LZMA FFS 包装、DXE 解压、主机构建和 payload/FV/boot 审计 | complete |
| 实机验证 payload discovery | pending（候选已准备，未刷写） |
| 实机验证 Image4 entry | pending；payload discovery 通过后再做 |

## 目标与判据

设备：Lenovo 小新 Pad Pro 12.7 (2025) TB375FC / yili，MT6897 (Dimensity 8300)，当前 Android 16 桌面，adb 在线，fastboot unlocked（见 HANDOFF）。

用户目标："为平板移植 iPadOS 27"。

上游事实（已核对 `UniXNU/docs/zh/porting/arm64e.md`）：**iPadOS 27 只有 arm64e 配置，且 arm64e 仅适用于受支持设备**（要求原生 PAC CPU、SoC 仓库已在官方支持列表、4 KiB 一级页表）。当前唯一 arm64e 设备是 sheng(SM8550)。MT6897 不在列表，也没有 MTK SoC 上游支持。因此本计划的可达路径是：

- **里程碑 A（本阶段目标）**：TB375FC 专用 Mu-Silicium UEFI + UniXNU 菜单在实机亮起。
- **里程碑 B（后续）**：XNU + DarwinBoot 交接（arm64 profile `ipados-17.7.11-21H461`，这是 iPadOS 17 不是 27）。
- **iPadOS 27**：在 MT6897 上当前不可达，需先有上游 MT6897 SoC 支持 + PAC/arm64e 资格；不要让任何人把 bootstrap profile 说成 iPadOS 27 支持。

## 阶段划分（上游 steps.md 八步）

| # | 阶段 | 归属 | 状态 |
| --- | --- | --- | --- |
| 1 | 收集设备输入（SoC/CPU/GIC/内存/存储/面板/触控/固件） | soc-mt6897 + lead | **complete** |
| 2 | 选择/建立共享支持：`UniXNU-mt6897-common` | soc-mt6897 | **complete（已复核）** |
| 3 | 创建设备仓库 `tb375fc-port/device`（格式 2） | lead | **complete** |
| 4 | `unixnu defconfig/status/tree` 解析通过 | lead | **defconfig + status 通过**；tree/fetch/build 需 WSL |
| 5 | UEFI 平台包 `uefi-tb375fc` + DSC/FDF/Configs + 构建 | uefi-platform + build-toolchain | **complete（FD 8 MiB + boot.img 618 KB）** |
| 6 | 驱动适配（本阶段只需 GOP 帧缓冲 + 输入 + RamDisk/FAT） | uefi-platform | **complete（已编入 FV）** |
| 7 | 构建 v4/零 ramdisk boot.img，`fastboot boot` 进菜单 | lead | **boot.img 已构建并校验；实机启动待用户授权** |
| 8 | XNU / DarwinBoot / 根卷 / 用户态 | 后续 | out of scope（本阶段） |

## 关键决策

- **启动镜像格式**：保持 stock `boot.img` 的 Android boot **header v4、page 4096、ramdisk_size=0**；UEFI FD 作为 gzip kernel 字段（参考 tb365fc 的 Plan A，同厂商 LK 路径）。
- **OS profile**：bootstrap 阶段用 `ipados-17.7.11-21H461`，并在 porting.toml/README 明确写"仅 bootstrap，不是兼容结论"。
- **不刷分区**：整条链路只用 `fastboot boot` RAM 启动；任何 flash/unlock/erase 需用户逐次授权。
- **工具链**：Windows 侧无 EDK2 工具链；WSL2 Ubuntu-24.04 有 **root 权限**、28 核、954G 空闲、GitHub 可达 → 所有 Mu-Silicium/EDK2 构建在 WSL 内完成。仓库文件仍留在 Windows 侧（`/mnt/c/...` 访问）。
- **不改共享上游**：`../UniXNU` 保持 commit `7307c26e`，只读使用；所有本地改动进 `tb375fc-port/` 或新建 `UniXNU-mt6897-common`。

## 错误记录

| 错误 | 尝试 | 处理 |
| --- | --- | --- |
| 交接说 `toolchain-cache` LLVM MSI 未装 | - | 改用 WSL 内 apt/upstream 工具链，不跑 MSI |
| `unixnu` CLI 警告仅 macOS/Linux 测试过 | - | Windows 下用法仍可用（仅解析/status），真正的 build 走 WSL |
| WSL 默认 sudo 需密码 | 1 | 改用 `wsl -u root`，成功 |

## 进度日志（Lead）

### 2026-11-06
- 读 HANDOFF.md、UniXNU 八步移植文档、arm64e 文档、bring-up 文档。
- 确认 WSL2 Ubuntu-24.04 有 root 免密、28 核、954G 空闲、GitHub 可达 → 作为构建主机。
- **实机取证（root adb，只读）**：导出 /sys/firmware/fdt（613,734 B）并用 dtc 反编译成 17,210 行 DTS；
  采集 /proc/cmdline、/proc/iomem、/proc/partitions、/dev/block/by-name、MIDR、cpufreq。
- **关键结论**：TB375FC 的 LK 交接（0x40000000 / 0x66f00000 / 0x47c80000，bootopt=64S3,32N2,64N2）
  与 UniXNU 已支持的 Lenovo TB365FC 完全一致 → FD base 0x40000000，走 Plan A（FD 作 gzip kernel）。
- **DRAM**：0x40000000 起 8 GiB；低区 0x40000000–0x47c80000 共 124.5 MiB 无保留区可用。
- 设备仓库 device/ 完成：board.toml、porting.toml(14 区通过 checker)、dts、defconfig、feeds.toml、
  local.toml、proprietary-files.txt、README、scripts/package.sh、evidence/（原始取证）。
- `unixnu defconfig` 与 `unixnu status --json` 通过，四层配置（arch/soc/os/device）正确解析。
- `unixnu tree` 在 Windows 上失败：WinError 1314 无符号链接特权 → 必须在 WSL 内执行 fetch/tree/build。

## 错误记录补充

| 错误 | 尝试 | 处理 |
| --- | --- | --- |
| `unixnu tree` OSError WinError 1314 symlink | 1 | Windows 无 symlink 权限；改用 WSL 内构建 |
| `porting-record.py` gbk 解码错误 | 1 | 设 `PYTHONUTF8=1` 后通过 |
| vendor_boot 用 boot v0 struct 解析得到错误字段 | 1 | 改用 vendor_boot v3/v4 专用 struct，得到 0x40000000 |
| device/README.md 首次 write 被拒绝（未先读） | 1 | 先 read 再 write |

### 2026-11-06 续
- **task-1（SoC 层）完成并复核**：`UniXNU-mt6897-common` 全结构落地；`tools/check-dtsi.sh` 通过
  （dtc 零 warning + 8 项对 live DT 的断言）。产出 memory-map.md / hardware-inventory.md。
- 设备仓库 `dts/tb375fc.dts` 通过 dtc `/include/ "mt6897.dtsi"` 正确合入 SoC 层（2,388-byte DTB），
  GIC / psci / timer / 8 CPU / memory / initrd / panel 全部出现在生成的 DTB 里。
- 关键发现：UniXNU 用裸 dtc 无 cpp，SoC 必须用 dtc 的 `/include/` 而非 `#include`。
- 关键发现：`./unixnu build firmware` 直接调 EDK2 `build`，不走 Mu-Silicium 的 build_uefi.py；
  <uefi> 目录同时是 WORKSPACE 和 PACKAGES_PATH[0]，所以 uefi-tb375fc 必须是一棵 Mu-Silicium checkout 的形状。
- porting.toml 在更新后仍通过 checker（14 区）。
- 待办：uefi-platform 交付 tb375fc.dsc/fdf/APRIORI/DXE → build-toolchain 跑首次编译。

### 2026-11-06 续 2（构建迭代中）
- **UEFI 平台包全部交付**：tb375fc.dsc / tb375fc.fdf / APRIORI.inc / DXE.inc / tb375fc.dec /
  MemoryMapLib / KeypadDeviceLib / Silicon/MediaTek/MT6897Pkg(13 文件) / BootShim.S / Configs / DTBs / mkbootimg.py。
- **Lead 独立验证**：上游 BootShim 必然过不了 bootimg.py 的 header 检查（差 112 字节），
  定制 shim 336 字节完全合规（_Payload@0x150、ARM\x64、0x40000000、len(shim)+FD_SIZE 全对）。定制是正确的。
- **edk2 版本决策**：改用 mu_basecore release/202511 @ 7e2a44a7（不是 feeds.toml 的 202608），
  因为 Mu-Silicium @964a886f 的 SiliciumPkg.dsc.inc 写死 ArmPkg/Library/ArmLib/ArmBaseLib.inf。
  UniXNU 官方支持 `--edk2 DIR` / `UNIXNU_EDK2`，所以走的是官方通道。
- **FD size 4→8 MiB**（0x00800000，FD_BLOCKS 2048），连带 stack 0x40800000 / DXE Heap 0x40840000。
- **ENABLE_ADVANCED_LOG = 1**（UART 不可用，黑屏即盲飞）。
- **Lead 从源头剥掉 8 个 UTF-8 BOM 文件**（EDK II MetaFileParser 零容忍 BOM，会 KeyError BASE_NAME）。
  同时确认 0 个 CRLF 文件。
- **Lead 铺好 edk2-202511 的 4 个子模块**：mipisyst(91 files, library/include ✅)、libspdm(1673, 202511 gitlink)、
  brotli(167)、libfdt(两层 libfdt/libfdt ✅)。
- 编译已连过 5 关：FDF packages path、ArmBaseLib 版本、Common/Mu 空子模块、PACKAGES_PATH typo、BOM。

**构建环境最终形态**：
```
/root/work/Mu-Silicium         Mu-Silicium @964a886f + Common/Mu(mu_plus b39b6499)
/root/work/edk2-202511         mu_basecore @ 7e2a44a7 + libfdt/mipisyst/brotli/libspdm
/root/work/submods             tarball 缓存
WSL 工具链                     clang 18.1.3 / LLD 18.1.3 / nasm 2.16.01 / dtc 1.7.0 / iasl / mtools / aarch64-linux-gnu-gcc
```
## 2026-11-07 接续
- 当前目标：修复可见菜单；先检查实际构建源码、备份和实时设备状态。
- ADB: TB375FC, slot _a, green, boot_completed=1, root。
- 不沿用旧报告的‘iPadOS 27 不可达’定论：当前属未支持/未验证，需 PAC、EL1 和平台资格实测。
- 新发现：旧交接对菜单模式/显存写入有相互冲突推测，均待源码验证。
- 检查路径错误：Main.c 位于 Library/UniXNULib，不在 Application/UniXNU。

- Alpha 实验主机测试：clang 缺少 sanitizer runtime，改用 GCC sanitizer 验证。

### 2026-11-07 单变量 alpha 候选已完成
- 放弃叠加 BDS 时序/底部日志带方案，按最新失败交接只验证新发现的 alpha 差异。
- 独立 GOP alpha 实验主机测试、固件编译、boot 打包与解压匹配全部通过。
- 产物：tb375fc-port/menu-alpha-test/boot-alpha.img，SHA256 50dda8666d63f87a8d812ed3410f059bfc44efd07f4198fc5f648ad7d457626a。
- 菜单目标尚未完成：实机测试需 boot_a 写入，本次未刷写，待用户明确确认。设备未重启。

2026-11-07 里程碑：菜单稳定可见、音量减导航实机通过。当前有效构建为 /root/work/tb375fc-voldown-fix，Windows 产物 tb375fc-port/menu-voldown-fix。确认手势待测；音量加、电源尚未完成。

2026-11-07：里程碑 A 已完成——菜单可见、音量减短按导航、约1秒长按松开确认进入 Tools 均由用户实机确认。后续为 payload/DarwinBoot/XNU；iPadOS尚未启动。

## iPadOS27 阶段（用户2026-11-07明确要求开始）
1. 只读 BSP 资格调查：CurrentEL/MPIDR/ISAR1/ISAR2/MMFR0/当前EL的TCR，编译中。
2. 根据读数设计带异常处理的 EL1 五组密钥/PACIA-AUTIA 与每核检查，不能以BSP特性宣称全核合格。
3. 4KiB 一级翻译实测；CPU声明支持不等于XNU路径已通过。
4. 日志导出与FAT payload发现，然后DarwinBoot/XNU入口。
有效回退基线 menu-voldown-fix；本阶段不写userdata、不改分区。

2026-11-07：BSP EL1主动PAC资格通过（五组密钥访问、临时启用后的签名/认证、SCTLR恢复、异常handler清理）。EL0与其他核心仍待验证。固件未集成ArmPsciMpServicesDxe，当前未找到ArmMpCoreInfo HOB构造；不能直接假设MP Services可用。后续应实现基于实测拓扑的多核测试链，或明确限定单启动核的早期XNU验证范围。

Payload诊断编译首轮缺PiHob类型声明，已补PiDxe.h后重编。Apple参考LZFSE源码按上游固定提交构建完成；使用官方kernelcache函数对已缓存27组件解码和pin校验。

2026-11-07 payload卷与外部配置加载实机通过，里程碑配置发现完成。有效基线initboot-payload两镜像；下一步准备Linux内核工具链、解析27配置并构建TB375FC XNU/ADT，随后DarwinBoot。不将菜单FAT成功说成iPadOS启动成功。

## 2026-11-07 内核构建执行
- 主机依赖check已通过；WSL 7.6GiB RAM/2GiB swap，编译jobs=2。
- 官方27 defconfig/status解析通过：arm64e、XNU12377.121.6、68项补丁。
- codeload LLVM大包传输无进度，停止该下载，WSL git远端访问实测可用，改用上游原生git固定提交。
- 官方Linux工具链安装已启动，保留构建目录便于失败后续编：/root/work/tb375fc-kernel-toolchain-build；prefix /root/work/tb375fc-kernel-toolchain。
- 当前平板不变，尚无可刷XNU内核。

2026-11-07 LLVM所需源码下载完成并固定提交核验通过；CMake配置通过，工具链实际编译中（3136项，jobs2）。后续依次完成工具链其余组件、27 XNU源码获取/补丁/编译、实测ADT/DarwinBoot。设备现有两镜像保持不变。

2026-11-07 里程碑：iPadOS27配置的原生arm64e XNU已编译/链接完成并导出结构校验报告。kernel27-native/kernel.macho及日志、符号、hash已保存。后续为首启ADT/负载容量（内核22.25MB大于8MiB init_boot分区），需新的安全承载方案及加载范围审核，再准备镜像和回退；未刷写。
2026-11-07 首启调试：首版与panic版均只观察到绿色交接前标记后花屏。下一步按官方green-bar流程用开启MMU后6秒PSCI重启checkpoint隔离；固定13MHz来自live DT，与SMC PSCI一致。内核原产物、旧测试镜像、菜单及按键逻辑保留。核验和打包完成再延续用户测试授权仅刷boot_a，实机结果需单独记录。
MMU后定时重启候选已完成构建、代码与负载校验，并仅boot_a刷入及重启成功。剩余：用户选择XNU后的实机计时结果；根据结果隔离入口前/后与显示路径。内核完整启动、存储和根卷阶段尚未完成。
交接前移候选已完成：A缓存维护后关闭MMU前，B关闭MMU后跳内核前，C内核首入口。三项同一菜单、共用原ADT/入口重启内核副本，无MMU关闭后显存绘图。编译/机器码入FV/负载校验及仅boot_a刷入重启完成。当前必须获得A项实机结果才能缩小故障阶段；尚未确认任何新检查点。
三检查点A已观察到预期重启（停绿条后重启，未实测秒数）。下一步：B After MMU off；B若同样重启再测C Kernel entry。无需重复刷写。
A能重启、B花屏无重启已记录。基于owned-window清理不覆盖loader自身代码、EDK2执行同步仅PoU，完成独立self-code PoC clean+identity guard候选并通过编译/机器码最终FV/完全相同负载检查，已仅boot_a刷入及重启。当前等待复测B，B通过再C；XNU真正启动、USB日志、根卷阶段仍未完成。
里程碑：code-clean候选B/C均实机自动重启、无花屏，DarwinBoot关MMU到kernel首入口通过。下一步保留已验证loader代码清理/identity guard，恢复无定时重启的XNU bootstrap内核（入口蓝/紫及panic屏），复用同ADT，制作可回退镜像再测试。
恢复原启动版已完成编译、最终镜像核验、仅boot_a刷入及重启。当前最后已证实里程碑仍是上版B/C（关MMU至首内核入口），新版本原XNU bootstrap结果待用户画面反馈。无需Mac；不做根卷/存储写入。
2026-11-08里程碑：已实际进入XNU MMU开启后的IOKit初始化，并获取可读data abort。当前针对AAPL,phandle缺失修正ADT，构建镜像中；完成最终校验后仅boot_a复测。完整内核、存储/根卷及用户态尚未启动。
ADT AAPL,phandle修正已完成源码/回归/双语文档、精确属性差异、最终镜像校验，已仅boot_a刷入并重启。当前待用户重选TB375FC XNU27 boot反馈；上版实机最后到达IOKit初始化。
新里程碑：ADT phandle修正版已越过原IODT崩溃，实际到达trust_cache_runtime_init。下一阶段原始AppleImage4/AMFI等27组件预链接、RELEASE-LTO参考与ABI映射校验，尚未执行。当前保持不刷写，等待用户后续安排；原Android未恢复。
用户最新明确继续修内核，撤销恢复/重启指令。当前只做主机编译。RELEASE-LTO reference已完成源码编译，正在补齐LTO-enabled ld64并保留对象恢复链接。随后预链接原始安全组件并审核映射报告；本轮不刷机。

2026-11-08 本轮完成：定位原生MH_EXECUTE缺LC_SEGMENT_SPLIT_INFO，原因现有standalone模式XNU_KERNEL_CHAINED_FIXUPS=0。经官方cli.prepare/patches.prepare恢复目标refer链接后，复用对象、以XNU_KERNEL_CHAINED_FIXUPS=1完成原生重链接。输出prelink27/native-fileset/kernel.macho，23,693,928 bytes，SHA256 1c6e73c93f4a4f0e92315506d00dadeeab7cefdca8e82baf03d927a5a86339d1。arm64e/iOS27/MH_EXECUTE、可执行入口0xfffffff00753a488、433137条有效split-segment记录、chained fixups、无未定义符号。原kernel27-native和所有实机候选未覆盖；WSL原始mach备份于/root/work/tb375fc-prelink27/native-threaded-backup。脚本tools/build-native-fileset27.py可复现该重链接，日志qualification在输出目录。
严格Image4+corecrypto闭包重试已完成，11项依赖包含CoreTrust和IOCryptoAcceleratorFamily；56处未解析引用/39个唯一原始地址，无stub，工具正确拒绝生成candidate。summary、完整report、map、log、unresolved-targets.json已保存prelink27/image4。其余阶段不再是编译错误，而是原始安全组件的ABI配对；不能仅根据相邻地址名称猜测绑定（部分reference-order邻近命名明显跨到recount等其他函数族）。AMFI栈布局接口仍未修改或验证。XnuPrelink63测试再次通过，既保留未选组件过滤，也验证入选missing报错和正确site绑定。全部主机进程已结束；本轮未新增设备操作。下一步对39目标逐个以原始机器码、调用者、PAC/vtable及源接口证明配对，之后重新严格预链接；完整iPadOS启动仍未完成。

2026-11-08 本轮结果：将实时构建日志移入 WSL，解决 Windows 日志写入造成的 D-state 等待，增量构建完成。新内核 23,694,112 bytes，SHA256 91ce34e4a1559ec631ebd787cd4582c8c5a11f258d454ce9cb204721b37d99c8，arm64e subtype c0000002，入口 fffffff00753a488，433143 split references，无 undefined，AMFI stack shift 已导出。Image4/corecrypto 严格依赖链零 missing/零 stub，实际 DarwinBoot parser 通过；负载 95,125,504 bytes，SHA256 198a0fa8d2476b507312c67b13f03b0c16114ce49a94ca5d3ed4c8ff9b8044ad。完整四安全组件闭包仍有8组件226引用/162唯一目标，严格模式拒绝输出，未伪造绑定。security/target-evidence.json 与 target-disassembly.txt 已导出，zerofill目标单独标记而非当作代码读取。RELEASE-LTO参考为补丁前固定版本，当前仅受控新增 stack.c AMFI 栈补丁，不宣称参考与新内核完全同一补丁集。本轮没有设备命令，未刷写、重启或恢复。下一步逐项审查完整闭包 ABI，不能将主机构建/解析成功等同实机启动成功。

2026-11-08 平台 ABI 绑定推进：新增26个经审查接口，14个原始函数除BTI外与native完整机器码相同；其余按源码、成员偏移、dispatch slot、PAC、参数及返回值验证。共享override PAC必须用完整函数体区分基类/派生类。原始PAC也可能包含多个地址，因此改用该原始目标存在且native目标唯一的断言，不谎称原始集合唯一。四组件严格检查missing由162目标226引用降为136目标197引用；仍拒绝输出完整candidate，未使用allow-missing或stub。63项现有工具测试通过。证据prelink27/security/reviewed-platform-bindings.json，复现tools/review-platform-bindings27.py。README及EN/ZH组件文档更新。设备无操作；当前不需要用户输入。下一步审查剩余136目标（包括数据/虚表与复杂平台方法）。

缓存/批量审查工作流已落地并通过真实输入验证：{"cold_seconds": 66.365, "warm_seconds": 46.985, "strict_reports_identical": true, "missing_targets": 136, "device_written": false, "map_reuse_seconds": 9.783, "map_reuse_report_identical": true, "review_queue_seconds": 4.247}。未重编内核；未执行设备命令。后续按review-queue.json组件批次核对，批次结束统一严格预链接。

2026-11-08 继续适配：平台第二批9方法（完整源码/机器码/PAC/参数/返回流），5个metaClass常量指针+2个gMetaClass直接对象（getter及类名初始化交叉验证），6个类身份已确认且全槽位数和认证布局完全一致的虚表。严格预链接由136目标197引用降为114目标165引用；candidate_written=false，stubbed空。68测试通过。证据security/reviewed-platform-batch2.json、reviewed-metaclass-pointers.json、reviewed-platform-vtables.json。队列已刷新114项，缓存3hit，4.612秒。中断注册original多出ap-wake-interrupts初始化helper并更新全局，native源码缺该逻辑，未盲绑；PMLog/睡眠包装仍待证明。虚表getMetaClass位置按地址点+0x38，即符号基址+0x48核对，首次用符号+0x38读到release后已纠正，最终全部身份断言通过。未重编内核，未执行设备命令。下一批可核对OSMetaClassBase虚表13槽唯一布局及thread_block包装；仍需严格证据，未写入这些待审项。

2026-11-08 新最小首启镜像已完成并获用户明确授权刷入：内核1f8a71624ac210d9c023e320eaa7d083c4bc887b72b72e81cb9c6b0a12995b90，MAC启动钩子实际调用/slot0x1e8/PACe4fe验证通过。元类精确构造参数审查11项无错，并确认此前自动RootParent虚表实为IOService，已纠正。Image4/corecrypto严格最小闭包0missing/0stub、生产parser通过，95,125,504字节，SHA1817068d295e5112749c171fe53e7b1fe217dc8664218d4a4e8d980a41efa24f。独立载体FAT128MiB/FD32MiB，FD40000000..42000000，栈42000000..42040000，堆至47c00000，低于LK DTB47c80000；内核窗口动态从UEFI可用内存分配。保留按键/菜单源码；扩容FAT解析ASan/UBSan通过。EDK2使用上一版edk2-202511修正首次路径错误；大GUID section超过16MiB使用扩展头，打包审核已适配。候选boot19,345,408字节，SHA59841a20bf65e63fdacb242c4efe9f61aadbcf708dbf21de1067083c33ef9fa7，v4零ramdisk，回退为上一版UniXNU而非stock Android。实机只读确认HA227LJ3/P98300DA2/slot a/boot_a64MiB后仅flash boot_a；发送59.193s、写入0.200s均OKAY、exit0；随后fastboot reboot OKAY。等待用户菜单及XNU画面，尚未宣称越过Image4 panic。全安全链92是旧native91ce上的最后统计，不能当作当前新native1f8的最终数量；本轮优先准备最小实机测试。

# TB375FC 移植 — 进度日志

## 2026-11-06 阶段一完成

### 里程碑

**TB375FC 专用的 UniXNU UEFI 固件已从零构建成功，并通过全部离线契约校验。**

| 产物 | 字节数 | SHA-256 |
| --- | --- | --- |
| SILICIUM_UEFI.fd | 8,388,608 (0x800000) | `6101a25292c8abf9dfab4f8bc331a7cb88f704994cf1ba68eb0c8ead01240c44` |
| boot.img | 618,496 | `2a453c080415b3d1a6509583b0331a496130a725c3e9cb3bb0e393ba9164dff6` |
| shim.bin | 336 | — |

- 构建 68 秒 rc=0，178 个 .inf 编入，FV 占用 644,120/8,388,608（7%）
- **两个独立脚本算出完全相同的 boot.img sha256** → 构建确定性成立
- 6 个必需模块全部在 FV 里（UniXNU / UniXNUDxe / UniXNURamDiskDxe / DarwinBoot / DiskIoDxe / Fat）
- DTB 可从源码逐字节重建（`6820FCE210659CF41C9C4148002B7A9D0A463908689C7E1147DB98223409FE29`）

### 尚未完成（关键边界）

**固件从未在设备上启动过。构建成功 ≠ 能启动。**

未验证项：
1. framebuffer 几何（1840×2944 / stride 7360）是假设，可能花屏
2. 面板身份只是 LK LCM 名匹配（`nt36532_dsi_vdo_144hz_tianma_lcm_drv`），非电气验证
3. SimpleFbDxe 能否接管 LK 留在 0xFC16F000 的扫描缓冲区
4. LK 是否接受自建 boot.img 的 `fastboot boot`
5. 菜单出现前无日志通道（UART 被 disable_uart=1 关闭，无排针）

**iPadOS 27 在当前条件下不可达**：iPadOS 27 只有 arm64e 配置，UniXNU 限制 arm64e 仅用于受支持设备
（当前仅 sheng/SM8550）。本移植用 arm64 的 iPadOS 17.7.11 bootstrap 配置，仅确定构建输入，不构成兼容性声明。

### Lead 亲自完成

- 通读 HANDOFF.md + UniXNU 官方八步移植文档
- 环境探测：Windows 无 EDK2 工具链；WSL2 Ubuntu-24.04 root 免密、28 核、954G 空闲
- **实机只读取证（root adb）**：导出 live FDT（613,734 B）→ dtc 反编译 17,210 行 DTS、
  /proc/cmdline、/proc/iomem、/proc/partitions、/dev/block/by-name、逐核 MIDR、cpufreq
- 解析 stock boot/vendor_boot/init_boot 头部 → 拿到 LK 交接地址
- **关键发现**：TB375FC 的 LK 交接（0x40000000 / 0x66f00000 / 0x47c80000 / bootopt=64S3,32N2,64N2）
  与 UniXNU 已支持的 Lenovo TB365FC 完全相同 → 直接沿用 Plan A
- 解码 `atag,videolfb` → framebuffer 0xFC16F000 + 0x3E90000、LCM 名 tianma NT36532
- 设备仓库 device/ 完成（board.toml / porting.toml 14 区通过 checker / dts / defconfig / feeds / README / evidence）
- **独立验证上游 BootShim 必然失败**（差 112 字节）→ 确认定制 shim 的必要性
- **剥离 8 个 UTF-8 BOM 文件**（EDK II MetaParser 零容忍）
- **铺 edk2-202511 的 4 个子模块**（libfdt 两层结构 / mipisyst / libspdm / brotli）
- 独立复核 SoC DTSI（check-dtsi.sh 8 项断言）、DTB 可复现性、FD 模块清单、boot.img 三类契约
- 独立打包并校验 boot.img（与团队产物 sha256 一致）

### 团队分工

| 队友 | 任务 | 结果 |
| --- | --- | --- |
| soc-mt6897 | task-1 UniXNU-mt6897-common + 硬件解析 | ✅ completed，check-dtsi.sh 全 PASS |
| uefi-platform | task-2 uefi-tb375fc 平台包 | ✅ completed，28 个自建文件，构建通过 |
| build-toolchain | task-3 WSL2 工具链 + FD 构建 | 解决 12 个真实错误，出 FD |

### 待用户决策

**首次实机 RAM 启动授权**。脚本已备：`tb375fc-port/tools/first-boot.ps1`
（只做 adb reboot bootloader → fastboot devices/getvar → fastboot boot，不写任何分区）
## 最终状态（Lead 确认）

三个共享任务全部 completed：

| 任务 | 负责人 | 交付 |
| --- | --- | --- |
| task-1 | soc-mt6897 | `UniXNU-mt6897-common/`：soc.toml、dts/mt6897.dtsi（check-dtsi.sh 8 项断言全 PASS）、docs/memory-map.md、docs/hardware-inventory.md |
| task-2 | uefi-platform | `tb375fc-port/uefi-tb375fc/`：28 个自建文件（DSC/FDF/MT6897Pkg/MemoryMapLib/KeypadDeviceLib/BootShim/Configs/DTBs/tools） |
| task-3 | build-toolchain | WSL2 工具链 + Mu-Silicium/EDK2 构建流水线 + `tools/BUILD.md`（15,274 B）+ FD/boot.img |

产出物全部落盘并验证：
- `out-tb375fc/boot.img` 618,496 B
- `out-tb375fc/SILICIUM_UEFI.fd` 8,388,608 B
- `out-tb375fc/shim.bin` 336 B
- `STAGE-RESULT.md` 阶段成果说明
- `tools/first-boot.ps1` 首次启动脚本（待授权执行）

设备 HA227LJ3 仍在 Android 正常状态，**未执行任何 fastboot 写操作**。
移植记录 checker：14 个结构化区通过。

## 2026-11-07
接续已有黑屏排查，尚未重启或写入设备。

已创建 tools/prepare-alpha-test.py 和 menu-alpha-test/ 源码前后快照；独立 WSL 树 /root/work/tb375fc-alpha-test。GCC sanitizer 主机测试通过，固件编译中。未写入/重启设备。

单变量 alpha 构建与验证完成，完整说明/源码快照/测试/日志/参数哈希已保存在 tb375fc-port/menu-alpha-test/。ADB boot_completed=1。下一步仅为获明确写入确认后的实机观察；不能将主机通过写成菜单成功。

2026-11-07：用户明确授权‘刷入’。校验 alpha 镜像 SHA256 通过；fastboot product P98300DA2/current-slot a/unlocked yes。boot_a 写入 OKAY，reboot OKAY。待用户报告屏幕结果；当前 boot_a 为实验镜像，尚未恢复。

2026-11-07 输入后续：菜单可见、按键无响应。新的控制台时序单变量固件编译通过并打包 menu-input-test/boot-input.img。用户需手动回到 fastboot；未执行第二次写入。

输入连接时序版：现场 fastboot 已连接，slot a；boot-input.img 哈希核验通过。flash boot_a OKAY，reboot OKAY，待实机按键结果。

2026-11-07：KPD 只读诊断版编译、镜像解压匹配及哈希校验通过，fastboot boot_a 写入并重启成功，等待 RAW/CHG 实测。

2026-11-07 音量减 release 修复：状态机 sanitizer 测试、固件编译、打包校验均通过；boot_a 写入 OKAY、reboot OKAY，待用户短按与1秒按住松开的实机验证。

2026-11-07：用户确认音量减可用。保留当前实机版本，等待 Tools 确认手势验证，不主动恢复 boot_a。

用户确认‘能进入’Tools：长按松开确认实机通过，菜单目标达成。有效镜像 menu-voldown-fix/boot-voldown.img；不要用旧 build-fd 默认树覆盖此可用版本。

开始 iPadOS27资格调查：准备独立 /root/work/tb375fc-arm64e-survey；只修改菜单诊断文字为BSP只读系统寄存器调查，不写PAC密钥。尚未实机资格通过。

arm64e-survey/boot-survey.img 编译及打包校验完成，SHA256 96a291806c320a1b35875deb87dbde0930043f89c73b898bfd4c0df7fd212fc7。当前ADB/fastboot均未连接，尚未写入，待用户手动进入fastboot。

2026-11-07：用户确认已进入fastboot；BSP硬件检查镜像 boot-survey.img 哈希核对通过，boot_a 写入及重启成功。等待菜单BSP寄存器读数。

BSP寄存器照片已保存结构化结果 arm64e-survey/hardware-result.json。EL1与启动核PAC特性字段、4KiB配置已确认；主动PAC和全核仍待实现/验证。WSL无rg，后续查询使用grep或Windows rg。

准备 PAC 主动检查：独立 tb375fc-pac-active；CpuArch 注册同步异常处理，仅对精确探针PC区间跳至恢复点；已占用则跳过。十个密钥半寄存器只读/同值写回/核对，PACIA改变与AUTIA还原检查。未测试EL0/其他CPU。

PAC主动检查固件编译和打包校验通过；现场fastboot slot a，镜像哈希核验通过。boot_a写入与重启OKAY。等待屏幕 KEY/PAC/TRAPS/ESR/CLEAN 结果。

诊断面板修复候选：恢复成功版轮询、停止重复覆盖两行、GOP增加目标矩形cache clean；新增实际wrapper ASan/UBSan测试通过（精确cache范围/次数、RGB、矩形、读操作与失败路径）。固件编译与打包校验通过；当前fastboot未连接，尚未刷入此候选。

面板/输入候选已在fastboot连接后写入boot_a并重启成功；SHA256 80e59260d5ef3cf679dcf2c3cf3a107b69181717ff5f3b8001ec772e2b57a0e2。待实机两行稳定显示、音量减和ENIA结果。

临时EnIA启用检查版编译及boot打包校验完成。产物 pac-enable/boot-pac-enable.img，尚未写入；当前fastboot未连接。等待用户进入fastboot。

检测到fastboot连接；临时EnIA检查版哈希核验通过，boot_a写入与重启OKAY。等待SIGN/AUTH/RESTORE等实机结果。

用户确认ENIA/SIGN/AUTH/RESTORE均1 TRAPS0 CLEAN=Success。保存pac-enable/hardware-result.json。检查发现多核MP Services驱动尚未集成；当前只完成启动核EL1，不对全核或EL0作通过结论。

推进官方payload阶段：补齐LK x0->SEC参数->入口GUID HOB，配置发现FAT卷2MiB打包完成。尚未加入内核或虚假XNU项；待实机确认外部配置标题。Linux kernel工具链check缺ninja/cpio/tcsh/unifdef/cmake，已启动安装；官方27 IPSW info远程读取仍在等待。

Payload发现版镜像哈希通过，slot a，boot_a写入与重启成功。待菜单标题证明卷配置发现。原始27 KernelCache已提取；其他组件仍在提取，Linux缺失主机工具安装完成。

Payload只读诊断版编译/打包通过；哈希校验通过后boot_a写入与重启成功。等待HOB/X0/FIXEDFDT/FS/INITRD/HEAD/FAT数值。

Payload诊断已收敛：FDT/HOB正确、原厂LZ4 initrd、FS0 FAT0。需转向实际ramdisk来源init_boot；下一步先恢复当前Android并读取/核对init_boot备份，尚未写过init_boot。

当前执行已核验boot_a原备份恢复，未写init_boot。等待Android后只读备份init_boot_a/b。候选驱动新增严格标记FAT切片定位，主机ASan/UBSan通过，编译中。init_boot新写入必须在具体镜像及备份完成后获用户确认。

init_boot_a/b现场备份并哈希核验完成；initboot-payload/boot-payload.img及init_boot_a.payload.img均已准备可审阅。当前Android正常，未写init_boot_a或b。新增init_boot_a写入需用户确认。

2026-11-07 用户明确确认写boot_a及init_boot_a；两镜像哈希预检通过，现场product P98300DA2 slot a unlocked yes。两个分区写入成功并重启。当前Android恢复需要同时恢复boot_a和init_boot_a；b槽未写。等待payload配置发现结果。

用户确认payload标题出现。保存initboot-payload/hardware-result.json。当前boot_a/init_boot_a保持此实验组合，未恢复。

继续内核阶段：WSL依赖check通过，27配置解析68补丁通过。开始上游Linux工具链固定提交构建，jobs2以限制内存。只改变下载/工作目录保留方式；未修改设备分区。

2026-11-07 用户切换网络后重试：发现/root/.gitconfig将GitHub重写至ghfast.top；仅本次进程忽略全局配置，未改用户设置。直连Git已取得LLVM固定提交FETCH_HEAD，checkout仍下载中。Windows直连大包停于603571字节；发现新启用本地7897代理，显式代理下载已超过6MiB，尚未完整，不算编译开始。设备分区未修改。

LLVM主要源码下载成功，HEAD=58c46bae21181fb4240cf964461bea738d96d50e，官方builtin补丁应用成功。CMake首次配置报LLVM libc目录缺失，原因是本地稀疏下载列表漏/libc/；已补目录并改为HEAD命中时保留已应用补丁、跳过重复fetch/force checkout。备用curl600秒总时限退出，留86077749字节部分包，不作为完整输入使用。

2026-11-07 下载阶段达成：LLVM HEAD固定58c46bae21181fb4240cf964461bea738d96d50e，稀疏编译源码含libc齐备，官方builtin补丁已应用；CMake配置/生成通过。上游工具链Ninja jobs2真实编译开始，3136项，已到97项，无编译错误。活动日志/root/work/tb375fc-kernel-toolchain-libc.log，exec session91271；尚非XNU编译或可刷内核。

继续构建：根据实测内存可用6.7GiB，干净中断Ninja后以jobs4续编，保留已有696项；没有源码编译失败，退出2为主动SIGINT。新日志kernel-toolchain-j4.log。独立辅助工具复用已缓存5个上游源码包，host-tools-build工作目录与LLVM分开；日志kernel-host-tools.log。发现上游Linux目标固定17/arm64，独立kernel27-workspace的kernel.py与prepare-xnu.py已改读profile架构/版本；保留linux-profile-target.patch，原checkout未改。XNU固定ac9718...源码fetch-direct运行中。

辅助工具按官方函数构建完成：ld64-956.6（arm64e支持）、MIG、IIG、host-cc、LZFSE；未将其等同完整工具链。根据持续内存余量续编jobs8，单链接任务限制仍1。Windows代理Git已取得XNU ac9718...及6个小依赖；corecrypto仍下载。跨Windows文件归属使Git本地upload-pack拒绝缓存，进程safe.directory未穿透transport，改复制完整Git对象到WSL自有source-cache再导入，不改全局safe.directory。

XNU及全部内核依赖固定版本均下载并导入WSL成功。68补丁首次卡在zzz-boot-progress：其上下文依赖Sheng PMGR不在TB375FC序列；独立副本移除不存在的Sheng上下文hunk，并将Sheng watchdog调用限定XNU_SHENG_PMGR，重新生成diff；所有68项官方应用通过，5个缺失头文件按blob哈希核验补齐。评审记录/root/work/tb375fc-progress-patch-review.json。随后观察到WSL PID1 uptime不足一分钟，LLVM编译进程已消失、日志尾出现NUL；原因未证实，Windows PowerShell也出现0xC0000005。没有将其报告成源码编译失败。保留对象并降回jobs4续编，日志kernel-toolchain-recovery.log，session13928。

实际故障定位：Windows C盘Free=0，WSL CreateInstance E_FAIL；仅查WSL虚拟剩余容量漏了宿主盘。已清理本任务不完整LLVM下载；Windows Git缓存已复制/完整hash及8仓Git fsck核验后迁D，并用目录junction保留原路径；旧toolchain-cache（639706865B的LLVM MSI）也已迁D并逐文件hash核对，原路径junction保留。当前C约1.3-1.5GB，D约36GB。首次Move未带Force导致隐藏/只读项部分迁移，已合并核验并用Force移动余项至D/git-cache-c-remnant，完整缓存位于D/git-cache。没有删除设备备份。
中间文件审计发现100个.o、16个.a、21个生成.inc为空，已仅移除这些无效生成文件供Ninja重建；重建756项，日志kernel-toolchain-repair.log，session25107。首次链接缺符号由这些空文件解释。
后续kernel输出改D:/tb375fc-port-20261107/kernel27-build，已通过文件/符号链接检查；目录为UID1000且不区分大小写，fsutil设置case sensitive被拒绝，不修改系统权限。使用任务私有Git配置文件列明精确safe.directory并经本地Git transport继承；不改用户global Git。D盘源码导入完成，补丁准备中session59823。
用户确认有可用Mac；后续根卷/Data镜像可按官网在Mac制作，当前继续工具链及内核编译。

工具链完整安装成功（toolchain ready）。实际编译ABI验证通过：Mach-O arm64e subtype2、iOS27 LC_BUILD_VERSION、认证指针重定位type11。系统llvm-readobj18显示Unknown，使用原始Mach-O结构核验通过。D盘68补丁准备完成。正式kernel构建已启动session29667，日志/root/work/tb375fc-kernel27-workspace/kernel-build.log，输出D盘，skip-prelink用于早期原生内核。当前C盘约16GB可用。

继续核实首轮XNU构建：installhdrs阶段持续推进，kernel-bootstrap.log有新头文件输出；尚未进入完成链接，不宣称可刷内核。C约16GB、D约36GB可用，当前无新增错误。

用户要求内核完成再通知。当前真实阻碍：firehose编译sys/ucred.h使用OS_PTRAUTH_SIGNED_PTR_AUTH_NULL，展开四参数__ptrauth(..., authent icates-null-values)，已构建upstream clang24仅接受1-3参数。未擅自去除空指针认证语义或降为arm64。需要Apple clang支持或实现对应编译器功能后继续；Mac此前用户确认可用。错误日志kernel-bootstrap.log及kernel-build-resume.log。

保留指针认证语义的候选补丁已生成kernel27-tools/pac-null-compiler/llvm-auth-null-candidate.patch，状态UNBUILT_REVIEW_ONLY。修改属性参数入口、仅识别authenticates-null-values、动态认证/签名/不同null表示转换，未修改XNU或替换已安装clang。新增生成IR验证用例，必须验证零值赋值、读/比较/copy、普通与signed-null之间转换、局部聚合/全局零初始化、非法option拒绝；全部通过之前不得用于内核。源码恢复下载中，独立D盘缓存。

LLVM280629308B源码包完整下载并解包至/root/work/tb375fc-llvm-pac-candidate/source。候选补丁首次因Windows CRLF不匹配，规范为LF后重试通过，独立编译配置进行中，session80940，日志/root/work/tb375fc-pac-null-candidate.log。候选编译器未安装，原内核构建尚未恢复，语义测试仍为准入门槛。

PAC候选clang第一轮3134项编译完成，但语义用例在parse阶段失败：ParsePtrauthQualifier还有硬编码ArgExprs.size()>3，非Sema/Attr限制。已补ParseDecl.cpp及诊断数量为4，保留parser-candidate.patch，Ninja增量编译/测试session56418，日志/root/work/tb375fc-pac-null-parser.log。此外CGExprConstant显式null常量签名候选已保存constant-null-candidate.patch。仍未安装候选或继续XNU，必须待语义测试全通过。

PAC候选编译器已构建，补齐Parser四参数和CK_NullToPointer常量签名路径后，test-pac-null-codegen.py全项通过（动态赋值、读/比较/copy不绕过null，局部聚合、signed-null普通转换、全局零初始化签名、未知选项拒绝）。尚不等于完整编译器回归或实机资格。仅独立kernel27-workspace允许TB375FC_CROSS_CLANG显式覆盖，未覆盖已安装工具链；XNU已恢复session新进程，日志kernel-pac-compiler.log。

用户实施计划明确不新增刷写。当前XNU在prepare-xnu.py libs拉LLVM runtime源码无进展，已停止该fetch并改用已完整下载的固定LLVM源码归档提取compiler-rt profile；另下载固定Libc899a3b2d归档，缓存/root/work/tb375fc-kernel-runtime-cache，记录SHA256及commit。仅独立prepare-xnu.py添加精确commit本地缓存优先，原仓库未改；恢复构建session96490，日志kernel-runtime-cached.log，尚未链接完成。

XNU进入实际CC/CXX编译，无新fatal error；观察clang CPU约3-5%、D状态等待读取，NTFS/DrvFS为明显瓶颈。为保留进度，停止此任务build-kernel27的明确进程树，创建D:/tb375fc-port-20261107/kernel27-build.ext4稀疏12GiB镜像并loop mount至/root/work/tb375fc-kernel27-ext4，rsync完整复制现有树中。原NTFS目录不删；校验内容后bind mount到原输出路径，所有嵌入绝对路径保持。迁移session9361、日志/root/work/tb375fc-ext4-migration.log，未完成不得恢复编译。计划后续跨WSL重启需重新loop及bind挂载，不能误在旧NTFS副本继续。PAC完整源码差异已导出llvm-pac-null-complete.patch并reverse apply检查，build-evidence.json记录候选binary和patch SHA256/测试范围。

迁移继续rsync：约一万文件已复制，后续还需内容checksum检查、bind mount；未宣称迁移完成。已启动受校验门槛保护的自动续编进程session66643，等待明确迁移PID15401退出及ext4-migration.json完成标记后才运行实际内核构建，日志kernel-ext4.log。build-kernel27.py已新增镜像存在时自动loop+bind挂载并验证ext4文件系统、缺完成标记拒绝构建，防止重启后误用旧NTFS副本。当前尚无可刷内核；设备未写入。

用户明确要求持续跟进直到内核完成，不可仅报告后台任务后结束。迁移复制完成554853156字节，约25686条目；首次checksum使用rsync-rcn未带-l导致符号链接打印skipping non-regular而被assert误判，不代表普通文件内容不一致。未bind mount，自动续编保护正确拒绝。现使用rsync-rlcn重新完整校验，session99677，PID16212，差异输出/root/work/tb375fc-ext4-check.log目前0字节。通过后需正式bind原路径、写迁移marker并重新启动build-kernel27，原NTFS副本完整保留。

内核构建完成：ext4完整内容+symlink校验通过后bind原路径并恢复。首轮bootstrap因无provider链接记录留存正常结束；随后补齐独立设备kernel.archives（corecrypto plain/namespaced、ArmDTPlatform、ArmCpu、SoftwareAES、pthread），源码corecrypto529项0失败；prelinked模式在原生内核留下source_corecrypto14引用未定义，独立配置改为source并重编。最终kernel-source-crypto.log成功输出native only。
产物tb375fc-port/kernel27-native/kernel.macho 22248464B，SHA256 5c7f78fdfe1e801ac0ebfaeab70aaf4b8bc742a505e93db3c3b9735245560e3b；实际Mach-O核验arm64e/MH_EXECUTE/iOS27、LC_UNIXTHREAD入口0xfffffff00753a488落可执行段、全部文件范围合法、llvm-nm -u为空、arm_init与source_corecrypto14必要符号存在。副本hash一致。qualification.json、完整symbols及最终/首轮日志已导出。内核构建与host结构校验完成，不是硬件启动或用户态成功。未做新增刷写，后续ADT/负载/内存布局及DarwinBoot测试仍待完成。

首启负载：独立board设置32MiB FAT16，官方payload生成器生成内核/ADT/配置（不加root/Data/trust），内核hash与原产物一致。ADT单启动核、intc节点导出、GIC地址来自实测；synthetic平台seed保存在private，不当作硬件ECID。未改共享SoC。负载LZMA实测约4.6MiB，因此独立固件快照tb375fc-first-xnu-boot嵌入压缩FV FREEFORM RAW FAT，RamDiskDxe优先读取严格标记的32MiB嵌入卷并发布只读BlockIo，不同时发布旧initrd卷。原成功快照不改。固件编译session43898。尚未完成固件编译/包校验，未刷写。
首次XNU测试镜像first-xnu-boot/boot-xnu-entry.img已准备，5545984字节，SHA256 3df3befa146dec6eaccb6cd4a08d4b02bad7104cb9a57e69ff2770dae0b8e817。最终8MiB FD解压FAT比对、ADT读回、真实32MiB FAT边界ASan/UBSan、菜单源码字节匹配、BootShim/FD压缩往返均通过。单BSP、无自动boot/reset、入口蓝条及MMU前紫条；源码POST HMAC重建，完整panic屏因padding不足未启用。实际DarwinBoot内存窗口待实机确认。当前fastboot HA227LJ3，四个镜像/回滚hash已核验。用户此前明确本阶段不新增刷写，因此候选尚未刷入；需要新授权才能只写boot_a，init_boot_a保持现有。README及qualification.json记录限制与回退。
用户授权刷入测试。现场核对HA227LJ3/product P98300DA2/current-slot a/unlocked yes及候选SHA256通过。first-xnu-boot/boot-xnu-entry.img仅boot_a写入成功：Sending OKAY17.144s，Writing OKAY0.198s，Reboot OKAY。init_boot_a及b槽未改。等待实机菜单及手动XNU入口蓝/紫条结果，不将刷写成功当作内核启动成功。
首次XNU实机：用户观察先有绿色交接前标记，随后左半灰白、右半蓝紫纹理；无可读文字。不能据此确认XNU入口或MMU完成，hardware-result.json已记录。诊断版启用上游panic reporter，移除两行页面统计以容纳padding，黑底改alpha FF，保留蓝/紫阶段标记并重封SOURCE POST HMAC；固件BUILD OK，负载解压/ADT/FAT sanitizers/菜单源码核验通过。xnu-panic-test/boot-xnu-entry.img 5550080B SHA256 2b5cb91697b7162e4862dc6c0b8bf9c9fcac99cb6987df4bb81df2bc497c70ba。延续用户刷入测试授权，核对HA227LJ3/product P98300DA2/slot a/unlocked yes及镜像哈希后，仅boot_a刷入成功，Sending17.137s Writing0.114s Reboot0.118s；init_boot及b槽保留。等待用户再次选择XNU后的画面，未确认内核成功启动。
MMU reset候选生成与POST重封成功；新增核验器首次错误地对已打补丁内核重新调用仅接受原始SCTLR/ISB/CPACR模式的rebase而拒绝。修正为从未修改原内核定位，再核对候选branch和完整重启code字节；不放宽模式检查。
panic版用户报告绿色后全屏花屏，无可读panic；hardware-result.json保存照片和video参数。按官网green-bar步骤完成MMU后6秒重启版：独立probe脚本TIMEBASE=13000000，源内核定位checked SCTLR/ISB/CPACR，核验候选site 0xfffffff00753c438跳至stage3 cave 0xfffffff00753a5c0，完整linked reset代码逐字节相同，SMC PSCI与live DT一致，SOURCE POST重封。ADT比对仅/chosen/random-seed熵变化，其余49属性相同。固件96秒BUILD OK，FAT/ADT/解压/菜单源码核验通过。xnu-mmu-reset-test/boot-xnu-entry.img SHA256 e738ebd0c6b736ca8d370f2e83602821e5ac950ad7f21494182715afe0aeba45。续此前测试授权核对设备后仅boot_a刷入成功（Sending17.137s/Writing0.198s/Reboot0.116s）。当前等待用户手动XNU后是否约6秒自动重启，不能据刷写/编译宣称内核启动成功。
前移检查点候选：/root/work/tb375fc-handoff-checkpoints独立Mu+package副本，解除DarwinBoot symlink后本地复制，未改Windows共享组件。DarwinEnter新增x6模式，x12保存；A缓存维护后关MMU前/B关MMU后同步TLB完/默认C首kernel entry，三者CNTFRQ_EL0实际频率等待6秒PSCI SMC重启，不使用栈或关MMU后显存。内核副本仅入口分支、已验证padding与POST HMAC变化，原XNU启动未执行。完整原ADT从panic版复用。实际Mach-O parser套件通过；固件91秒BUILD OK；解压FV中确认包含两种参数字符串和完整编译Entry.obj机器码，FAT sanitizer/ADT读回/菜单字节匹配通过。boot镜像5541888B SHA256 4ff9c1265a88ae9c2ae1a171dcb37ffa8d00fe1457f3ab5065b00009c48e8126，证据保存xnu-handoff-checkpoints目录。
三检查点镜像已核对HA227LJ3/product P98300DA2/slot a/unlocked yes与SHA256后仅boot_a刷入成功（Sending17.138s，Writing0.200s），重启成功0.112s。上一轮中断发生在成功输出及进度写入之后，不重复刷写。flash-result.json已补齐；等待用户手动选择A Before MMU off后的计时重启结果，再决定是否测试B/C。
用户已反馈A项绿条后重启；hardware-result.json已保存，flash-result待办改为B项。无需编译/刷入新版本，等待同一菜单B After MMU off的画面与是否自动重启。
独立/root/work/tb375fc-handoff-code-clean准备完成（不覆盖三检查点基线）。内核/ADT/FATpayload原样复制；仅DarwinBoot增加自身代码PoC clean与identity guard。实际Mach-O parser现有套件通过。固件编译session54444，完成前不刷写。
code-clean候选编译144秒BUILD OK；实际Entry.obj显示DC CVAC x13偏移0x70，覆盖DarwinEnter..End0x108在IC IALLU和SCTLR写前；全部机器码确认存在最终解压FV，identity guard字符串存在。32MiB FAT payload与三检查点基线SHA完全相同d24bd120d6c3030a4484222b95da383e497d21b2a1eb3ccb336de5f4951bb10a，原ADT/入口reset kernel/菜单按键保持。最终boot-xnu-entry.img 5545984B SHA256 f5a92206fe64716f53d906c6eb5ab8a9c5966d1b5160ab928c76c8384d653d9b。核对HA227LJ3/product P98300DA2/slot a/unlocked yes及镜像hash后，延续测试授权仅boot_a刷入成功：Sending17.127s/Writing0.198s/Reboot0.077s。待用户复测B；尚不能宣称缓存是实机根因或XNU已启动。
用户确认code-clean版B/C均自动重启，无花屏；hardware-result.json单独记录，未把C reset stub当原XNU运行。准备独立/root/work/tb375fc-xnu-code-clean-boot，恢复先前0ee9c072... --no-reset蓝/紫+panic kernel，复用原ADT，菜单改TB375FC XNU27 boot。保留已验证Entry.S原字节，C仅改normal/timed日志文字。机器码核验入口及pre-MMU绘图确实继续原start_first_cpu/原SCTLR下一条，post-MMU原mov x0,#0x300000存在，无stage3reset，panic两个hook存在。固件编译session83664，完成后仍需最终FD/负载/保留loader机器码核验再刷写。
恢复原XNU bootstrap镜像构建/打包完成。最终FD解压负载、FAT sanitizers、ADT读回、稳定菜单源码、entryblue/preMMUmagenta继续原启动及postMMU非reset机器码均核验。最终loader transition SHA256 9f2dd07e8b134c6f8a06dd3ea22c63e320ca75adc79832ef69ef1e79447bb376与B/C实机通过版一致，payload kernel0ee9c072...，原ADT b501fcc...。产物xnu-code-clean-boot/boot-xnu-entry.img 5550080B SHA256 2f313ea5a1162e3dafae0996c07b5d104d91e969f7178b12cd1807133b0e8477。核对设备/slot/unlocked/hash后仅boot_a刷入成功（Sending17.128s/Writing0.199s/Reboot0.050s）。等待手动选择TB375FC XNU27 boot后的新画面；此版无故意定时重启，有panic输出，尚未确认原内核初始化成功。
2026-11-08：收到原XNU启动版可读Kernel data abort照片。符号/指令解析证实PC 0xfffffff007a89aa0=IODTGetICellCounts+0x34，ldr x16,[x0]，x0/FAR均0，ESR96000005；调用链进入ConfigureIOKit/IODeviceTreeAlloc。根因证据：ADT intc只有phandle，XNU AddPHandle登记AAPL,phandle，interrupt-parent无法解析；官网marble记录同一错误。修复Windows UniXNU/scripts/unixnu/devicetree.py及独立kernel27-workspace副本，保留原phandle并补Apple别名，支持linux-only、拒绝0或冲突；新增5个实际converter unittest均通过，双语文档已更新。新独立/root/work/tb375fc-xnu-adt-phandle-fix将原ADT仅新增/arm-io/intc/AAPL,phandle，旧10920B hash b501...，新10960B hash88c473ea60474e44f71cc554d02e014c75f82db80b4fdac778abd09d2dda0d5a；其他属性字节一致，原来未解析的GIC interrupt-parent现在可解析，kernel/loader/config不变。固件编译session32082进行中，尚未刷写。
2026-11-08 ADT phandle候选：固件141秒BUILD OK；最终FD解压确认kernel/loader transition与原panic版一致，实际FAT读回ADT新增AAPL,phandle可被XNU登记，所有interrupt-parent解析及cell宽度检查通过；原ADT全部已有属性字节一致，5项generator regression通过，git diff --check通过。产物xnu-adt-phandle-fix/boot-xnu-entry.img 5541888B SHA256 b4f55025b747568d6b535b93069617e7653f0f14cd23284df18a02b7d50793fd。核对HA227LJ3/product P98300DA2/slot a/unlocked yes及候选hash后仅boot_a刷入成功，Sending17.128s/Writing0.200s/Reboot0.103s。等待用户手动XNU后的画面，尚未确认此panic实机消失或完整内核启动。
IMG4诊断完成并保存IMG4-NEXT-STAGE.md及最新hardware-result.json；无新增编译进程、无刷写、无原系统恢复。下一阶段是正式prelink及真实安全组件早期注册，不能靠跳过panic继续。用户此前让等待恢复/刷写，本轮保持设备分区不变。
2026-11-08纠正操作范围：误把用户进入bootloader当作恢复触发，启动了原boot/init_boot恢复命令；用户中断并明确要求继续修内核、不重启原系统。已停止恢复父进程29920及携带原备份路径的fastboot进程14844。工具未返回刷写阶段结果，不能确认是否已写入；当前设备未枚举fastboot/adb，需以后只读核对，不宣称分区保持不变。用户最新要求后续仅在电脑修内核，未经新明确指令不得刷写或重启。继续IMG4预链接准备。
继续内核预链接：已用既有Linux xcrun shim和已验证PAC clang启动独立RELEASE-LTO reference，实际源码对象完成后链接失败。确认原cctools ld64构建时--disable-lto-support（binary含no builtin LTO support），不是XNU源码编译报错；主机nm也不能解析新LLVM bitcode。已用同一LLVM源码增量构建libLTO和llvm-nm。保留957个既有native目标/本轮reference对象，准备复用已生成的LTO对象只重做链接。预链接kit现有62测试通过。安全组件最小roots（AppleImage4/AMFI/CoreTrust/corecrypto）依赖闭包19项已导出security-closure.json，包含实际ApplePMGR等依赖，不自行替换provider。
独立LTO ld64构建：最初假设host-tools源码为git checkout，实际为已校验源码tar归档，改用既有sha256 ab76388e...2213770B归档解包；首次开启LTO编译缺LLVM24新增llvm-c/Visibility.h，已加入llvm-config --cppflags提供原始/生成头目录。当前session31236构建私有cctools-lto-prefix，不覆盖原工具链。reference第一轮LTO源码位于ext4 lto-18dfbebc93dfd6ef，失败日志/命令已保存；需同版本LTO ld64完成后恢复链接，不能重置整个编译进度。
独立LTO ld64已从已校验cctools归档构建，补齐LLVM24头目录后成功，不覆盖原工具链。真实mixed arm64e assembly+LLVM24bitcode smoke链接通过（纯bitcode -r可合法输出合并bitcode，测试已改为与XNU相同mixed输入）。恢复reference链接时最初覆盖LD漏了-nostdlib，导致自动寻找libc++而失败；修正为clang -nostdlib -fuse-ld=私有ld64，现LTO优化真正完成第一轮并生成justlto.o。按官网reference-only audit去掉67个本配置缺失强制导出，第二轮链接进行中session92447。所有修改限主机比较副本；native/source内核未改、设备未再操作。下一步严格security prelink helper已备，不启用allow-missing。

2026-11-08 主机续修：RELEASE-LTO比较内核已完成，arm64e/iOS27、无未定义符号，SHA d1be4f12df221991d0bcd6615fe954013280a6b15f12cc0f94652795633e4a8b；它仅用于符号配对，不可当作启动内核。四安全组件严格预链接在缺失 _unixnu_kernel_stack_shift 处停止。核对实际 stack.c 后发现当前16KiB栈、4KiB页、三保护页，现有分配路径并不明确满足fixture声称32KiB对齐的前提，因此没有添加未经证明的常量或更改保护页。先隔离AppleImage4+corecrypto的真实依赖闭包。修复预链接器在组件未入选时仍要求其per-site原生符号的问题；入选位置仍严格检查，63项host测试通过（新增未选/入选/全选缺符号回归）。当前仅主机运行严格Image4预链接，无allow-missing、无新增刷写或重启。

2026-11-08 本轮完成：定位原生MH_EXECUTE缺LC_SEGMENT_SPLIT_INFO，原因现有standalone模式XNU_KERNEL_CHAINED_FIXUPS=0。经官方cli.prepare/patches.prepare恢复目标refer链接后，复用对象、以XNU_KERNEL_CHAINED_FIXUPS=1完成原生重链接。输出prelink27/native-fileset/kernel.macho，23,693,928 bytes，SHA256 1c6e73c93f4a4f0e92315506d00dadeeab7cefdca8e82baf03d927a5a86339d1。arm64e/iOS27/MH_EXECUTE、可执行入口0xfffffff00753a488、433137条有效split-segment记录、chained fixups、无未定义符号。原kernel27-native和所有实机候选未覆盖；WSL原始mach备份于/root/work/tb375fc-prelink27/native-threaded-backup。脚本tools/build-native-fileset27.py可复现该重链接，日志qualification在输出目录。
严格Image4+corecrypto闭包重试已完成，11项依赖包含CoreTrust和IOCryptoAcceleratorFamily；56处未解析引用/39个唯一原始地址，无stub，工具正确拒绝生成candidate。summary、完整report、map、log、unresolved-targets.json已保存prelink27/image4。其余阶段不再是编译错误，而是原始安全组件的ABI配对；不能仅根据相邻地址名称猜测绑定（部分reference-order邻近命名明显跨到recount等其他函数族）。AMFI栈布局接口仍未修改或验证。XnuPrelink63测试再次通过，既保留未选组件过滤，也验证入选missing报错和正确site绑定。全部主机进程已结束；本轮未新增设备操作。下一步对39目标逐个以原始机器码、调用者、PAC/vtable及源接口证明配对，之后重新严格预链接；完整iPadOS启动仍未完成。

2026-11-08 ABI推进：40个函数identity+1个kOSBooleanTrue常量指针已审核并固定原始函数SHA。严格Image4闭包缺失从56降7后降至0，无stub。首个严格完整集合95,125,504B SHA ccef5d6ed833536179a851142256975568b8cab29be22c55d86783ac29787097，但实际DarwinBoot parser拒绝：root subtype c0000002 vs native member80000002，private生产代码line264。没有删除检查或刷写。
LLVM24 MachObjectWriter.cpp明确硬编码arm64e user ABI0、不支持kernel标记；native编译已有-mkernel和-fptrauth-function-pointer-type-discrimination。修私有ld64使显式-kernel输出正确kernel ABI0，未知version仍拒绝，不改全局工具链。首次make漏跟踪templateheader依赖，smoke如实失败；触发OutputFile.cpp重编后通过：user subtype80000002、kernel c0000002、函数指针真实AUTH discriminator分别0/2981、ABI1输入被拒绝。patch/smokequalification已保存prelink27。现复用对象重链接，再严格预链接并运行原生产parser。旧native及首个严格输出保留。

2026-11-08：Image4/corecrypto 严格预链接与生产 DarwinBoot 解析已通过，候选 SHA256 29bbce64dd734eafbca04b1c88f59389bc0f863ed32e1adb75a07313b70bf6f5；无 missing/stub。AMFI 栈分配正式 OS 补丁已准备，70,536 个分配位置主机 UBSan 检查通过。上一轮增量构建无完成记录且无残存进程，本轮重新执行，尚在编译。所有设备操作仍暂停。

2026-11-08 本轮结果：将实时构建日志移入 WSL，解决 Windows 日志写入造成的 D-state 等待，增量构建完成。新内核 23,694,112 bytes，SHA256 91ce34e4a1559ec631ebd787cd4582c8c5a11f258d454ce9cb204721b37d99c8，arm64e subtype c0000002，入口 fffffff00753a488，433143 split references，无 undefined，AMFI stack shift 已导出。Image4/corecrypto 严格依赖链零 missing/零 stub，实际 DarwinBoot parser 通过；负载 95,125,504 bytes，SHA256 198a0fa8d2476b507312c67b13f03b0c16114ce49a94ca5d3ed4c8ff9b8044ad。完整四安全组件闭包仍有8组件226引用/162唯一目标，严格模式拒绝输出，未伪造绑定。security/target-evidence.json 与 target-disassembly.txt 已导出，zerofill目标单独标记而非当作代码读取。RELEASE-LTO参考为补丁前固定版本，当前仅受控新增 stack.c AMFI 栈补丁，不宣称参考与新内核完全同一补丁集。本轮没有设备命令，未刷写、重启或恢复。下一步逐项审查完整闭包 ABI，不能将主机构建/解析成功等同实机启动成功。

2026-11-08 平台 ABI 绑定推进：新增26个经审查接口，14个原始函数除BTI外与native完整机器码相同；其余按源码、成员偏移、dispatch slot、PAC、参数及返回值验证。共享override PAC必须用完整函数体区分基类/派生类。原始PAC也可能包含多个地址，因此改用该原始目标存在且native目标唯一的断言，不谎称原始集合唯一。四组件严格检查missing由162目标226引用降为136目标197引用；仍拒绝输出完整candidate，未使用allow-missing或stub。63项现有工具测试通过。证据prelink27/security/reviewed-platform-bindings.json，复现tools/review-platform-bindings27.py。README及EN/ZH组件文档更新。设备无操作；当前不需要用户输入。下一步审查剩余136目标（包括数据/虚表与复杂平台方法）。

2026-11-08 效率优化：新增可选 trusted-local decoded Side 缓存，完整镜像SHA/头偏移/fixup范围/分析代码/Python版本作key；fixture变化只刷新匹配，不失效解码。递归命名reference复用同一解码对象。68测试通过，包含损坏重建与失效检查。真实输入强制rematch对照cold 66.365秒(0hit/3miss)，warm46.985秒(3hit/0miss)，严格报告逐字段完全一致，136个missing未放行。组件审查队列136项，4.247秒(3hit)，只输出建议，无自动绑定。首次尝试在audit删除旧report期间并发生成队列失败，已改为audit完成后顺序运行，核验report输入SHA。

缓存/批量审查工作流已落地并通过真实输入验证：{"cold_seconds": 66.365, "warm_seconds": 46.985, "strict_reports_identical": true, "missing_targets": 136, "device_written": false, "map_reuse_seconds": 9.783, "map_reuse_report_identical": true, "review_queue_seconds": 4.247}。未重编内核；未执行设备命令。后续按review-queue.json组件批次核对，批次结束统一严格预链接。

2026-11-08 继续适配：平台第二批9方法（完整源码/机器码/PAC/参数/返回流），5个metaClass常量指针+2个gMetaClass直接对象（getter及类名初始化交叉验证），6个类身份已确认且全槽位数和认证布局完全一致的虚表。严格预链接由136目标197引用降为114目标165引用；candidate_written=false，stubbed空。68测试通过。证据security/reviewed-platform-batch2.json、reviewed-metaclass-pointers.json、reviewed-platform-vtables.json。队列已刷新114项，缓存3hit，4.612秒。中断注册original多出ap-wake-interrupts初始化helper并更新全局，native源码缺该逻辑，未盲绑；PMLog/睡眠包装仍待证明。虚表getMetaClass位置按地址点+0x38，即符号基址+0x48核对，首次用符号+0x38读到release后已纠正，最终全部身份断言通过。未重编内核，未执行设备命令。下一批可核对OSMetaClassBase虚表13槽唯一布局及thread_block包装；仍需严格证据，未写入这些待审项。

2026-11-08 后续：9方法+OSMetaClassBase虚表验证后114→104；3通知/Blocks全局变量后104→101。入口BTI归一化新增4回归，72测试通过，严格101→92（12解决，3新missing），无既有reviewed目标被替换。发现旧map错误将IOService::start映射IORootParent::start；原始start有MAC权限检查，native旧源码仅true，拒绝直接绑定。原始policy slot0x1e8/PAC0xe4fe与native同型双参数MAC回调吻合，正式OS-owned四文件补丁已准备，335指针政策表布局和8种允许/拒绝主机UBSan测试通过。新内核构建正在运行，本轮必须等构建完成并检查实际call/slot/PAC，不能只凭导出符号声称完成。未执行设备命令。

2026-11-08 新最小首启镜像已完成并获用户明确授权刷入：内核1f8a71624ac210d9c023e320eaa7d083c4bc887b72b72e81cb9c6b0a12995b90，MAC启动钩子实际调用/slot0x1e8/PACe4fe验证通过。元类精确构造参数审查11项无错，并确认此前自动RootParent虚表实为IOService，已纠正。Image4/corecrypto严格最小闭包0missing/0stub、生产parser通过，95,125,504字节，SHA1817068d295e5112749c171fe53e7b1fe217dc8664218d4a4e8d980a41efa24f。独立载体FAT128MiB/FD32MiB，FD40000000..42000000，栈42000000..42040000，堆至47c00000，低于LK DTB47c80000；内核窗口动态从UEFI可用内存分配。保留按键/菜单源码；扩容FAT解析ASan/UBSan通过。EDK2使用上一版edk2-202511修正首次路径错误；大GUID section超过16MiB使用扩展头，打包审核已适配。候选boot19,345,408字节，SHA59841a20bf65e63fdacb242c4efe9f61aadbcf708dbf21de1067083c33ef9fa7，v4零ramdisk，回退为上一版UniXNU而非stock Android。实机只读确认HA227LJ3/P98300DA2/slot a/boot_a64MiB后仅flash boot_a；发送59.193s、写入0.200s均OKAY、exit0；随后fastboot reboot OKAY。等待用户菜单及XNU画面，尚未宣称越过Image4 panic。全安全链92是旧native91ce上的最后统计，不能当作当前新native1f8的最终数量；本轮优先准备最小实机测试。

### 2026-11-08 接手路线复核
- 根据最新接手记录，确认移除 FDF 中无消费者的 128 MiB payload FREEFORM 后，实机已回到 UniXNU 菜单；不要再沿用旧 96 MiB FAT/20 MiB FD 的 SEC 修复方案。
- 对照 UniXNU 官方 UEFI / boot 文档：大 payload 应来自 GKI `vendor_boot` 的 `vendor_ramdisk`，通过 FDT initrd 交给 `UniXNURamDiskDxe`；boot 镜像自身保持 ramdisk=0。
- 更正早期“当前无 vendor_boot 输入”的判断：D 盘现存 ZUI 17.0.04.016 用户固件包，可只读解析；其 vendor_boot SHA `935af3d2…`、v4、64 MiB、ramdisk 26,913,632 B、DTB 411,876 B、2 个 table entries。修复 `tools/parse_vendor_boot.py`，原工具把 vendor_boot 当作 Android boot header 解读。
- 该映像比设备最后采集到的 ZUXOS Android 16 版本旧，不能作为当前刷写基底。当前活动槽 vendor_boot 无法 fastboot fetch；本轮没有从设备回读、刷写或重启。
- 设备当前 initrd 观测为 0x66f00000..0x68b246ed（28.14 MiB），周边有 reserved 区；95.1 MB 内核 payload 不能由历史 2 MiB init_boot FAT 承载。需要匹配当前版本的 vendor_boot 或完整官方包，再确定容量和RAM交接方案。

用户最新指示：回到 Image4 阶段，修一步测一步。再查 UniXNURamDiskDxe 后发现其现有 `PublishEmbeddedPayload()` 会通过数值 GUID `4238B7F0-0271-4B91-A5A1-827375FC0027` 从 FV RAW section 读入 FAT；早期“FDF FILE 无消费者”结论来自纯文本 GUID grep，判断错误。当前路径转为恢复 FDF embedded payload consumer，避免改 vendor_boot。

- 已检查 DXE entry 顺序：先发布 embedded payload；仅 section 不存在时再回退 FDT initrd。
- 实际 128 MiB FAT 用 `xz --format=lzma -1` 生成 LZMA-Alone 流，大小 21,123,379 B（约20.14 MiB）。压缩后 FVMAIN 估算约23.1MiB，低于91.75MiB early heap；FFS 文件仍超16MiB，需保留并依赖大FFS header修复。
- MemoryMapLib 确实描述全局8GiB DRAM及多个高位可用区；DXE阶段可申请展开卷和DarwinBoot窗口，但需通过固件驱动日志/实机阶段单独确认分配。
- 目前只完成源码/布局审计与压缩尺寸计算；尚未改动当前构建树、未创建候选、未执行设备写入/重启。接下来以独立副本实现 LZMA 解压及 payload hash/卷扫描核验，然后重新构建，目标先验证菜单配置项/payload discovery，不一口气归因到 Image4。
# 2026-11-08 接手审计：修正 payload 路线

用户提出可能方向有误，重新核对最新接手文档、实机 A/B 记录、已有产物、设备取证和 UniXNU 官网流程。

- 已确认 `nopayload-ab-test.img` 实机越过 SEC 并进入 UniXNU 菜单。根因是 FDF 中无人消费的 128 MiB `FirstXnu/payload.img` FREEFORM 撑大 FVMAIN；删除后 FVMAIN 2.92 MiB，SEC early heap 91.75 MiB 足够。旧 handoff 的“再缩 96 MiB FAT/20 MiB FD”方案已被更新文档证伪，不应继续执行。
- 官网 UEFI 集成和 boot 流程要求：大 payload 放 GKI `vendor_boot` 的 `vendor_ramdisk`；boot ramdisk 保持 0。FDT `/chosen/linux,initrd-start/end` 经 `UniXNURamDiskDxe` 发布 FAT 卷。
- 过去 init_boot_a 2 MiB FAT 只验证了配置标题发现，不能容纳当前约 95.1 MB 内核。当前 96 MiB 镜像候选的 boot header 也都 ramdisk=0；它们仅是主机产物，不是可运行完整负载的证明。
- 设备实测 initrd 地址 `0x66f00000..0x68b246ed`，约 28.14 MiB；`iomem` 标出 `0x70000000..0x74ffffff` reserved。不能直接在原地址扩成 96/128 MiB。
- 本地 `live-fastboot-backup/vendor_boot_a.img` 是 0 字节；没有原始 header v4、DTB 和 vendor ramdisk table/fragments。无损 vendor_boot 打包现在缺此输入。
- 本轮无设备命令、无刷写、无分区变更。

结论和下一阶段门槛已写入 `task_plan.md`、`findings.md`：先取得与当前版本匹配的原厂 vendor_boot，再离线解析并设计载体/地址；确认回读和分区约束后才考虑新设备测试。没有把 kernel 构建成功或菜单恢复等同于 XNU/iPadOS 启动。


## 2026-11-08 Image4 负载逐步修复

- 结论：当前还不是 AES 实机验证阶段。之前的内核崩溃截图报 `image4 interface not available`，而那是内核 Image4/信任缓存接口路径，不足以归因为 AES。我们此前没有把这版内核成功交给 XNU 执行，因此也没有实机 AES 日志。
- 已在隔离副本 `/root/work/tb375fc-image4-test27` 修正传输阶段：FDF 将 128 MiB FAT 以 LZMA-Alone 压缩后放入 UniXNU RamDisk DXE 消费的 RAW section；DXE 解压时强制展开大小恰为 128 MiB、scratch 上限 32 MiB，再校验 FAT 标签、BPB 和完整扇区范围后发布只读 Block I/O。
- 128 MiB 原卷 SHA256 `675f3100ac4eb54723ba4435d2a4c49ea2429d17a3b0ae2f7ed64930a8e847f6`；压缩文件 18,570,799 B，主机 EDK2 解压往返 hash 相同。隔离固件构建成功，FD 33,554,432 B，SHA256 `28117da8525218c03d964dd7a969622a11d70093ffbf95e337d3dc4889f39405`；FVMAIN 21,647,360 B，compact FV 占用 19,501,432 / 33,554,432 B。构建日志 `/root/work/tb375fc-image4-test27/build-lzma.log`。
- 新 boot 候选在 `tb375fc-port/image4-lzma-step/boot-image4-test.img`，SHA256 `f5b8b6a9a36afa19f31e95166d9e6b008641bdbd8a3f2b6be9b18454368d2c41`，19,607,552 B，Android boot v4、ramdisk=0、低于 boot_a 64 MiB。配套回退仍是先前 UniXNU 测试镜像，不是 stock Android。`qualification.json` 记录 `device_written=false, hardware_tested=false`。
- 主机 packaging 审计成功：确认 FV 内扩展 FFS RAW 的 LZMA 数据解压后与 128 MiB FAT 完全一致；FV 压缩层解开后可找到压缩负载；生产 DarwinBoot kernel parser 通过；内核窗口需求 95,141,888 B，额度 268,435,456 B；boot roundtrip/header/ramdisk checks 均通过。
- 尚未刷写该新候选。下一步是只验证 boot_a 上 SEC→菜单→DXE LZMA 解压→菜单 payload/config discovery，观察是否显示 XNU 启动入口；只有该阶段通过，再测试 Image4 到 XNU。不要把菜单显示当作 AES/XNU 成功。

### 2026-11-08 逐步返回 Image4 负载阶段
- 回答“是否卡在 AES”：没有证据显示当前阻塞是 AES；已知旧截图明确是 `image4 interface not available`，这是 Image4/信任缓存接口缺失信号，不是 AES 失败诊断。当前新内核尚未在实机运行，无法说已验证 AES。
- 修复方案：在隔离构建树中压缩 128 MiB FAT 为 18,570,799 B LZMA-Alone；恢复正确的 FDF RAW consumer GUID；RamDisk DXE 于 DXE 阶段分配解压缓冲，限制输出 128 MiB、scratch 32 MiB，校验 FAT 后发布 Block I/O。
- EDK2 固件 build rc=0，FD=32 MiB，SHA256 `28117da8525218c03d964dd7a969622a11d70093ffbf95e337d3dc4889f39405`；FVMAIN=21,647,360 B；FVMAIN_COMPACT used=19,501,432/33,554,432 B。解压往返与原 FAT SHA一致。
- 主机打包审计通过。新候选 `image4-lzma-step/boot-image4-test.img` SHA256 `f5b8b6a9a36afa19f31e95166d9e6b008641bdbd8a3f2b6be9b18454368d2c41`，19,607,552 B，v4 ramdisk=0，boot_a 容量64 MiB。production DarwinBoot parser、内核范围预算、FV RAW 及 FV 压缩层检查通过。
- 尚未刷入或实机测试。下一步仅验证 SEC、菜单、DXE 解压及 payload/config discovery；阶段通过再验证 Image4→XNU。

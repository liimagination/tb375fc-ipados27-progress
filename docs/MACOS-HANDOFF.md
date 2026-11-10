# macOS 工作迁移指南（TB375FC / iPadOS 27）

更新：2026-11-10。目标 Mac：MacBook 12-inch (2017)。此文档说明如何接续 Windows/WSL 上的进度；不表示 Mac 已完成构建验证。

## 1. GitHub 仓库是什么

私有仓库：`https://github.com/liimagination/tb375fc-ipados27-progress`。当前初始快照提交为 `4697d7a79b32df17df1303b98951462fa4a35ef4`。先在 Mac 安装 Git 并克隆：

```sh
git clone https://github.com/liimagination/tb375fc-ipados27-progress.git
cd tb375fc-ipados27-progress
git status --short
```

仓库是精选交接快照，不是 UniXNU 上游仓库，也不是完整的可直接编译工作树。设备端源码配置、测试证据、最近的 LZMA DXE 源码和两个上游工作树的补丁都已纳入。原厂镜像、固件、内核和构建目录刻意不在仓库中。

## 2. 建议的工作方式

MacBook 12 2017 是低功耗双核/低内存机器。先把它作为源码浏览、补丁审阅、文档、轻量脚本与小型编译验证环境；不要假设它适合完整 XNU/EDK2 大型干净构建。先查看机器配置：

```sh
sw_vers
uname -m
sysctl -n hw.ncpu
sysctl -n hw.memsize
xcode-select -p
clang --version
python3 --version
```

项目实际固件构建此前在 WSL Ubuntu 24.04、root 用户、28 个可用 CPU 上执行，使用独立 Mu-Silicium、EDK2 和 UniXNU 工作树。记录的本地 EDK2 路径是 `/root/work/edk2-202511`，隔离构建根为 `/root/work/tb375fc-image4-test27`。不要把这些 Linux 构建路径直接套成 macOS 路径。

移植的入口脚本是 UniXNU `package/build.sh`，默认需要 Mu_Basecore/EDK2 及指定子模块、BaseTools 和 `clang`。脚本注释记载 Mu_Basecore `release/202608`，但这不等于本项目隔离树已验证的精确提交；首次重建前应从 Windows/WSL 原目录读取 Git commit、submodule 状态、工具版本和构建日志并补录。不要只按分支名拉取浮动版本。

## 3. 复原源码快照和补丁

两个补丁对应的基线已记录：

- UniXNU：`source-diffs/UniXNU-base-commit.txt`（`7307c26ea5053a9b7a3e58b81a12b6f1d5a2b226`）。
- `kits/XnuPrelink`：`source-diffs/XnuPrelink-base-commit.txt`（`b1888c12456c46a2226d63c794465dfcee08d465`）。

先获取对应仓库和该精确提交，再应用父仓库补丁与新文件；XnuPrelink 是子模块，需单独恢复：

```sh
git clone https://github.com/UniXNU/UniXNU.git UniXNU
cd UniXNU
git checkout 7307c26ea5053a9b7a3e58b81a12b6f1d5a2b226
git submodule update --init --recursive
git -C kits/XnuPrelink checkout b1888c12456c46a2226d63c794465dfcee08d465
git apply --check ../tb375fc-ipados27-progress/source-diffs/UniXNU-uncommitted/working-tree.patch
git apply ../tb375fc-ipados27-progress/source-diffs/UniXNU-uncommitted/working-tree.patch
```

随后将 `source-diffs/UniXNU-uncommitted/new/` 中的文件复制到 UniXNU 仓库根目录的对应相对路径。对子模块按同样方式检查并应用 `source-diffs/XnuPrelink-uncommitted/working-tree.patch`，再复制其 `new/` 文件。应用后检查 `git status` 和 diff，避免把补丁套到不同上游版本。不要向 UniXNU 上游推送这些本地改动。

设备侧快照 `device/source/`、`uefi-tb375fc/` 和 `implementation/image4-lzma-step/` 用于参考和差异核对，不应盲目覆盖新克隆仓库。优先按交接文档的隔离构建方式重建；第一次 macOS 原生完整构建需要单独验证依赖和产物。

## 4. 从 Windows 另行复制的文件

以下文件不在 GitHub 快照里。如果准备在 Mac 上复现现有 Image4 阶段，请通过外接盘/局域网把这些文件从 Windows 原工作区复制过去，并保留目录结构。Windows 源根：`C:\Users\李昭杭\Documents\修包\tb375fc-port`。

### 必需：下一步实机测试材料

1. `image4-lzma-step/boot-image4-test.img`：当前 LZMA payload 启动候选，约 19.6 MB，SHA256 `f5b8b6a9a36afa19f31e95166d9e6b008641bdbd8a3f2b6be9b18454368d2c41`。主机检查通过，**尚未刷机/实测**。
2. `image4-lzma-step/qualification.json` 与 README（仓库内已有）。
3. `rollback/boot_a.stock.img`：历史原厂 `boot_a` 回退镜像，SHA256 `1ad2cb450587a101ec6fcb7e0db7cf0b3f9b9ff978cf972a7b52a909cb815357`。
4. `image4-first-start/rollback-unixnu-previous.img`：旧 UniXNU 测试镜像，SHA256 `b4f55025b747568d6b535b93069617e7653f0f14cd23284df18a02b7d50793fd`。它**不是原厂 Android 回退镜像**。

复制后在 Mac 校验哈希：

```sh
shasum -a 256 boot-image4-test.img boot_a.stock.img rollback-unixnu-previous.img
```

刷机工具和授权设备操作应在确实需要时单独准备。交接记录显示最后一次 `boot_a` 原厂镜像恢复后 fastboot reboot 返回 OKAY，但没有观察到 Android 用户态；设备当时被报告没电。因此复制的旧回退镜像不能证明设备现在状态，连接后需重新只读检查序列号、slot、分区大小。

### 有需要再复制：完整离线复现资料

- `device/`、`uefi-tb375fc/` 的原始输入和解析证据：`inputs/`、`dtbo-entries/`、`hardware-live/`、`device/evidence/`、`boot-and-dtbo-report.json`、`tb375fc-dtb-inventory.json`、`vendor-boot-v4016-report.json`。
- 若要复现某次旧试验，复制对应测试目录的 `qualification.json`、`hardware-result.json` 和必要的日志；日志常带机器路径，发给他人前先审阅。
- `kernel27-native/kernel.macho`（历史 95,125,504-byte 内核候选，SHA256 `1817068d295e5112749c171fe53e7b1fe217dc8664218d4a4e8d980a41efa24f`）及其 `symbols.txt`、`qualification.json`、日志：只有需要继续 XNU/Image4 分析时复制。它不是 iPadOS 27 安装镜像，也不代表当前 LZMA 启动候选已实机通过。
- 历史 `boot_a`/`init_boot_a` 备份、`vendor_boot`、DTB/DTBO 二进制只在明确需要比较时复制；避免把原厂分区镜像上传到 GitHub。

建议把大文件放在 Mac 项目目录旁的 `local-artifacts/`，而不是提交到 Git：

```text
tb375fc-ipados27-progress/
local-artifacts/
  boot-image4-test.img
  boot_a.stock.img
  rollback-unixnu-previous.img
  kernel.macho                 # 可选
```

## 5. 不需要迁移的东西

- 无需把完整 WSL 虚拟磁盘、`Build/`、LLVM 下载缓存、对象文件或临时输出复制到 Mac；它们占空间且无法作为 macOS 原生构建结果复用。
- 不需要在根卷/Data 镜像阶段之前准备 iPadOS APFS 镜像。当前还没到 UFS/root mount，iPadOS System/Data 镜像未制作。
- 不要把凭据、私钥、ADB 授权私钥或原厂固件塞进仓库。需要下载 Apple 或工具链资源时在 Mac 本地授权和获取。

## 6. 接手顺序

1. 克隆私有快照，读 `README.md` 和 `docs/HANDOFF-2026-11-08-COMPLETE.md`。
2. 先从 Windows 复制当前 LZMA boot 候选和两份不同用途的回退镜像，验证 SHA256。
3. 获取指定 UniXNU 基线，按本指南恢复补丁，记录 macOS、Xcode/CLT、Clang、Python 版本和依赖提交。
4. 先做轻量解析/源码检查，再评估 MacBook 12 是否能承受完整构建。若耗时或内存不足，Mac 继续用于开发，通过远程 Linux 构建机生成产物。
5. 有设备测试时一次验证一个阶段：新版镜像先到 UniXNU 菜单并确认 payload 项，再单独测 DarwinBoot/XNU。记录 SHA、刷写分区、设备身份、画面/日志和回退结果。
6. XNU 到达 BSD 后才继续 UFS、真实盘读取、IOKit、APFS root、Data、用户态和可见 UI。当前这些都未完成。

## 7. 当前不确定项

- 此快照没有完整记录隔离 WSL 的 EDK2、Mu-Silicium、LLVM/Clang、Python 精确版本和全部 Git/submodule commit。迁移前应从原机可用目录和日志补录，不要猜版本。
- 当前 LZMA 镜像的测试结果只到主机构建/打包；Mac 上重新构建不等于该文件实机可启动。
- 目标为 iPadOS 27；arm64e 完整资格、MT6897 上游支持、UFS、root/Data 和用户态均有未完成项。详见完整 handoff。

# 小米 14 OSS 内核构建修复记录

工作目录（Ubuntu 虚拟机）：`/home/zeus/lineage-houji/kernel-work/houji-oss`。

本轮已构建 Linux 6.1.130 Image、内核树内模块、设备配置的 30 组外置驱动，以及额外接入的 houji 触屏和 CS35L41 功放。产物仅用于后续适配验证，不是可刷入的 boot.img 或完整 ROM。

## 固定源码

详见 `source-manifest.json`：

- 内核：lolipuru/android_kernel_xiaomi_sm8650，e，`24dd31e4b1fecb6f82a6c82fa456d585f37c3551`。
- 模块：lolipuru/kernel_xiaomi_sm8650-modules，`4a31d6ee77b27c1b93d329de7bf4cbe16e699599`。
- 设备树：lolipuru/kernel_xiaomi_sm8650-devicetrees，lineage-22.2，`2675a05b30296e05bee91bfd1a86550e21b973fd`。
- 工具链：现有 Android kernel workspace 的 Clang r487747c 和 kernel-build-tools。工具链完整 manifest 见先前 `vm/artifacts/kernel-6.1.176-20260911/` 保存的记录。

小米官方 `MiCode/Xiaomi_Kernel_OpenSource` 的 `shennong-u-oss` 是小米 14/14 Pro 的 Android 14、6.1.25 基线，本轮没有声称重现官方 HyperOS 3 二进制。

## 最小修复

补丁保存在 `patches/`，分别应用到对应源码根目录。

- USB notifier：从控制器获取 hcd 并检查空指针，解决未初始化变量。
- 充电驱动：补齐函数内已有逻辑所需的局部变量声明。
- IPA：共享头文件末尾追加 NCM 协议值 6，保留原有 0～5 编号，并重编内核和模块。这是待真机验证的兼容补丁，未声称已追溯到匹配版本的上游提交。
- 视频：补齐 Iris2 缓冲计算使用的局部 HFI_MIN/HFI_MAX 宏。
- 触屏：修复 houji 配置头文件名、源文件列表续行、SPI 类型头文件，标注当前未使用的辅助函数；构建时选择 houji，避免通用 volcano 配置覆盖。
- 功放：启用 CS35L41，并显式使用厂商版头文件，避免与主线同名头文件冲突。
- 设备树：修正音频节点标签引用和无效禁用引用；从原 lolipuru DTBO 核对 GPIO53 音频切换引脚状态并补齐。来源摘要在 `reference-dt/provenance.json`。

## 复现命令

在同一 Ubuntu 虚拟机和既有工具链环境中：

```bash
cd /home/zeus/lineage-houji/kernel-work/houji-oss
bash build-kernel.sh
bash build-modules-with-touch.sh
python3 audit-built-modules.py --wifi kiwi_v2
python3 audit-built-modules.py --wifi qca6750
python3 build-devicetrees.py "$PWD" /home/zeus/lineage-houji/kernel-work/android14-6.1
```

新源码目录需按 manifest checkout 三份源码，保持 `kernel/sm8650`、`kernel/sm8650-modules`、`kernel/sm8650-devicetrees` 的相邻布局，分别 `git apply` 对应补丁，再将脚本放到工作目录运行。内核自带的 DTS vendor 符号链接依赖此布局。`build-modules.sh` 是较早的 30 组探测脚本；最终使用 `build-modules-with-touch.sh`。

构建脚本固定内核构建时间、用户、主机和相机编译元数据，但尚未做两个全新目录的逐字节重现比较。这里的“复现”表示保存了版本、修复和构建步骤，不等于已证明 bit-for-bit reproducible。

## 验证与边界

- 最终模块构建状态：`logs/modules/status.tsv`。
- 模块版本与 CRC：`out/module-audit-kiwi_v2.json`、`out/module-audit-qca6750.json`。两套 Wi-Fi 驱动按互斥组合审计，不能同时加载它们的同名导出接口。
- 设备树：10 个输入编译成功，两种基础 DTB 与所选 overlay 的组合检查成功；`combined-*.dtb` 是检查产物，不是已完成板型筛选和打包的刷机镜像。
- 与旧预编译模块按名称比较：`out/coverage-vs-prebuilt.json`。当前仍有 59 个旧模块名未生成；其中有小米定制模块和其他硬件驱动。名称差异不自动等于全部必需，但必须在替换前核对加载清单、设备树和实际硬件。
- 原设备配置缺失的 `vendor/xiaomi/sm8650_GKI.config` 未伪造；当前脚本明确使用存在的三个配置片段。部分 houji 配置项无 Kconfig 定义，不能据此声称所有小米功能已启用。
- 原设备树缺失的 5 份模块加载/屏蔽清单尚未接回 LineageOS 完整构建。
- 本轮没有替换现有 ROM、没有刷入手机，也没有验证启动、触屏、指纹、相机、功放、NFC、充电等真机功能。

下一步应优先核对 59 个差异模块中的实际必需项，补齐来源和配置，再生成与本内核一致的模块加载清单、DTB/DTBO 和启动镜像。

## Archive limitations

Source-only archive. The reference-dt/provenance.json, extracted reference DTBO and local build logs are not included. Historical paths describe the original workstation; adjust toolchain paths for another machine. This is not an installable kernel release.

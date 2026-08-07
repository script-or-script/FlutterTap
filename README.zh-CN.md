<p align="center">
  <img src="docs/assets/banner.png" alt="FlutterTap — 用于拦截 Flutter 应用流量的 Zygisk 模块">
</p>

<p align="center">
  <a href="README.md">English</a> ·
  <a href="README.pt-BR.md">Português (BR)</a> ·
  <b>中文</b>
</p>

一个 **Zygisk** 模块，可将所选 **Flutter** 应用的网络流量重定向到可配置的代理，并绕过 BoringSSL 的 TLS
证书校验 —— 无需在设备上安装证书、无需重新打包应用，也不需要通过 USB 保持 Frida 会话。

附带一个 **管理应用**（Jetpack Compose），可直接在手机上选择目标应用并设置代理 IP 与端口。

<p align="center">
  <img src="docs/screenshots/pixel8a-android17/5-bypass-app2.png" alt="Flutter 应用的 HTTPS 流量在 Burp Suite 中被捕获并解密">
</p>
<p align="center">
  <em>Flutter 应用的 HTTPS 流量以明文抵达 Burp —— 设备上并未安装任何 CA 证书。</em>
</p>

## 为什么需要它

Flutter 应用不使用 Android 的 TLS 栈：引擎内置了自己的 **BoringSSL**，并维护独立的信任链。这意味着把
Burp 的 CA 证书装进系统**根本拦截不到任何东西** —— 应用完全忽略 Android 的证书存储。而且 Flutter 也不遵循
Wi-Fi 上配置的代理，因此流量会直接离开设备，根本不经过你的拦截器。

绕过它的技术已为人所知，但通常需要运行 Frida 脚本，并且每次都要启动 `frida-server`、连着数据线。
FlutterTap 把同样的思路做成了一个常驻模块：安装一次、选好应用，之后每次开机都会自动生效。

## 为什么用模块，而不是 Frida 或 LSPosed

**Frida** 在*探索*应用行为方面无可替代 —— 改一行 JavaScript，几秒内就能看到效果，这项技术最初也正是这样
诞生的。问题在于*长期使用*：它依赖 `frida-server` 与活动会话（关掉终端，Hook 就没了），重启后不复存在，
要拦截启动阶段的调用还必须用 spawn（`-f`）；而在真实评估中最关键的是，它**会开放一个监听端口，并留下众所
周知的痕迹**（线程名、内存中的字符串、`/proc` 条目）。检测 Frida 几乎是任何移动安全防护 SDK 做的第一件事。

**LSPosed/Xposed** 的问题不是便利性，而是层级：它 Hook 的是 **Java/ART 方法**。而在 Flutter 中，
`verify_cert_chain` 与 `GetSockAddr` 是 **`libflutter.so` 内部未导出的原生函数** —— 根本没有可供拦截的
Java 方法。一个 LSPosed 模块仍然要做与 FlutterTap 完全相同的原生工作，却还要额外背上整个 Xposed 运行时。

|  | Frida | LSPosed | FlutterTap |
|---|---|---|---|
| 能触及原生 BoringSSL | 能 | **不能直接** | 能 |
| 重启后依然有效 | 否 | 是 | 是 |
| 无需数据线／外部工具 | 否 | 是 | 是 |
| 监听端口／外部进程 | 有（易被检测） | 无 | 无 |
| 从应用启动的第一刻起生效 | 仅手动 spawn | 是 | 是 |
| 对未选中的应用零影响 | — | 取决于模块 | 是（`DLCLOSE`） |
| 开发迭代速度 | **极快** | 中等 | 慢 |

最后一行是一个诚实的缺点：迭代原生模块远比改一个 `.js` 慢。这也正是这种分工合理的原因 ——
**用 Frida 去发现，用模块去运行**。

### 创建它的原因

动机来自真实的移动流量分析工作。在实际评估中，应用需要**在一段时间内自然地运行**：反复开关、接收推送、
在后台同步，甚至由完全不知道 Frida 是什么的人来操作。而整套环境还必须是**可复现的** —— 任何人勾几个复选框
就能重新配置，不需要终端，也不需要记命令。

FlutterTap 把一个手工且脆弱的流程变成了**基础设施**。按应用选择目标正是为此而设计：它不是全局拦截器 ——
对于你没有选中的应用，它会立即从内存中卸载自己，从而同时降低副作用风险与被检测面。

## 工作原理

涉及的函数都没有导出，因此必须在每个进程中于运行时定位：

1. 模块通过 Zygisk 载入每个应用进程，检查该包名是否在目标列表中 —— 若不在，立即卸载自身（对未选中的应用
   零占用）。
2. 在一个监控线程中等待 `libflutter.so` 载入，并解析其 ELF 段。
3. 在内存中扫描字符串 `"ssl_client"` 与 `"Socket_CreateConnect"`，并据此**使用 Capstone 反汇编**定位
   `verify_cert_chain` 与 `GetSockAddr` 的真实地址 —— Capstone 正是 Frida 底层所用的同一个库。
4. 使用 Dobby 安装三个 Hook：捕获目标 `sockaddr`、将 IP/端口改写为代理地址，并强制返回“证书有效”。

实现细节，以及与原始方案的差异及其原因，见 [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)。

## 兼容性

| 项目 | 支持情况 |
|---|---|
| Android | 10（API 29）至 17（API 37） |
| 架构 | `arm64-v8a`、`x86_64` |
| Root | Magisk、KernelSU、SukiSu Ultra、APatch |
| Zygisk | Magisk 内置 Zygisk、Zygisk Next（**包括启用 Zygisk Next Linker 时**）或 NeoZygisk |

已在两套差异明显的真机环境中验证：

- **OnePlus 5** —— Android 10，Kitsune Magisk v27.2 + NeoZygisk 2.3
- **Pixel 8a** —— Android 17，SukiSu Ultra + Zygisk Next 1.4.3，并**启用 Zygisk Next Linker**

> **必须启用 Zygisk。** 仅有 Root 是不够的：整套机制都运行在 Zygisk 的回调中。在 Magisk 中请打开
> “Zygisk” 选项；在 KernelSU/SukiSu Ultra 上请安装 Zygisk Next 或 NeoZygisk；APatch 自带兼容实现。
>
> 与 **Zygisk Next Linker** 的兼容需要专门的修复。如果你在维护一个会被该功能破坏的 Zygisk 模块，
> [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) 中的 “Zygisk Next Linker compatibility” 一节记录了
> 发现的三个陷阱（以及一条曾浪费大量时间的错误线索），可直接复用。

## 安装

**推荐 — 通过 MMRL：** 在 [MMRL](https://mmrl.dev/) 中打开 *Repositories → Add*，粘贴下面的仓库地址。FlutterTap 即可一键安装，并自动获得更新通知：

```
https://raw.githubusercontent.com/script-or-script/FlutterTap-mmrl/main/json/modules.json
```

**或手动安装：** 请从[最新 release](../../releases/latest) 下载两个文件。

1. **安装模块**：在你的 Root 管理器（Magisk / KernelSU / SukiSu Ultra / APatch）中刷入
   `FlutterTap-<版本>.zip`，然后重启设备。
2. **安装管理应用**（`FlutterTap-manager-<版本>.apk`），首次启动时授予 Root 权限。
3. **进行配置**：填入代理的 IP 与端口（运行 Burp 的那台机器的 IP，需与手机处于同一 Wi-Fi），并勾选要
   拦截的应用。
4. **强制停止目标应用，然后重新打开。** Hook 只会进入新创建的进程 —— 仅仅切回应用是不够的。

> **管理应用同样需要 Root**，而不仅仅是保存配置时需要：配置位于 `/data/adb/` 下，没有 Root 连已保存的
> 内容都读不到。

在 Burp 中，请使用启用了 **invisible proxying**（隐形代理）的监听器，因为应用发送的是普通请求，而不是
面向代理的请求。

### 分步骤实证

以下为 Pixel 8a（Android 17 + SukiSu Ultra）上的真实截图，按实际操作顺序排列。

**1. 环境与模块安装**

<p align="center">
  <img src="docs/screenshots/pixel8a-android17/1-device.png" alt="Pixel 8a 上的 Android 17、运行中的 SukiSu Ultra、安装日志以及已启用的模块">
</p>

Pixel 8a 上的 Android 17、以 LKM 模式运行的 SukiSu Ultra、以 *Module installed successfully* 结尾的安装
日志，最后是模块列表中已启用的 FlutterTap —— 并且已经出现 **Action** 按钮，可直接从这里打开管理应用。

**2. 授予 Root 与配置代理**

<p align="center">
  <img src="docs/screenshots/pixel8a-android17/2-gerenciador-config.png" alt="Root 被拒绝、在 SukiSu Ultra 中手动授予、以及配置完成的应用">
</p>

首次启动时应用可能显示 **“Root access denied”**：KernelSU 系的管理器会按应用记住这一决定，并且不会再次
弹窗。解决办法就写在界面上 —— 打开 SukiSu Ultra，进入 **应用配置（App Profile）**，手动打开
**超级用户（SuperUser）**。之后应用便会显示 *Root access granted*、*Module installed* 与 *Enabled*，
此时即可保存真实的代理 IP 与端口。

**3. 准备 Burp**

<p align="center">
  <img src="docs/screenshots/pixel8a-android17/3-invisible-proxy.png" width="620" alt="启用了隐形代理的 Burp 监听器">
</p>

监听器必须勾选 **Support invisible proxying**（位于 *Request handling* 选项卡）。否则 Burp 会丢弃这些
连接：应用发送的是普通 HTTP 请求，而不是发往代理的请求。

**4. 端到端验证 —— 普通 HTTP**

<p align="center">
  <img src="docs/screenshots/pixel8a-android17/4-bypass-app1.png" alt="选中 VulnApp、触发请求，并在 Burp 中捕获流量">
</p>

将 **VulnApp** 设为目标后，在设备上触发的请求会出现在 Burp 中，并且**抵达 8083 端口** —— 也就是监听器的
端口，这证明重定向确实生效了。`user-agent: Dart/3.12 (dart:io)` 则确认请求来自 Flutter 自身的 HTTP
客户端。

**5. 端到端验证 —— 解密后的 HTTPS**

<p align="center">
  <img src="docs/screenshots/pixel8a-android17/5-bypass-app2.png" alt="Ostorlab Insecure App 的 HTTPS 流量在 Burp 中被解密">
</p>

对 **Ostorlab Insecure App** 做同样的操作，这次走 **HTTPS**：Burp 显示 `https://ostorlab.co` 返回
**HTTP/2 200**，且 HTML 为明文。这正是 Pinning 绕过所带来的结果 —— 而**设备上并未安装任何 CA 证书**。

### 不使用管理应用进行配置

管理应用只是单个 JSON 文件的前端。若需自动化（批量部署、CI、无屏设备），可直接写入
`/data/adb/fluttertap/config.json`：

```json
{
  "enabled": true,
  "proxy_ip": "192.168.1.10",
  "proxy_port": 8080,
  "target_packages": ["com.example.app"]
}
```

每有新进程启动时，该文件都会从磁盘重新读取，因此只要对目标应用执行 `am force-stop` 再打开即可 ——
无需重启设备。匹配依据是进程名，且填写 `com.example.app` 同样会命中具名子进程
（`com.example.app:remote`）。

## 排查

```sh
adb logcat -s FlutterTap:V
```

对于目标应用，预期日志顺序为：`selected for hooking` → `libflutter.so loaded at ...` →
`verify_cert_chain=0x... GetSockAddr=0x...` → `hooks: installed for ... -> proxy IP:PORT`，随后在应用
发起请求时出现 `hooks: overwrite sockaddr` / `hooks: verify_cert_chain bypass`。

如果**什么都没有输出**，通常是以下三种情况之一：包名与进程名不匹配、`config.json` 无效（模块会回退到
默认值，从而什么都不拦截），或者配置更改时应用已经在运行。

## 构建

需要 Android SDK/NDK 以及 **JDK 17 或 21**（Gradle 8.11.1 无法解析 JDK 22+ 的版本号；若系统默认的
`java` 更新，请将 `JAVA_HOME` 指向 Android Studio 自带的 JBR）。

```sh
git clone --recurse-submodules https://github.com/script-or-script/FlutterTap.git
cd FlutterTap
export ANDROID_HOME=/path/to/Android/Sdk
./scripts/build_module_zip.sh          # 生成 dist/FlutterTap-<版本>.zip
./gradlew :manager-app:assembleDebug
```

> 在 Windows 上请克隆到**较短的路径**（例如 `C:\dev\FlutterTap`）。Capstone 的目录嵌套足以超过 260
> 字符限制，否则克隆会以 *"Filename too long"* 失败。

完整说明（包括 release 签名）见 [`docs/BUILD.md`](docs/BUILD.md)。

## 文档

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) —— 各部分如何工作、为何如此设计，包含与 Zygisk Next
  Linker 的兼容性说明
- [`docs/BUILD.md`](docs/BUILD.md) —— 构建、打包与签名

## 免责声明

本项目仅用于**经授权的流量分析** —— 即在你有权测试的应用上进行移动渗透测试、逆向工程与安全研究。未经
授权拦截第三方应用的流量在多数司法管辖区均属违法。

## 许可证

MIT —— 见 [`LICENSE`](LICENSE)。原生模块静态链接了 Dobby（Apache-2.0）与 Capstone（BSD-3）；第三方组件
列于 [`THIRD_PARTY.md`](THIRD_PARTY.md)，完整许可证文本随分发的 `.zip` 一并提供。

---
由 Eduardo Lopes 开发

# DeskMon — DTK 原生系统监控应用 · 开发文档

> **比赛**：Deepin 社区「10亿 Token 奖池，写出属于你的 deepin 桌面插件」
> **方向**：方向三「DTK 创新原生应用」（`dtk-development` skill）
> **作品名**：DeskMon / 桌面监控
> **许可证**：GPL-3.0
> **截止日期**：2026年9月9日 23:59
> **作者**：kookboy
> **创建日期**：2026年8月10日

---

## 一、项目概述

将已有 PyQt5 版系统监控工具（`/home/kookboy/Projects/desktool/system_monitor.py`，1601行）迁移为 **DTK 原生 C++ 应用**，实现 Deepin 桌面原生视觉、主题自动跟随、单实例管理。

### 1.1 原版功能清单（全部保留）
- 悬浮无边框小窗，右下角，可拖动/调整宽度
- CPU 使用率 + 温度
- 内存使用率 + 已用/总量
- GPU 使用率 + 显存占用 + 温度（有 NVIDIA 显卡时）+ 显存压力等级
- 系统盘使用率
- 网络上传/下载速度 + IP
- 系统托盘：显示/隐藏、GPU显存管理、进程管理、设置、透明度、退出
- 配置持久化：`~/.config/deskmon/config.json`（透明度、刷新率、位置、置顶、开机自启动）
- GPU 显存一键释放 / 进程结束

### 1.2 原版代码关键文件
| 文件 | 说明 |
|------|------|
| `system_monitor.py` | 主程序 1601 行：Config / SystemMonitor / MetricWidget / VRAMWidget / NetWidget / GPUProcessDialog / ProcessListDialog / SettingsDialog / MonitorWidget |
| `start.sh` | 启动脚本（venv） |
| `系统监控.desktop` | 桌面快捷方式 |
| `deb-build/` | deb 打包产物 |

---

## 二、环境核查

### 2.1 当前系统状态

| 项 | 状态 | 备注 |
|---|---|---|
| DTK6 运行时 | ✅ 6.7.47 | `libdtk6core/gui/widget/log/declarative` |
| DTK5 运行时 | ✅ 6.7.47 | `libdtkcore5/libdtkgui5` |
| Qt5 运行时 | ✅ | `dde-qt5xcb-plugin` 等 |
| Qt6 运行时 | ✅ | `dde-qt6integration` 等 |
| DTK6 头文件 | ❌ 缺失 | `/usr/include/dtk6/` 为空，无任何 `-dev` 包 |
| Qt6 Widgets 头文件 | ❌ 缺失 | |
| cmake | ✅ 可用 | `/home/kookboy/jm-prefix/usr/bin/cmake`（自定义前缀，非 apt 包；建议另装系统包以备 deb 构建） |
| NVIDIA GPU | ✅ | RTX 4060 Laptop，`nvidia-smi` 正常 |

### 2.2 启动开发前必须安装

> 本方案定为 **仅 DTK6 / Qt6 单版本**（理由见 §5.2），不引入 DTK5 双版本分支以集中精力冲方向三。

```bash
sudo apt update
sudo apt install cmake qt6-base-dev qt6-tools-dev libdtk6core-dev libdtk6gui-dev libdtk6widget-dev
# cmake 系统包可选（当前自定义前缀已可用，装系统包是为了 deb 构建环境一致性）
```

### 2.3 关键 skill 文档路径

| 场景 | 文档 |
|------|------|
| 项目 CMake 配置 | `references/app-dev-with-dtk.md` |
| DTK 源码编译调试 | `references/dtksrc-compile-debug.md` |
| DApplication 入口 | `references/widgets/application.md` |
| DMainWindow 窗口 | `references/widgets/window.md` |
| 进度控件 | `references/widgets/progress.md` |
| 调色板/主题 | `references/theme/palette.md` |
| 系统信息 | `references/utilities/sysinfo.md` |
| 单实例 | `references/utilities/singleton.md` |
| 控件选择 | `references/widgets/index.md` |

---

## 三、为什么选 DTK 迁移

| 维度 | PyQt5 原版 | DTK 新版 |
|------|-----------|---------|
| 视觉 | 自绘深色，不随系统主题 | 原生 Chameleon 皮肤，自动亮/暗主题切换 |
| 窗口 | 自绘无边框，DDE 窗口管理不识别 | DMainWindow + DTK 窗口装饰 |
| 进度条 | 自绘 QProgressBar | `DProgressBar` / `DCircleProgress` |
| 单实例 | 无（会开多个窗口） | `setSingleInstance()` 原生支持 |
| 托盘 | 有但非标准 | DDE 标准系统托盘 |
| 主题 | 固定色 | `DPalette` 语义化，跟随系统活跃色 |
| 评分契合度 | 一般 | **完美契合「DTK 原生应用」方向** |

---

## 四、功能规划

### 4.1 MVP（核心功能，2 周完成）
- [x] 悬浮无边框小窗，右下角，可拖动/调整宽度
- [x] CPU：长条进度 + 使用率 + 温度
- [x] 内存：长条进度 + 已用/总量
- [x] GPU（有显卡时）：使用率 + 显存 + 温度（压力等级待补）
- [x] 系统盘：磁盘进度
- [x] 网络：上/下行速度 + IP
- [x] 系统托盘：显示/隐藏、GPU 显存管理、进程管理、透明度、设置、退出
- [x] 配置持久化：`~/.config/deskmon/config.json`
- [x] 开机自启动

> **无 NVIDIA 时的降级策略**：启动时探测 `nvidia-smi` 是否存在且可执行成功；不可用则隐藏 GPU 面板与「GPU 管理」托盘菜单项，主窗口自动收缩布局，配置中不写入 GPU 相关项。进程管理仍可用（走 `/proc`）。deb 中 `nvidia-smi` 放 `Recommends` 而非 `Depends`，避免无 N 卡用户无法安装。

### 4.2 加分项（冲刺一等奖）
- [x] 🍅 番茄钟面板：25/5 分钟倒计时 + 完成通知（DNotifySender）
- [x] 📈 CPU/GPU 迷你折线图：最近 60 秒趋势（`Sparkline`）
- [x] 🎨 主题自动跟随：亮/暗/活跃色（DPalette）
- [x] 🎮 显存一键释放：`DTableWidget` + `DDialog`
- [x] ⚙️ 原生设置面板
- [x] 🔔 阈值告警：CPU/GPU > 90% 时系统通知（原 DFloatingMessage 在无边框悬浮窗不渲染，改用 `DNotifySender`）

### 4.3 新增（原版没有，新版做）
1. ~~圆形进度环~~ → 已改为**紧凑长条 + 百分比 + 独立配色**（`MetricRow`），更省高度
2. 系统主题自动跟随
3. 单实例 + 双击托盘激活
4. GPU 温度折线趋势（未做）
5. 番茄钟专注计时（未做）

---

## 五、技术架构

### 5.1 目录结构

```
deskmon-dtk/
├── CMakeLists.txt                 # DTK6/Qt6 单版本
├── main.cpp                       # DApplication + setSingleInstance
├── src/
│   ├── config.cpp/.h              # 配置读写 (~/.config/deskmon/)
│   ├── systemmonitor.cpp/.h       # CPU/内存/磁盘/网络/GPU 数据层（/proc+sysfs）
│   ├── nvidia_gpu.cpp/.h          # nvidia-smi 封装 + 降级探测
│   ├── processmgr.cpp/.h          # 进程查询（/proc）+ 终止
│   ├── themecolors.cpp/.h         # 主题跟随的指标配色
│   ├── settings_dialog.cpp/.h     # 原生设置面板（DDialog）
│   ├── process_dialog.cpp/.h      # 进程管理对话框
│   ├── gpu_dialog.cpp/.h          # GPU 显存管理对话框
│   ├── monitorwidget.cpp/.h       # 主悬浮窗口 + 托盘 + 阈值告警
│   └── widgets/
│       ├── metricrow.cpp/.h       # 紧凑指标行（点+名称+百分比+细条）
│       ├── netwidget.cpp/.h       # 网络组件（速度+本机 IP）
│       ├── sparkline.cpp/.h       # CPU/GPU 迷你折线（加分项）
│       └── pomodoro.cpp/.h        # 番茄钟（加分项）
├── icons/
│   └── deskmon.svg                # 矢量图标源
├── deskmon.desktop                # 启动器
└── README.md
```

### 5.2 关键技术决策

| 决策 | 选型 | 依据 |
|------|------|------|
| 语言 | C++17 | DTK 原生命运 |
| 构建 | CMake，仅 DTK6/Qt6 | 集中精力冲方向三；避免双版本分支稀释工期（如后续有需求再补 DTK5） |
| GPU 数据 | `QProcess` 调 `nvidia-smi --query-gpu=...` | 原版 psutil 同逻辑 |
| 进程数据 | 读 `/proc/[pid]` 或 `ps` | Linux 原生 |
| 主题 | `DPalette` + `DGuiApplicationHelper::setPaletteType` + DBus 监听 | 指标色由系统活跃色派生（`themecolors.h`），明暗主题自动调亮度；DTK 默认不跟随 DDE GlobalTheme，需通过 DBus 监听 `org.deepin.dde.Appearance1.Changed` 信号手动同步 `paletteType` |
| 进度控件 | 每项一行 `MetricRow`（`DProgressBar` 细条 + 百分比 + 附加信息） | 实测经验：环形太占高度，长条更紧凑 |
| 单实例 | `DApplication::setSingleInstance("deskmon")` | |
| 窗口 | `DWidget` + `DBlurEffectWidget` 悬浮窗 | **改选**：DMainWindow 面向带标题栏/侧栏标准应用，强加最小尺寸，不适合无边框悬浮小窗；磨砂+圆角由 DBlurEffectWidget 承担 |
| 配置 | JSON 文件（原版兼容） | 复用 `config.json` 格式 |
| 日志 | `qDebug/qInfo`（Qt 原生） | **改选**：jm-prefix 的 DTK6 `DLogManager::registerConsoleAppender()` 会破坏事件循环（QTimer 不再触发），弃用 |

### 5.3 关键 API 速查

```cpp
// 主入口
#include <DApplication>
DApplication app(argc, argv);
app.setSingleInstance("deskmon");
app.setProductName("DeskMon");

// 主题
#include <DPalette>
#include <DGuiApplicationHelper>
DPalette palette = DGuiApplicationHelper::instance()->applicationPalette();
QColor titleColor = palette.color(DPalette::TextTitle);

// DDE GlobalTheme 跟随（DApplication 默认不自动跟随，需手动同步）
#include <QDBusInterface>
#include <QDBusConnection>
// 1. 启动时同步：读 org.deepin.dde.Appearance1.GlobalTheme/GtkTheme
//    判断含 "dark" → DGuiApplicationHelper::setPaletteType(DarkType/LightType)
// 2. 运行时监听 Changed 信号（ty 是小写 "globaltheme"/"gtk"，需 CaseInsensitive 比较）

// 主题切换监听（setPaletteType 触发，联动更新指标色/分隔线/标签色）
connect(DGuiApplicationHelper::instance(),
        &DGuiApplicationHelper::themeTypeChanged,
        this, &MyWidget::onThemeChanged);

// 系统通知（DNotifySender，无边框悬浮窗可靠）
#include <DNotifySender>
DUtil::DNotifySender("DeskMon 告警")
    .appName("DeskMon").appIcon("deskmon")
    .appBody("CPU 超过阈值").timeOut(3000).call();

// 属性动画（番茄钟呼吸脉动）
#include <QPropertyAnimation>
auto *anim = new QPropertyAnimation(icon, "pulseScale", this);
anim->setDuration(1000);
anim->setStartValue(1.0); anim->setEndValue(1.12);
anim->setEasingCurve(QEasingCurve::InOutSine);

// 窗口效果（确切 API 以 references/widgets/blur-effect.md 为准）
// setWindowRadius / setEnableBlurWindow / setTranslucentBackground
//   的所属类与可用性需在实现前查 blur-effect.md 核对，勿照抄

// 单实例新进程
connect(&a, &DApplication::newInstanceStarted, this, [this]() {
    raise(); activateWindow();
});
```

---

## 六、开发计划

| 周 | 时间 | 内容 | 里程碑 |
|---|------|------|--------|
| **第1周** | 8/10-8/16 | 装编译工具链 + 搭 CMake 工程 + 数据层（CPU/内存/磁盘/网络/GPU）+ 主悬浮窗跑通 | 能跑起来显示 CPU 数据 |
| **第2周** | 8/17-8/23 | 原生控件重写全部指标 + 系统托盘 + 进程/显存管理 + 配置持久化 + 自启动 | 功能完整 MVP |
| **第3周** | 8/24-8/30 | 番茄钟 + 迷你折线 + 主题跟随 + 告警 + 图标设计 + deb 打包 | 冲刺加分项 |
| **打磨** | 8/31-9/7 | 录演示视频、截图、写文档、存 AI 对话截图、跑自测 checklist | 备赛材料 |

### 6.1 自测验收 checklist（打磨周必跑一遍）

- [ ] 亮/暗主题切换：面板文字、进度环、活跃色实时跟随，无硬编码色残留
- [ ] 单实例：第二次启动激活已有窗口而非新开
- [ ] 托盘菜单：显示/隐藏、GPU 管理、进程管理、设置、透明度、退出逐项可用
- [ ] 无 N 卡降级：临时改名 `nvidia-smi` 后启动，GPU 面板与「GPU 管理」菜单项应自动隐藏，窗口布局收缩正常
- [ ] 配置读写：改透明度/刷新率/位置后退出重启，值应恢复
- [ ] 开机自启：重启登录后自动出现
- [ ] deb 安装/卸载：干净安装无报错；卸载不留残留配置外的文件
| **提交** | 9/9 前 | 论坛发帖 `【deepin插件开发活动】DeskMon` | ✅ 参赛 |

---

## 七、AI 辅助环节（活动硬要求）

> 比赛要求：**基于 deepin Skills 完成开发，并在材料里说明用了哪些 Skill 模块**，并附「AI 编程工具中调用 deepin Skills 的对话记录截图」。
> Skills 仓库：https://github.com/linuxdeepin/deepin-skills

### 7.1 使用的 Skill 模块清单

DeskMon 定位为 DTK 原生桌面应用（方向三），全部开发基于 **`dtk-development`** 这一个 Skill 模块完成（仓库另有 `dde-shell-development` / `dde-control-center-development` / `dde-tray-development` 三个面向 Shell/控制中心/托盘插件的项目，本作品不是插件、未使用）。

`dtk-development` 模块下实际调用并落地的 references 文档如下（每项均可在源码中核对）：

| references 文档 | 对应DeskMon 功能 | 代码位置 |
|------|------|------|
| `references/widgets/application.md` | `DApplication` 入口、`setSingleInstance` 单实例、`newInstanceStarted` 激活已有窗口 | `main.cpp:6,62,72,89` |
| `references/widgets/window.md` | 主悬浮窗选型 `DWidget`（放弃 `DMainWindow`，因强加最小尺寸不适合无边框小窗） | `src/monitorwidget.h:29-33`、`src/monitorwidget.cpp:85` |
| `references/widgets/blur-effect.md` | `DBlurEffectWidget` 承担磨砂+圆角、`AutoColor` 遮罩跟随主题 | `src/monitorwidget.cpp:21,122,128,130` |
| `references/widgets/progress.md` | `DProgressBar` 紧凑指标条（`MetricRow`） | `src/widgets/metricrow.h:8,35`、`src/widgets/metricrow.cpp:51` |
| `references/widgets/dialog.md` | `DDialog` 原生设置面板 / GPU 显存管理 / 进程管理对话框 | `src/settings_dialog.cpp:20-31`、`src/gpu_dialog.cpp:23-30`、`src/process_dialog.h:7` |
| `references/widgets/view.md` | `DTableWidget` 进程表 / 显存占用表 | `src/gpu_dialog.h:8,38`、`src/process_dialog.cpp:70` |
| `references/theme/palette.md` | `DPalette::Highlight` 派生各指标配色，`DPaletteHelper` 语义化设色 | `src/themecolors.cpp:6-15`、`src/widgets/metricrow.h:18` |
| `references/theme/theme-switch.md` | `themeTypeChanged` 联动刷新指标色/分隔线/标签色 | `src/monitorwidget.cpp:95` |
| `references/utilities/gui-helper.md` | `DGuiApplicationHelper::setPaletteType(DarkType/LightType)` 主动同步明暗 | `main.cpp:32,36`、`src/themecolors.cpp:13` |
| `references/utilities/dbus.md` | 监听 `org.deepin.dde.Appearance1.Changed` 信号补 DTK 不自动跟随 GlobalTheme 之缺陷 | `main.cpp:18-20,81-83` |
| `references/utilities/desktop-services.md` | `DUtil::DNotifySender` 番茄钟完成通知 / CPU·GPU 阈值告警 | `src/widgets/pomodoro.cpp:194,206`、`src/monitorwidget.cpp:559` |
| `references/app-dev-with-dtk.md` | CMake 工程骨架、`find_package(Dtk6Core/Gui/Widget)`、`debian/control` 模板 | `CMakeLists.txt:17-20`、`debian/control` |
| `references/dtksrc-compile-debug.md` | jm-prefix 工具链断链重定向、`DObject`/`DThemeManager` 转发头补齐、`DLogManager` 破坏事件循环的定位 | 见 §环境备忘 + §8.2 排错 |

### 7.2 需附对话截图的环节（按活动硬要求）

1. 悬浮窗 + 磨砂圆角（`widgets/window.md` + `blur-effect.md`）
2. 主题跟随实现（`theme/palette.md` + `utilities/gui-helper.md` + `utilities/dbus.md`）
3. 进度条 / 对话框 / 表格控件选型（`widgets/{progress,dialog,view}.md`）
4. 单实例 + 系统通知（`widgets/application.md` + `utilities/desktop-services.md`）
5. deb 打包配置（`app-dev-with-dtk.md` 的 `debian/control` 模板）
6. 编译排错（DTK6/Qt6 头文件缺失、CMake 找包失败、`DLogManager` 破坏事件循环）

---

## 八、deb 打包

参考 skill `app-dev-with-dtk.md` 的 `debian/control` 模板：

```
Source: deskmon
Build-Depends:
 cmake,
 debhelper-compat (= 13),
 pkg-config,
 qt6-base-dev,
 qt6-tools-dev,
 libdtk6core-dev,
 libdtk6gui-dev,
 libdtk6widget-dev,

Package: deskmon
Architecture: any
Depends:
 ${misc:Depends},
 ${shlibs:Depends},
Recommends:
 nvidia-smi,
```

> `nvidia-smi` 放 `Recommends` 而非 `Depends`：有 N 卡的用户默认装上获得 GPU 面板，无 N 卡的用户不会被硬依赖阻塞安装（启动时自动降级隐藏 GPU 面板，见 §4.1）。

安装路径：
- 程序：`/usr/bin/deskmon`
- 桌面文件：`/usr/share/applications/deskmon.desktop`
- 图标：`/usr/share/icons/hicolor/{scalable,256x256,48x48}/apps/deskmon.{svg,png}`

**图标构建**：源文件为 `icons/deskmon.svg`。构建期用 `rsvg-convert`（或 `inkscape`）生成多尺寸 PNG，在 `CMakeLists.txt` 中用 `add_custom_command` 生成、`install(FILES ...)` 落地：

```cmake
# 生成多尺寸 PNG（rsvg-convert 未装时回退 inkscape 或仅装 SVG）
find_program(RSVG_CONVERT rsvg-convert)
if(RSVG_CONVERT)
  foreach(size 16 22 24 32 48 64 128 256)
    set(out ${CMAKE_CURRENT_BINARY_DIR}/icons/${size}/deskmon.png)
    add_custom_command(OUTPUT ${out}
      COMMAND ${CMAKE_CONVERT} -w ${size} -h ${size}
              ${CMAKE_SOURCE_DIR}/icons/deskmon.svg -o ${out}
      DEPENDS ${CMAKE_SOURCE_DIR}/icons/deskmon.svg)
    list(APPEND icon_pngs ${out})
  endforeach()
  add_custom_target(deskmon_icons ALL DEPENDS ${icon_pngs})
  install(FILES ${icon_pngs}
          DESTINATION /usr/share/icons/hicolor/256x256/apps
          RENAME deskmon.png OPTIONAL)
endif()
# 矢量版始终安装
install(FILES icons/deskmon.svg
        DESTINATION /usr/share/icons/hicolor/scalable/apps
        RENAME deskmon.svg)
```

### 8.1 实际打包步骤（已跑通）

> 本机工具链在 `~/jm-prefix/usr`（Qt6.8 + DTK6.7.44，非 apt 包），系统未装 dev 头文件，故用 `-d` 跳过 `dpkg-checkbuilddeps`。

```bash
cd deskmon-dtk
# 构建（fakeroot 需关闭 sandbox，故脱离 ZCode 沙箱执行）
dpkg-buildpackage -b -us -uc -d
# 或仅重打 binary 阶段（已有 build stamp）
fakeroot debian/rules binary
# 产物在父目录，归档到 dist/
mkdir -p dist && cp ../deskmon_*.deb dist/
```

### 8.2 排错备忘

- **`debhelper compat level specified both in debian/compat and debian/control`** → 删 `debian/compat`，只保留 `control` 里的 `debhelper-compat (= 13)`（debhelper ≥ 13 要求）。
- **`cmake: error while loading shared libraries: librhash.so.0`** → jm-prefix 自带 cmake 需要自己的运行库，`debian/rules` 顶部 `export LD_LIBRARY_PATH` 指向 jm-prefix lib。
- **`fakeroot ... libfakeroot-sysv.so ... cannot be preloaded` + `dh_testroot: You must run this as root`** → fakeroot 通过 `LD_LIBRARY_PATH=PATHS` 把 `libfakeroot/` 目录传给子进程做 LD_PRELOAD；**不能在 rules 里 `:=` 覆盖 `LD_LIBRARY_PATH`**，否则丢掉 fakeroot 注入的路径导致 preload 失效。修法是 `:=` 前先用 `$(if $(LD_LIBRARY_PATH),:$(LD_LIBRARY_PATH))` 追加保留父进程值。

---

## 九、待办清单

- [x] 编译工具链（jm-prefix 已含 Qt6.8 + DTK6.7.44；已修复断链 + 补齐 `DObject`/`DThemeManager` 转发头）
- [x] 确认 DTK6 头文件可用
- [x] 搭 CMake 工程骨架 + `main.cpp` 跑通
- [x] 实现数据层（`SystemMonitor` 移植：CPU/内存/磁盘/网络/GPU）
- [x] 实现 `MetricRow` / `NetWidget` 控件（原 `MetricRing`/`MetricBar` 已由 `MetricRow` 取代）
- [x] 实现主悬浮窗口 `MonitorWidget`（拖动/改宽/磨砂圆角/单实例）
- [x] 实现系统托盘（显示隐藏/透明度/设置/退出）
- [x] 实现 GPU / 进程管理对话框（`ProcessMgr` / `ProcessDialog` / `GpuDialog`，含一键释放与结束进程）
- [x] 实现设置面板 + 配置持久化 + 开机自启动 + 透明度可调
- [x] 实现番茄钟（加分项，托盘可切换显隐）
- [x] 实现迷你折线图（加分项，CPU/GPU 60s 趋势）
- [x] 实现主题自动跟随（加分项）
- [x] 实现阈值告警（加分项，边沿触发 + 系统通知）
- [x] 设计矢量图标（`icons/deskmon.svg` + `deskmon-tray.svg` + `deskmon-symbolic.svg`；多尺寸 PNG 已生成）
- [x] 快速启动脚本（`start.sh`：build/run/clean 三模式）
- [x] 标题栏隐藏按钮（✕ 隐藏到托盘）
- [x] deb 打包（`debian/` 源码包 + `dpkg-buildpackage -b`；输出 `dist/deskmon_1.0.0-1_amd64.deb`，`Installed-Size: 345 KB` 由 `dpkg-gencontrol` 自动计算；`Recommends: nvidia-smi` 软依赖；依赖由 `dh_shlibdeps` 自动生成 `libdtk6* / libqt6*`）
- [ ] 录演示视频、截图
- [ ] 论坛发帖提交

---

## 十、开发进度日志

### 2026-08-10（首日开工）· 里程碑：MVP 跑通 + 全部加分项
- **工程骨架**：CMake + main.cpp + 数据层（SystemMonitor/NvidiaGpu/Config）+ 控件（MetricRow/NetWidget）+ 主悬浮窗（拖动/改宽/磨砂圆角/单实例）+ 托盘 + 配置持久化 + 开机自启
- **环境修复**：jm-prefix 工具链 5 个断链重定向、补齐 `DObject`/`DThemeManager` 转发头
- **关键 bug 定位**：`DLogManager::registerConsoleAppender()` 会破坏事件循环（QTimer 不再触发），二分定位后弃用，改用 Qt 原生日志
- **选型修正**：环形控件太占高度 → 紧凑长条 `MetricRow`；`DMainWindow` → `DWidget` + `DBlurEffectWidget`
- **功能扩展**：
  - 透明度可调（托盘子菜单 + 设置滑条）
  - 主题跟随（指标色由系统活跃色派生，`themeTypeChanged` 实时更新）
  - 原生设置面板（透明度/刷新间隔/显隐/置顶/自启）
  - 进程管理对话框（/proc 数据 + 排序 + 结束进程 + DDialog 确认）
  - GPU 显存管理对话框（显存进程表 + 一键释放）
  - 迷你折线图（CPU/GPU 60s 趋势）
  - 番茄钟（25/5 自动轮换 + 系统通知）
  - 阈值告警（CPU/GPU > 90% 边沿触发 + 系统通知；原 DFloatingMessage 在无边框悬浮窗不渲染，改用 DNotifySender）
  - 网络区域只显示本机 IP（默认路由接口，过滤 docker 桥接）
  - **主题跟随修复（第一轮）**：`DBlurEffectWidget` 遮罩改 `AutoColor`（之前固定深色遮罩，在亮色主题系统上不跟随）；标题圆点/折线颜色随活跃色实时更新
  - **UI 美化（第一轮）**：标题区精简（活跃色圆点+加粗应用名）、折线渐变填充、进度条加粗、间距优化
- **代码量**：约 2700 行（27 个源文件，含 CMakeLists）

### 2026-08-10（同日二更）· UI 精细化 + 主题跟随深度修复
- **UI 美化（第二轮，6 项）**：
  - **模块分区**：标题/指标区、指标区/折线图、网络区/番茄钟间增加 3 条 `DPalette::FrameBorder` 色分隔线，边距统一 `16,14,16,12`，间距收紧至 6px
  - **字体层级**：`MetricRow` 指标名与百分比加粗，附加信息改用 `DPalette::TextTips`（自动跟主题）；百分比与信息间距加宽至 4px
  - **折线图增强**：`Sparkline` 加圆角背景框 + 边框、Y 轴 0/100 标签、X 轴 -60s/现在 刻度、峰值圆点标记、暗色自适应网格/填充透明度
  - **网络区重构**：`NetWidget` 从「↑ 值 ↓ 值」单行改为「上传 值 / 下载 值」两列等宽 + IP 单独一行，标签用 `TextTips` 色
  - **番茄钟**：按钮缩小 52×24→44×22 / 48×24→40×22；番茄图标运行时呼吸脉动动画（`QPropertyAnimation` pulseScale 1.0↔1.12）
  - **暗色文字修复**：`NetWidget` 标签和 IP 文字在主题切换时通过 `updateLabelColors()` 刷新 `TextTips` 色（之前构造时设固定色，切暗色后不更新）
- **主题跟随深度修复（核心问题）**：
  - **根因**：`DApplication` 默认不会自动跟随 DDE `GlobalTheme` 变化（非商店安装应用尤甚），之前仅靠 `themeTypeChanged` 被动响应，DTK 本身不主动切换
  - **修复**：`main.cpp` 新增 `ThemeSyncHelper` + `appearanceIsDark()` + `syncThemeToAppearance()`：
    - 启动时通过 DBus 读 `org.deepin.dde.Appearance1.GlobalTheme`/`GtkTheme`，调 `DGuiApplicationHelper::setPaletteType()` 同步一次
    - 监听 `Changed` 信号（`ty` 参数是小写 `"globaltheme"`/`"gtk"`，需 `compare(CaseInsensitive)`），主题切换时实时更新 `paletteType`
  - **验证**：浅色↔暗色切换实时生效；`MonitorWidget::applyThemeColors()` 已连 `themeTypeChanged`，联动更新指标色、分隔线色、网络标签色
- **代码量**：约 2900 行（新增约 200 行，含主题同步、折线增强、网络重构、番茄动画）

### 2026-08-10（同日三更）· 图标套件 + 启动脚本 + 隐藏按钮
- **图标设计**：
  - **主图标 `deskmon.svg`**：DDE 圆角方形底板（深蓝灰渐变）+ 仪表盘弧形（蓝色渐变进度环）+ 金色闪电（呼应标题栏标志）+ 底部四色点（CPU 蓝/内存 绿/GPU 粉/磁盘 黄）
  - **托盘图标 `deskmon-tray.svg`**：极简单色版，蓝色弧线 + 金色闪电，适合 22×22 任务栏
  - **Symbolic 图标 `deskmon-symbolic.svg`**：单色剪影版，`currentColor` 自动跟随主题前景色，暗色/通知场景
  - **多尺寸 PNG**：用 `rsvg-convert` 生成主图标 8 尺寸（16~256）、托盘 4 尺寸、symbolic 4 尺寸
  - **托盘图标加载**：三级降级 — 主题图标 → 本地 PNG 多尺寸 → 程序绘制回退
- **快速启动脚本 `start.sh`**：
  - `./start.sh` / `build` — 增量编译后启动（默认）
  - `./start.sh run` — 跳过编译直接启动
  - `./start.sh clean` — 清理 build 后全量编译并启动
  - 自动设 `LD_LIBRARY_PATH`，启动前 `pkill` 旧实例避免单实例冲突
- **标题栏隐藏按钮**：标题行新增 ✕ 按钮，点击隐藏窗口到托盘（托盘左键唤回），与 ◐ 透明度按钮并排
- **代码量**：约 2950 行

### 待办（下一阶段）
- [ ] deb 打包（`debian/` + 多尺寸 PNG 图标安装规则）
- [ ] 备赛材料：演示视频、截图、AI 对话记录存档
- [ ] 论坛发帖提交
# 【deepin 插件开发活动】DeskMon —— DTK6 原生桌面系统监控

> Deepin 社区「10 亿 Token 奖池，写出属于你的 deepin 桌面插件」活动参赛作品
> 方向三：DTK 创新原生应用  ·  作者：kookboy  ·  许可证：GPL-2.0-or-later

## 一句话介绍

DeskMon 是一款基于 **DTK6 / Qt6（C++17）** 原生实现的桌面悬浮系统监控小工具，从原 PyQt5 版迁移而来，自动跟随 deepin 亮/暗主题，单实例运行，自带系统托盘与原生 DTK 控件。**不是脚本拼装，是原生 DTK 应用。**

![DeskMon 浅色主题](screenshots/light.png)

![DeskMon 暗色主题](screenshots/dark.png)

## 为什么做

之前用 PyQt5 写过一个系统监控小工具（`system_monitor.py`，1601 行），工作能用但有几个痛点：

| 维度 | PyQt5 原版 | DeskMon 新版 |
|------|-----------|---------|
| 视觉 | 自绘深色，不随系统主题 | 原生 Chameleon 皮肤，亮/暗自动切换 |
| 窗口 | 自绘无边框，DDE 窗口管理不识别 | DTK 原生窗口装饰 |
| 进度条 | 自绘 QProgressBar | `DProgressBar` 语义化配色 |
| 单实例 | 无（会开多个窗口） | `DApplication::setSingleInstance()` |
| 托盘 | 非标准 | DDE 标准系统托盘 |
| 主题 | 固定色 | `DPalette` 跟随系统活跃色 |

正好借这次活动把它升级成「真·DTK 原生应用」，对应方向三。

## 功能清单

### MVP 核心
- 右下角无边框悬浮小窗（可拖动 / 右下角调整宽度）
- CPU 使用率 + 温度
- 内存使用率 + 已用/总量
- GPU 使用率 + 显存 + 温度（有 NVIDIA 卡时）+ 显存压力等级
- 系统盘使用率
- 网络上/下行速度 + 本机 IP（默认路由接口，过滤 docker 桥接）
- 系统托盘：显示/隐藏、GPU 显存管理、进程管理、透明度、设置、退出
- 配置持久化：`~/.config/deskmon/config.json`
- 开机自启动

### 加分项（冲一等奖）
- 🍅 **番茄钟面板**：25/5 分钟自动轮换，完成时 `DNotifySender` 系统通知，运行时番茄图标呼吸脉动动画
- 📈 **CPU/GPU 迷你折线图**：最近 60 秒趋势，渐变填充 + 峰值圆点标记 + 暗色自适应网格
- 🎨 **主题自动跟随**：`DPalette` 语义化配色，指标色由系统活跃色派生，明暗主题自动调亮度
- 🎮 **显存一键释放**：`DTableWidget` 列出显存占用进程 + `DDialog` 确认释放
- ⚙️ **原生设置面板**：透明度滑条 / 刷新间隔 / 指标显隐 / 窗口置顶 / 开机自启
- 🔔 **阈值告警**：CPU/GPU > 90% 边沿触发 + 系统通知（`DNotifySender`，悬浮窗隐藏时也能提醒）

### 原版没有、新版做的
- 紧凑长条指标行 `MetricRow`（点 + 名称 + 百分比 + 细条 + 附加信息）替代占高度的环形
- DDE GlobalTheme DBus 监听，补 DTK 默认不跟随主题之缺陷
- 单实例 + 双击托盘激活
- 全套矢量图标（主图标 / 托盘 / symbolic，多尺寸 PNG）

## 技术栈与关键 API

| 层 | 选型 |
|----|------|
| 语言 | C++17 |
| 框架 | DTK6 + Qt6（仅 DTK6 单版本，集中精力冲方向三） |
| 构建 | CMake |
| GPU 数据 | `QProcess` 调 `nvidia-smi --query-gpu=...` |
| 进程数据 | 读 `/proc/[pid]`（Linux 原生，无 psutil） |
| 主题 | `DPalette` + `DGuiApplicationHelper::setPaletteType` + DBus 监听 `org.deepin.dde.Appearance1.Changed` |
| 进度控件 | `DProgressBar`（实测比 `DCircleProgress` 紧凑） |
| 单实例 | `DApplication::setSingleInstance("deskmon")` |
| 窗口 | `DWidget` + `DBlurEffectWidget` 悬浮窗（磨砂 + 圆角 + AutoColor 遮罩） |
| 通知 | `DUtil::DNotifySender`（无边框悬浮窗场景比 `DFloatingMessage` 可靠） |

## 基于 deepin Skill 的开发过程

本作品全程借助 **deepin Skills** 仓库的 `dtk-development` 模块完成（仓库共 4 个 Skill，本作品是应用非插件，仅使用该模块）。

**实际调用并落地到代码的 references 文档**：

| Skill 引用文档 | 用于 | 代码位置 |
|------|------|------|
| `widgets/application.md` | `DApplication` 入口、单实例、`newInstanceStarted` 激活 | `main.cpp:62-89` |
| `widgets/window.md` | 主悬浮窗选型 `DWidget` | `monitorwidget.h:29-33` |
| `widgets/blur-effect.md` | `DBlurEffectWidget` 磨砂+圆角、`AutoColor` 跟随主题 | `monitorwidget.cpp:122-130` |
| `widgets/progress.md` | `DProgressBar` 紧凑指标条 | `metricrow.cpp:51` |
| `widgets/dialog.md` | `DDialog` 设置/GPU/进程对话框 | `settings_dialog.cpp:20-31` |
| `widgets/view.md` | `DTableWidget` 进程/显存表 | `gpu_dialog.h:8-38` |
| `theme/palette.md` | `DPalette::Highlight` 派生指标配色 | `themecolors.cpp:6-15` |
| `theme/theme-switch.md` | `themeTypeChanged` 联动刷新 | `monitorwidget.cpp:95` |
| `utilities/gui-helper.md` | `setPaletteType(DarkType/LightType)` 主动同步 | `main.cpp:32-36` |
| `utilities/dbus.md` | 监听 `Appearance1.Changed` 补 GlobalTheme 跟随 | `main.cpp:18-83` |
| `utilities/desktop-services.md` | `DNotifySender` 番茄钟/阈值告警通知 | `pomodoro.cpp:194`、`monitorwidget.cpp:559` |
| `app-dev-with-dtk.md` | CMake 工程 + `debian/control` 模板 | `CMakeLists.txt:17-20` |
| `dtksrc-compile-debug.md` | 工具链断链重定向、`DLogManager` 破坏事件循环定位 | 见下方踩坑节 |

## 安装

提供了 `.deb` 安装包（99 KB），依赖自动解析，安装后从启动器搜「DeskMon / 桌面监控」即可。

```bash
# 安装（apt 会自动按 Recommends 拉装 nvidia-smi，无 N 卡也正常装）
sudo apt install ./deskmon_1.0.0-1_amd64.deb

# 卸载（保留 ~/.config/deskmon 用户配置）
sudo apt remove deskmon
```

包元数据：
- `Installed-Size: 345 KB`（`dpkg-gencontrol` 自动计算）
- `Recommends: nvidia-smi`（软依赖，无 N 卡用户也能安装，启动时自动降级隐藏 GPU 面板）
- 依赖由 `dh_shlibdeps` 自动生成（`libdtk6*`、`libqt6*`）

查看安装大小：`dpkg -l deskmon` 或 `apt show deskmon | grep Installed-Size`

## 源码构建

```bash
# 本机工具链在自定义前缀（Qt 6.8.0 + DTK6 6.7.44），标准系统包环境下可省略 CMAKE_PREFIX_PATH
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/jm-prefix/usr" -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"

# 或用快速启动脚本（build/run/clean 三模式）
./start.sh            # 增量编译后启动
./start.sh clean      # 清理后全量编译并启动

# deb 打包
dpkg-buildpackage -b -us -uc -d
mkdir -p dist && cp ../deskmon_*.deb dist/
```

## 踩过的坑（顺手给后人提个醒）

1. **`DMainWindow` 不适合无边框悬浮窗**：它面向带标题栏/侧栏的标准应用，会强加最小尺寸。改用 `DWidget` + `DBlurEffectWidget` 承担磨砂圆角。

2. **`DApplication` 默认不跟随 DDE GlobalTheme**：商店外安装的应用尤甚，只靠 `themeTypeChanged` 被动响应没用。修复：启动时通过 DBus 读 `org.deepin.dde.Appearance1.GlobalTheme/GtkTheme` 同步一次，再监听 `Changed` 信号（`ty` 是小写 `"globaltheme"/"gtk"`，需 `compare(CaseInsensitive)`）实时更新 `paletteType`。

3. **`DFloatingMessage` 在无边框悬浮窗不渲染**：阈值告警改用 `DNotifySender`，窗口隐藏时也能提醒。

4. **`DLogManager::registerConsoleAppender()` 破坏事件循环**：QTimer 不再触发，二分定位后弃用，改用 Qt 原生日志（`qDebug/qInfo`）。

5. **环形进度太占高度**：改用紧凑长条 `MetricRow`（点+名称+百分比+细条），节省纵向空间。

6. **deb 打包时 `LD_LIBRARY_PATH` 覆盖 fakeroot**：本机工具链在自定义前缀，`debian/rules` 里 `export LD_LIBRARY_PATH :=` 直接覆盖了 fakeroot 注入的 preload 路径，导致 `libfakeroot-sysv.so` 找不到。修法是 `:=` 前用 `$(if $(LD_LIBRARY_PATH),:$(LD_LIBRARY_PATH))` 追加保留父进程值。

## 无 NVIDIA 时的降级

启动时探测 `nvidia-smi` 是否存在且可执行成功；不可用则隐藏 GPU 面板与「GPU 管理」托盘菜单项，主窗口自动收缩布局，配置中不写入 GPU 相关项。进程管理仍可用（走 `/proc`）。deb 里 `nvidia-smi` 放 `Recommends` 而非 `Depends`，避免无 N 卡用户无法安装。

## 仓库

仓库地址：[GitHub - kookboy/deskmon](https://github.com/kookboy/deskmon)（参赛期间以本地仓库为准，发帖后公开）

许可证：**GPL-2.0-or-later**

## 适用环境

- deepin / UOS / DDE 桌面环境
- DTK6 + Qt6 运行时
- Linux x86_64

---

**致谢**：感谢 deepin 社区的 `dtk-development` Skill 文档，让一个 PyQt5 老项目能顺利迁移成原生 DTK6 应用。希望这个工具对大家有用，欢迎 issue / PR / 星标 ✨
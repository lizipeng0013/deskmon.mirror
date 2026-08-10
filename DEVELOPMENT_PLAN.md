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
- [x] 系统托盘：显示/隐藏、透明度、设置、退出（GPU/进程管理入口待补）
- [x] 配置持久化：`~/.config/deskmon/config.json`
- [x] 开机自启动

> **无 NVIDIA 时的降级策略**：启动时探测 `nvidia-smi` 是否存在且可执行成功；不可用则隐藏 GPU 面板与「GPU 管理」托盘菜单项，主窗口自动收缩布局，配置中不写入 GPU 相关项。进程管理仍可用（走 `/proc`）。deb 中 `nvidia-smi` 放 `Recommends` 而非 `Depends`，避免无 N 卡用户无法安装。

### 4.2 加分项（冲刺一等奖）
- [ ] 🍅 番茄钟面板：25/5 分钟倒计时 + 完成通知
- [ ] 📈 CPU/GPU 迷你折线图：最近 60 秒趋势
- [x] 🎨 主题自动跟随：亮/暗/活跃色（DPalette）
- [ ] 🎮 显存一键释放：`DTableWidget` + `DDialog`
- [x] ⚙️ 原生设置面板
- [ ] 🔔 阈值告警：CPU/GPU > 90% 时 `DFloatingMessage`

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
│   ├── systemmonitor.cpp/.h       # CPU/内存/磁盘/网络 数据层
│   ├── nvidia_gpu.cpp/.h          # nvidia-smi 解析
│   ├── processmgr.cpp/.h          # 进程查询（/proc）
│   ├── widgets/
│   │   ├── metricring.cpp/.h      # CPU/GPU 环形进度
│   │   ├── metricbar.cpp/.h       # 内存/磁盘 条形进度
│   │   ├── netwidget.cpp/.h       # 网络组件
│   │   ├── timerwidget.cpp/.h     # 番茄钟（加分项）
│   │   └── sparkline.cpp/.h       # 迷你折线（加分项）
│   ├── monitorwidget.cpp/.h       # 主悬浮窗口
│   ├── gpu_dialog.cpp/.h          # 显存管理
│   ├── process_dialog.cpp/.h      # 进程管理
│   └── settings_dialog.cpp/.h     # 设置
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
| 主题 | `DPalette` + `DStyle` | 指标色由系统活跃色派生（`themecolors.h`），明暗主题自动调亮度 |
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

// 主题切换监听
connect(DGuiApplicationHelper::instance(),
        &DGuiApplicationHelper::themeTypeChanged,
        this, &MyWidget::onThemeChanged);

// 环形进度
#include <DCircleProgress>
auto *ring = new DCircleProgress(this);
ring->setValue(75);

// 消息提示（DFloatingMessage 需实例化后 show，非全局函数）
#include <DFloatingMessage>
auto *msg = new DFloatingMessage(DFloatingMessage::ResidentType, this);
msg->setIcon(QIcon());
msg->setMessage("CPU 使用率超过 90%");
msg->show();

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

## 七、AI 辅助环节（活动硬要求截图）

比赛要求提供「AI 编程工具中调用 deepin Skills 的对话记录截图」，在以下环节调用 `dtk-development` skill 并截图：

1. 环形进度控件（`DCircleProgress` 用法核对）
2. 主题跟随实现（`DPalette` + `DGuiApplicationHelper`）
3. DTK 窗口效果（磨砂 + 圆角，以 `references/widgets/blur-effect.md` 为准核对 API）
4. 单实例 + 托盘
5. deb 打包配置（`debian/control`）
6. 编译排错（DTK6/Qt6 头文件缺失、CMake 找包失败等）

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

---

## 九、待办清单

- [x] 编译工具链（jm-prefix 已含 Qt6.8 + DTK6.7.44；已修复断链 + 补齐 `DObject`/`DThemeManager` 转发头）
- [x] 确认 DTK6 头文件可用
- [x] 搭 CMake 工程骨架 + `main.cpp` 跑通
- [x] 实现数据层（`SystemMonitor` 移植：CPU/内存/磁盘/网络/GPU）
- [x] 实现 `MetricRow` / `NetWidget` 控件（原 `MetricRing`/`MetricBar` 已由 `MetricRow` 取代）
- [x] 实现主悬浮窗口 `MonitorWidget`（拖动/改宽/磨砂圆角/单实例）
- [x] 实现系统托盘（显示隐藏/透明度/设置/退出）
- [ ] 实现 GPU / 进程管理对话框
- [x] 实现设置面板 + 配置持久化 + 开机自启动 + 透明度可调
- [ ] 实现番茄钟（加分项）
- [ ] 实现迷你折线图（加分项）
- [x] 实现主题自动跟随（加分项）
- [x] 设计矢量图标（`icons/deskmon.svg`；多尺寸 PNG 待 deb 打包时生成）
- [ ] deb 打包
- [ ] 录演示视频、截图
- [ ] 论坛发帖提交
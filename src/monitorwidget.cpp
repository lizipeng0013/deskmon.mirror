// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "monitorwidget.h"

#include "config.h"
#include "systemmonitor.h"
#include "themecolors.h"
#include "settings_dialog.h"
#include "process_dialog.h"
#include "gpu_dialog.h"
#include "widgets/metricrow.h"
#include "widgets/netwidget.h"
#include "widgets/sparkline.h"
#include "widgets/pomodoro.h"
#include "widgets/minibar.h"

#include <DNotifySender>

#include <DAboutDialog>
#include <DApplication>
#include <DGuiApplicationHelper>
#include <DBlurEffectWidget>
#include <DPalette>
#include <DPaletteHelper>

#include <QTimer>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedLayout>
#include <QFrame>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QScreen>
#include <QGuiApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QActionGroup>
#include <QToolButton>
#include <QFont>
#include <QApplication>
#include <QIcon>
#include <QFile>

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

// 电力（闪电）图标：颜色由外部设置，跟随主题活跃色
// （全局类，配合 monitorwidget.h 的前向声明）
class PowerIcon : public QWidget
{
public:
    explicit PowerIcon(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(18, 18);
    }

    void setColor(const QColor &color)
    {
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(m_color);
        QPolygonF bolt;
        bolt << QPointF(11, 1) << QPointF(4, 11) << QPointF(8, 11)
             << QPointF(7, 17) << QPointF(14, 8) << QPointF(10, 8) << QPointF(11, 1);
        p.drawPolygon(bolt);
    }

private:
    QColor m_color = Qt::gray;
};

MonitorWidget::~MonitorWidget() = default;

MonitorWidget::MonitorWidget(QWidget *parent)
    : DWidget(parent)
    , m_config(std::make_unique<Config>())
    , m_monitor(std::make_unique<SystemMonitor>())
{
    setupUi();
    setupTray();
    applyConfig();
    applyThemeColors();

    // 主题切换（亮/暗）时实时重算指标色
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
            this, [this](DGuiApplicationHelper::ColorType) { applyThemeColors(); });

    // GPU 探测异步完成时更新可见性（探测在 SystemMonitor 构造时启动）
    connect(m_monitor.get(), &SystemMonitor::gpuAvailabilityChanged, this, [this](bool available) {
        m_gpuAvailable = available;
        updateRowsVisibility();
    });

    // 定时刷新
    m_timer = new QTimer(this);
    m_timer->setInterval(m_config->refreshInterval());
    connect(m_timer, &QTimer::timeout, this, &MonitorWidget::refresh);
    m_timer->start();

    refresh();
}

void MonitorWidget::setupUi()
{
    setMinimumWidth(180);
    setMaximumWidth(400);
    resize(m_config->windowWidth(), 100);

    // 无边框 + 置顶 + 不抢占焦点（悬浮小窗）
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_container = new DBlurEffectWidget(this);
    m_container->setBlurEnabled(true);
    m_container->setRadius(10);
    m_container->setBlurRectXRadius(10);
    m_container->setBlurRectYRadius(10);
    // 遮罩色跟随系统明/暗主题（之前固定深色导致亮色主题下不跟随）
    m_container->setMaskColor(DBlurEffectWidget::AutoColor);
    m_container->setMaskAlpha(170);
    m_container->setBlendMode(DBlurEffectWidget::BehindWindowBlend);
    mainLayout->addWidget(m_container);

    // full/mini 内容互换：QStackedLayout 在两个面板间切，窗口行为逻辑不动
    m_stack = new QStackedLayout(m_container);
    m_stack->setContentsMargins(0, 0, 0, 0);

    // ---------------- 完整模式面板 ----------------
    m_fullPanel = new QWidget(m_container);
    m_stack->addWidget(m_fullPanel);

    auto *contentLayout = new QVBoxLayout(m_fullPanel);
    contentLayout->setContentsMargins(16, 14, 16, 12);
    contentLayout->setSpacing(6);

    // 标题行：电力图标 + 应用名（随主题活跃色）
    auto *titleLayout = new QHBoxLayout;
    titleLayout->setSpacing(6);
    m_titleIcon = new PowerIcon(m_fullPanel);
    m_titleIcon->setColor(QColor(QStringLiteral("#e8b020")));  // 金黄色
    auto *titleLabel = new QLabel(tr("系统监控"), m_fullPanel);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1);
    titleLabel->setFont(titleFont);
    // 保证标题文字不被右侧按钮挤压缩
    const QFontMetrics tFm(titleFont);
    titleLabel->setMinimumWidth(tFm.horizontalAdvance(tr("系统监控")) + 4);
    titleLayout->addWidget(m_titleIcon);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch(1);  // 标题与按钮组之间留白，按钮统一偏右

    // 三个按钮紧凑成组：◐ 透明度 → ⊟ 收缩 → ✕ 隐藏
    m_opacityBtn = new QToolButton(m_fullPanel);
    m_opacityBtn->setText(QStringLiteral("◐"));
    m_opacityBtn->setToolTip(tr("点击切换透明度（当前 %1%）")
                                 .arg(qRound(m_config->opacity() * 100)));
    m_opacityBtn->setCursor(Qt::PointingHandCursor);
    m_opacityBtn->setAutoRaise(true);
    connect(m_opacityBtn, &QToolButton::clicked, this, &MonitorWidget::cycleOpacity);
    titleLayout->addWidget(m_opacityBtn);

    // 收缩为迷你条：点击切 mini 模式（在 ◐ 和 ✕ 之间）
    m_collapseBtn = new QToolButton(m_fullPanel);
    m_collapseBtn->setText(QStringLiteral("⊡"));
    m_collapseBtn->setToolTip(tr("收起为单行迷你浮动条"));
    m_collapseBtn->setCursor(Qt::PointingHandCursor);
    m_collapseBtn->setAutoRaise(true);
    connect(m_collapseBtn, &QToolButton::clicked, this, [this] { setDisplayMode(QStringLiteral("mini")); });
    titleLayout->addWidget(m_collapseBtn);

    // 隐藏按钮：点击隐藏窗口到托盘（托盘左键可唤回）
    auto *hideBtn = new QToolButton(m_fullPanel);
    hideBtn->setText(QStringLiteral("✕"));
    hideBtn->setToolTip(tr("隐藏到托盘（托盘左键点击唤回）"));
    hideBtn->setCursor(Qt::PointingHandCursor);
    hideBtn->setAutoRaise(true);
    connect(hideBtn, &QToolButton::clicked, this, &QWidget::hide);
    titleLayout->addWidget(hideBtn);

    contentLayout->addLayout(titleLayout);

    // 分隔线：标题区 / 指标区
    m_titleSep = new QFrame(m_fullPanel);
    m_titleSep->setFrameShape(QFrame::HLine);
    m_titleSep->setFixedHeight(1);
    contentLayout->addWidget(m_titleSep);

    // 指标行（每项一行，独立配色，紧凑省高度；颜色由主题派生，见 applyThemeColors）
    m_cpuRow = new MetricRow(tr("CPU"), QColor(QStringLiteral("#0081ff")), m_fullPanel);
    contentLayout->addWidget(m_cpuRow);

    m_memRow = new MetricRow(tr("内存"), QColor(QStringLiteral("#00a8b8")), m_fullPanel);
    contentLayout->addWidget(m_memRow);

    // GPU 行始终创建，可见性由 updateRowsVisibility 控制（降级策略 + 设置项）
    m_gpuAvailable = m_monitor->gpuAvailable();
    m_gpuRow = new MetricRow(tr("GPU"), QColor(QStringLiteral("#ff6b9d")), m_fullPanel);
    contentLayout->addWidget(m_gpuRow);

    m_diskRow = new MetricRow(tr("系统盘"), QColor(QStringLiteral("#ffd93d")), m_fullPanel);
    contentLayout->addWidget(m_diskRow);

    // 分隔线：指标区 / 趋势区
    m_metricsSep = new QFrame(m_fullPanel);
    m_metricsSep->setFrameShape(QFrame::HLine);
    m_metricsSep->setFixedHeight(1);
    contentLayout->addWidget(m_metricsSep);

    // CPU/GPU 趋势迷你折线（加分项）：窗口始终约 60 秒，缓冲随刷新间隔自适应
    m_sparkline = new Sparkline(m_fullPanel);
    m_sparkline->setSeriesCount(2);
    m_sparkline->setBufferSize(qMax(12, 60000 / qMax(1, m_config->refreshInterval())));
    m_sparkline->setSeriesName(0, tr("CPU"));
    m_sparkline->setSeriesName(1, tr("GPU"));
    contentLayout->addWidget(m_sparkline);

    // 网络
    m_netWidget = new NetWidget(m_fullPanel);
    contentLayout->addWidget(m_netWidget);

    // 分隔线：折线图 / 网络区（始终显示，增强区块边界）
    m_netTopSep = new QFrame(m_fullPanel);
    m_netTopSep->setFrameShape(QFrame::HLine);
    m_netTopSep->setFixedHeight(1);
    contentLayout->addWidget(m_netTopSep);

    // 分隔线：网络区 / 工具区（番茄钟显示时才出现）
    m_netSep = new QFrame(m_fullPanel);
    m_netSep->setFrameShape(QFrame::HLine);
    m_netSep->setFixedHeight(1);
    m_netSep->setVisible(false);
    contentLayout->addWidget(m_netSep);

    // 番茄钟（加分项，默认隐藏，托盘可切换）
    m_pomodoro = new Pomodoro(m_fullPanel);
    m_pomodoro->setVisible(false);
    contentLayout->addWidget(m_pomodoro);

    // 底部右侧留出宽度调整热区（右下角 16px）
    auto *resizeHint = new QLabel(tr("⟋"), m_fullPanel);
    resizeHint->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    QFont hintFont = resizeHint->font();
    hintFont.setPointSizeF(hintFont.pointSizeF() - 3);
    resizeHint->setFont(hintFont);
    contentLayout->addWidget(resizeHint, 0, Qt::AlignRight);

    // ---------------- 迷你模式浮动条 ----------------
    m_miniBar = new MiniBar(m_container);
    m_stack->addWidget(m_miniBar);
    connect(m_miniBar, &MiniBar::expandRequested, this, [this] { setDisplayMode(QStringLiteral("full")); });
    connect(m_miniBar, &MiniBar::opacityClicked, this, &MonitorWidget::cycleOpacity);
    connect(m_miniBar, &MiniBar::hideRequested, this, &QWidget::hide);

    // 按持久化的模式选初始页（默认完整）
    setDisplayMode(m_config->displayMode());
}

void MonitorWidget::setupTray()
{
    // 图标：优先主题图标 → 本地 PNG → 程序绘制回退
    QIcon icon = QIcon::fromTheme(QStringLiteral("deskmon"));
    if (icon.isNull()) {
        // 尝试从程序目录/../share/icons/hicolor 加载多尺寸 PNG（安装布局为 /usr/bin/../share/...）
        const QString iconDir = QApplication::applicationDirPath() + QStringLiteral("/../share/icons/hicolor");
        for (int sz : {256, 128, 64, 48, 32, 22, 16}) {
            const QString path = QStringLiteral("%1/%2x%2/apps/deskmon.png").arg(iconDir).arg(sz);
            if (QFile::exists(path)) {
                QPixmap pm(path);
                if (!pm.isNull()) {
                    icon.addFile(path, QSize(sz, sz));
                }
            }
        }
    }
    if (icon.isNull()) {
        // 最终回退：程序绘制简单图标
        QPixmap pm(64, 64);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        const QColor accent = DGuiApplicationHelper::instance()->applicationPalette().color(DPalette::Highlight);
        p.setBrush(accent);
        p.drawEllipse(8, 8, 48, 48);
        p.end();
        icon = QIcon(pm);
    }

    auto *tray = new QSystemTrayIcon(icon, this);
    tray->setToolTip(tr("系统监控 DeskMon"));

    auto *menu = new QMenu(this);
    QAction *showAction = menu->addAction(tr("显示/隐藏"));
    connect(showAction, &QAction::triggered, this, [this] {
        if (isVisible())
            hide();
        else {
            show();
            positionWindow();
        }
    });

    // 透明度子菜单
    QMenu *opacityMenu = menu->addMenu(tr("透明度"));
    auto *opacityGroup = new QActionGroup(opacityMenu);
    opacityGroup->setExclusive(true);
    const int currentPct = qRound(m_config->opacity() * 100);
    for (int pct : {50, 60, 70, 80, 85, 90, 100}) {
        QAction *act = opacityMenu->addAction(QStringLiteral("%1%").arg(pct));
        act->setCheckable(true);
        act->setChecked(pct == currentPct);
        opacityGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, pct] {
            setOpacity(pct / 100.0);
        });
    }

    // 显示模式子菜单（完整 / 迷你）
    QMenu *modeMenu = menu->addMenu(tr("显示模式"));
    auto *modeGroup = new QActionGroup(modeMenu);
    modeGroup->setExclusive(true);
    const QString curMode = m_config->displayMode().isEmpty() ? QStringLiteral("full") : m_config->displayMode();
    m_fullModeAct = modeMenu->addAction(tr("完整窗口"));
    m_fullModeAct->setCheckable(true);
    m_fullModeAct->setChecked(curMode == QStringLiteral("full"));
    modeGroup->addAction(m_fullModeAct);
    connect(m_fullModeAct, &QAction::triggered, this, [this] { setDisplayMode(QStringLiteral("full")); });
    m_miniModeAct = modeMenu->addAction(tr("迷你条"));
    m_miniModeAct->setCheckable(true);
    m_miniModeAct->setChecked(curMode == QStringLiteral("mini"));
    modeGroup->addAction(m_miniModeAct);
    connect(m_miniModeAct, &QAction::triggered, this, [this] { setDisplayMode(QStringLiteral("mini")); });

    // GPU 显存管理（无 N 卡时隐藏）
    if (m_gpuAvailable) {
        QAction *gpuAction = menu->addAction(tr("GPU 显存管理"));
        connect(gpuAction, &QAction::triggered, this, [this] {
            auto *dlg = new GpuDialog(m_monitor.get(), this);
            dlg->exec();
            dlg->deleteLater();
        });
    }

    // 进程管理
    QAction *processAction = menu->addAction(tr("进程管理"));
    connect(processAction, &QAction::triggered, this, [this] {
        auto *dlg = new ProcessDialog(this);
        dlg->exec();
        dlg->deleteLater();
    });

    // 番茄钟（加分项）
    QAction *pomodoroAction = menu->addAction(tr("🍅 番茄钟"));
    pomodoroAction->setCheckable(true);
    pomodoroAction->setChecked(m_pomodoro->isVisible());
    connect(pomodoroAction, &QAction::toggled, this, [this](bool on) {
        if (m_pomodoro)
            m_pomodoro->setVisible(on);
        if (m_netSep)
            m_netSep->setVisible(on);
        if (on)
            positionWindow();
    });

    // 设置面板
    QAction *settingsAction = menu->addAction(tr("设置"));
    connect(settingsAction, &QAction::triggered, this, &MonitorWidget::openSettings);

    // 关于（作者：kookboy，版本号来自编译宏）
    QAction *aboutAction = menu->addAction(tr("关于"));
    connect(aboutAction, &QAction::triggered, this, [this, tray] {
        auto *dlg = new DAboutDialog(this);
        dlg->setProductName(tr("DeskMon 系统监控"));
        dlg->setProductIcon(tray->icon());
        dlg->setVersion(QCoreApplication::applicationVersion());
        dlg->setDescription(tr("DTK6 原生桌面系统监控悬浮小工具，"
                               "实时显示 CPU / GPU / 内存 / 磁盘 / 网络状态。\n"
                               "作者：kookboy"));
        dlg->setWebsiteName(QStringLiteral("gitee.com/yngeek/deskmon"));
        dlg->setWebsiteLink(QStringLiteral("https://gitee.com/yngeek/deskmon"));
        dlg->exec();
        dlg->deleteLater();
    });

    menu->addSeparator();
    QAction *quitAction = menu->addAction(tr("退出"));
    connect(quitAction, &QAction::triggered, this, &QApplication::quit);

    tray->setContextMenu(menu);
    connect(tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            if (isVisible())
                hide();
            else {
                show();
                positionWindow();
            }
        }
    });
    tray->show();
}

void MonitorWidget::applyConfig()
{
    setWindowOpacity(m_config->opacity());
    updateRowsVisibility();
    applyStayOnTop();
    positionWindow();
}

void MonitorWidget::updateRowsVisibility()
{
    // GPU 行：无 N 卡或用户关闭 → 隐藏（降级策略 + 设置项）
    const bool gpu = m_gpuAvailable && m_config->showGpu();
    const bool disk = m_config->showDisk();
    m_gpuRow->setVisible(gpu);
    m_diskRow->setVisible(disk);
    if (m_miniBar)
        m_miniBar->setRowsVisible(gpu, disk);
}

void MonitorWidget::applyStayOnTop()
{
    setWindowFlag(Qt::WindowStaysOnTopHint, m_config->stayOnTop());
    show();
    positionWindow();
}

void MonitorWidget::openSettings()
{
    auto *dlg = new SettingsDialog(m_config.get(), m_gpuAvailable, this);
    connect(dlg, &SettingsDialog::settingsApplied, this, [this] {
        applyConfig();
        // 刷新间隔可能被修改，重启定时器；折线缓冲同步，保持约 60 秒窗口
        m_timer->setInterval(m_config->refreshInterval());
        m_timer->start();
        if (m_sparkline)
            m_sparkline->setBufferSize(qMax(12, 60000 / qMax(1, m_config->refreshInterval())));
    });
    dlg->exec();
    dlg->deleteLater();
}

static void setSeparatorColor(QFrame *line, const QColor &color)
{
    if (!line)
        return;
    line->setStyleSheet(QStringLiteral("background-color: %1; border: none;").arg(color.name()));
    line->setFrameShadow(QFrame::Plain);
}

void MonitorWidget::applyThemeColors()
{
    const QVector<QColor> colors = ThemeColors::metricColors();
    m_cpuRow->setColor(colors.value(0));
    m_memRow->setColor(colors.value(1));
    if (m_gpuRow)
        m_gpuRow->setColor(colors.value(2));
    m_diskRow->setColor(colors.value(3));
    // 折线颜色与标题圆点同步活跃色
    if (m_sparkline) {
        m_sparkline->setColor(0, colors.value(0));
        m_sparkline->setColor(1, colors.value(2));
    }
    // 迷你条指标色与完整窗同源
    if (m_miniBar) {
        for (int i = 0; i < 4; ++i)
            m_miniBar->setMetricColor(i, colors.value(i));
        const auto arrows = ThemeColors::netArrowColors();
        m_miniBar->setNetColors(arrows.first, arrows.second);
        m_miniBar->updateLabelColors();
    }
    // 标题电力图标固定金黄色（不随主题色），无需在此更新
    // 网络箭头：上传橙、下载绿
    if (m_netWidget) {
        const auto arrows = ThemeColors::netArrowColors();
        m_netWidget->setArrowColors(arrows.first, arrows.second);
        m_netWidget->updateLabelColors();   // 标签/IP 提示色跟随主题
    }
    // 分隔线颜色跟随主题边框色
    const DPalette pal = DPaletteHelper::instance()->palette(this);
    const QColor frameColor = pal.color(DPalette::FrameBorder);
    setSeparatorColor(m_titleSep, frameColor);
    setSeparatorColor(m_metricsSep, frameColor);
    setSeparatorColor(m_netTopSep, frameColor);
    setSeparatorColor(m_netSep, frameColor);
}

void MonitorWidget::setOpacity(double v)
{
    const double clamped = qBound(0.3, v, 1.0);
    setWindowOpacity(clamped);
    m_config->set(QStringLiteral("opacity"), clamped);
    if (m_opacityBtn) {
        m_opacityBtn->setToolTip(tr("点击切换透明度（当前 %1%）")
                                     .arg(qRound(clamped * 100)));
    }
}

void MonitorWidget::cycleOpacity()
{
    // 透明度档位：100% → 85% → 70% → 55% → 40% → 循环
    static const QVector<double> presets = {1.0, 0.85, 0.70, 0.55, 0.40};
    const double cur = m_config->opacity();
    int idx = 0;
    double best = 999.0;
    for (int i = 0; i < presets.size(); ++i) {
        const double d = qAbs(presets.at(i) - cur);
        if (d < best) {
            best = d;
            idx = i;
        }
    }
    setOpacity(presets.at((idx + 1) % presets.size()));
}

void MonitorWidget::setDisplayMode(const QString &mode)
{
    if (!m_stack || (mode != QStringLiteral("full") && mode != QStringLiteral("mini")))
        return;

    const bool isMini = (mode == QStringLiteral("mini"));
    const bool wasMini = (m_stack->currentWidget() == m_miniBar);  // 区分真切换与同模式重复点击

    m_stack->setCurrentWidget(isMini ? static_cast<QWidget *>(m_miniBar) : m_fullPanel);

    // 圆角：迷你条更圆，胶囊感；完整窗沿用 10
    if (m_container) {
        const int r = isMini ? 14 : 10;
        m_container->setRadius(r);
        m_container->setBlurRectXRadius(r);
        m_container->setBlurRectYRadius(r);
    }

    // 尺寸：迷你固定高度，宽度按 sizeHint 并 clamp 到 [180,400]；
    // 完整恢复可变高度，宽度回配置值，高度由布局撑开（首次用当前高度保底）
    if (isMini) {
        const int w = qBound(180, m_miniBar->sizeHint().width(), 400);
        setFixedHeight(28);
        resize(w, 28);  // 高度已被 setFixedHeight 锁定，第二参数仅占位
    } else {
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
        // 完整模式宽度：取配置宽度与内容 sizeHint 宽度的较大值，
        // 避免 window_width 过窄导致标题栏 addStretch 剩余空间不足、按钮无法偏右
        const int contentW = m_fullPanel->sizeHint().width();
        const int w = qMax(m_config->windowWidth(), contentW);
        resize(w, qMax(height(), m_fullPanel->sizeHint().height()));
    }

    // 仅运行时真正切换模式时贴右下角；构造期或同模式重复点击不重定位
    if (isVisible() && wasMini != isMini) {
        snapToBottomRight();
        savePosition();   // 同步持久化，避免重启后位置与视觉不一致
    }
    m_config->setDisplayMode(mode);

    // 同步托盘「显示模式」勾选，避免从标题栏按钮/双击切换后脱节
    if (m_fullModeAct) m_fullModeAct->setChecked(!isMini);
    if (m_miniModeAct) m_miniModeAct->setChecked(isMini);
}

void MonitorWidget::positionWindow()
{
    const int x = m_config->positionX();
    const int y = m_config->positionY();
    if (x >= 0 && y >= 0) {
        move(x, y);
        return;
    }
    // 默认右下角（用实际窗口尺寸，show 后调用才能取到最终值）
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move(avail.right() - width() - 20, avail.bottom() - height() - 20);
    }
}

void MonitorWidget::snapToBottomRight()
{
    // 贴右下角：以窗口当前所在屏幕为准（多显示器不跨屏跳），距边缘 4px
    QScreen *screen = QGuiApplication::screenAt(pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const QRect avail = screen->availableGeometry();
    constexpr int kSnapMargin = 4;
    move(avail.right() - width() - kSnapMargin, avail.bottom() - height() - kSnapMargin);
}

void MonitorWidget::showEvent(QShowEvent *e)
{
    DWidget::showEvent(e);
    // 首次显示时布局已按内容定型，此时重新定位到右下角（若未存过自定义位置）
    positionWindow();
}

void MonitorWidget::savePosition()
{
    m_config->set(QStringLiteral("position_x"), pos().x());
    m_config->set(QStringLiteral("position_y"), pos().y());
}

void MonitorWidget::refresh()
{
    double cpu = -1, gpu = -1;

    // CPU
    cpu = m_monitor->cpuUsage();
    // 温度每 tick 只读一次（sysfs 扫描有开销），完整面板与迷你条共用；
    // 无传感器时返回 -1，显示侧隐藏温度信息而不是「-1℃」
    const double cpuTemp = m_monitor->cpuTemp();
    const QString cpuTempInfo = cpuTemp >= 0 ? tr("温度 %1℃").arg(int(cpuTemp + 0.5)) : QString();
    if (cpu >= 0) {
        m_cpuRow->setValue(int(cpu + 0.5));
        m_cpuRow->setInfo(cpuTempInfo);
    }

    // 内存
    double memPercent = 0;
    qint64 memUsed = 0, memTotal = 0;
    m_monitor->memoryUsage(memPercent, memUsed, memTotal);
    if (memPercent >= 0) {
        m_memRow->setValue(int(memPercent + 0.5));
        m_memRow->setInfo(QStringLiteral("%1/%2G").arg(memUsed / 1024.0, 0, 'f', 1)
                                                  .arg(memTotal / 1024.0, 0, 'f', 1));
    }

    // GPU
    const auto gpuStats = m_monitor->gpuStats();
    if (m_gpuRow) {
        gpu = gpuStats.util;
        if (gpu >= 0) {
            m_gpuRow->setValue(int(gpu + 0.5));
            m_gpuRow->setInfo(QStringLiteral("%1℃·%2/%3G").arg(int(gpuStats.temp))
                                                .arg(gpuStats.memUsedMB / 1024.0, 0, 'f', 1)
                                                .arg(gpuStats.memTotalMB / 1024.0, 0, 'f', 1));
        }
    } else {
        gpu = gpuStats.util;  // 无 GPU 行时仍取值供迷你条/折线使用
    }

    // 系统盘
    double diskPercent = 0;
    qint64 diskUsed = 0, diskTotal = 0;
    m_monitor->diskUsage(diskPercent, diskUsed, diskTotal);
    if (diskPercent >= 0) {
        m_diskRow->setValue(int(diskPercent + 0.5));
        m_diskRow->setInfo(QStringLiteral("%1/%2G")
                              .arg(diskUsed / 1.0, 0, 'f', 1)
                              .arg(diskTotal / 1.0, 0, 'f', 1));
    }

    // 网络
    const auto speed = m_monitor->networkSpeed();
    m_netWidget->setSpeed(speed.first, speed.second);
    m_netWidget->setIps(m_monitor->ipAddresses());

    // 折线趋势 + 阈值告警
    if (m_sparkline) {
        if (cpu >= 0) m_sparkline->setValue(0, cpu);
        if (gpu >= 0) m_sparkline->setValue(1, gpu);
    }

    // 迷你条：不可见时跳过，省 CPU
    if (m_miniBar && m_stack && m_stack->currentWidget() == m_miniBar) {
        if (cpu >= 0) {
            m_miniBar->setMetric(0, int(cpu + 0.5));
            m_miniBar->setMetricInfo(0, cpuTempInfo);
        }
        if (memPercent >= 0) {
            m_miniBar->setMetric(1, int(memPercent + 0.5));
            m_miniBar->setMetricInfo(1, QStringLiteral("%1/%2G").arg(memUsed / 1024.0, 0, 'f', 1)
                                                               .arg(memTotal / 1024.0, 0, 'f', 1));
        }
        if (gpu >= 0) {
            m_miniBar->setMetric(2, int(gpu + 0.5));
            m_miniBar->setMetricInfo(2, QStringLiteral("%1℃·%2/%3G").arg(int(gpuStats.temp))
                                                                    .arg(gpuStats.memUsedMB / 1024.0, 0, 'f', 1)
                                                                    .arg(gpuStats.memTotalMB / 1024.0, 0, 'f', 1));
        }
        if (diskPercent >= 0) {
            m_miniBar->setMetric(3, int(diskPercent + 0.5));
            m_miniBar->setMetricInfo(3, QStringLiteral("%1/%2G").arg(diskUsed / 1.0, 0, 'f', 1)
                                                               .arg(diskTotal / 1.0, 0, 'f', 1));
        }
        m_miniBar->setNetSpeed(speed.first, speed.second);
    }

    checkAlerts(cpu, gpu);
}

void MonitorWidget::checkAlerts(double cpu, double gpu)
{
    // 阈值：默认 90，可从配置覆盖
    const double cpuTh = m_config->get(QStringLiteral("cpu_alert_threshold"), 90).toDouble();
    const double gpuTh = m_config->get(QStringLiteral("gpu_alert_threshold"), 90).toDouble();

    // CPU 告警（边沿触发：仅从低于阈值跃升到超过时提醒一次）
    if (cpu >= 0) {
        if (cpu > cpuTh && !m_cpuAlerted) {
            m_cpuAlerted = true;
            notifyAlert(tr("CPU 使用率 %1% 超过阈值 %2%").arg(int(cpu)).arg(int(cpuTh)));
        } else if (cpu <= cpuTh) {
            m_cpuAlerted = false;
        }
    }

    // GPU 告警
    if (gpu >= 0) {
        if (gpu > gpuTh && !m_gpuAlerted) {
            m_gpuAlerted = true;
            notifyAlert(tr("GPU 使用率 %1% 超过阈值 %2%").arg(int(gpu)).arg(int(gpuTh)));
        } else if (gpu <= gpuTh) {
            m_gpuAlerted = false;
        }
    }
}

void MonitorWidget::notifyAlert(const QString &message)
{
    // 系统通知（DNotifySender）对无边框悬浮窗可靠，且窗口隐藏时也能提醒
    DUtil::DNotifySender(tr("DeskMon 告警"))
        .appName(QStringLiteral("DeskMon"))
        .appIcon(QStringLiteral("deskmon"))
        .appBody(message)
        .timeOut(3000)
        .call();
}

void MonitorWidget::togglePomodoro()
{
    if (!m_pomodoro)
        return;
    const bool on = !m_pomodoro->isVisible();
    m_pomodoro->setVisible(on);
    if (m_netSep)
        m_netSep->setVisible(on);
    if (on)
        positionWindow();
}

void MonitorWidget::activateWindow()
{
    show();
    positionWindow();
    raise();
}

// ---------------- 拖动 / 宽度调整 ----------------

void MonitorWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        const QPoint local = e->position().toPoint();
        // 右下角热区：调整宽度
        if (local.x() >= width() - 16 && local.y() >= height() - 16) {
            m_resizing = true;
            m_resizeStartPos = e->globalPosition().toPoint();
            m_resizeStartWidth = width();
            e->accept();
            return;
        }
        m_dragPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
        m_dragging = true;
        e->accept();
        return;
    }
    DWidget::mousePressEvent(e);
}

void MonitorWidget::mouseMoveEvent(QMouseEvent *e)
{
    // 右下角热区调整宽度
    if (m_resizing) {
        const int newWidth = m_resizeStartWidth + (e->globalPosition().toPoint().x() - m_resizeStartPos.x());
        resize(qBound(minimumWidth(), newWidth, maximumWidth()), height());
        e->accept();
        return;
    }
    if (m_dragging && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPosition().toPoint() - m_dragPos);
        e->accept();
        return;
    }
    // 进入右下角热区显示调整光标（仅完整模式；迷你条固定宽度）
    const bool fullMode = m_stack && m_stack->currentWidget() == m_fullPanel;
    const QPoint local = e->position().toPoint();
    const bool inCorner = fullMode && local.x() >= width() - 16 && local.y() >= height() - 16;
    setCursor(inCorner ? Qt::SizeHorCursor : Qt::ArrowCursor);
    DWidget::mouseMoveEvent(e);
}

void MonitorWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_resizing) {
        m_resizing = false;
        m_config->set(QStringLiteral("window_width"), width());
        e->accept();
        return;
    }
    if (m_dragging && e->button() == Qt::LeftButton) {
        m_dragging = false;
        savePosition();
        e->accept();
        return;
    }
    DWidget::mouseReleaseEvent(e);
}

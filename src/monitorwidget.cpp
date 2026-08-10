// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

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

#include <DNotifySender>

#include <DApplication>
#include <DGuiApplicationHelper>
#include <DBlurEffectWidget>
#include <DPalette>
#include <DPaletteHelper>

#include <QTimer>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
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

    auto *container = new DBlurEffectWidget(this);
    container->setBlurEnabled(true);
    container->setRadius(10);
    container->setBlurRectXRadius(10);
    container->setBlurRectYRadius(10);
    // 遮罩色跟随系统明/暗主题（之前固定深色导致亮色主题下不跟随）
    container->setMaskColor(DBlurEffectWidget::AutoColor);
    container->setMaskAlpha(170);
    container->setBlendMode(DBlurEffectWidget::BehindWindowBlend);
    mainLayout->addWidget(container);

    auto *contentLayout = new QVBoxLayout(container);
    contentLayout->setContentsMargins(14, 12, 14, 12);
    contentLayout->setSpacing(9);

    // 标题行：电力图标 + 应用名（随主题活跃色）
    auto *titleLayout = new QHBoxLayout;
    titleLayout->setSpacing(6);
    m_titleIcon = new PowerIcon(container);
    m_titleIcon->setColor(QColor(QStringLiteral("#e8b020")));  // 金黄色
    auto *titleLabel = new QLabel(tr("系统监控"), container);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1);
    titleLabel->setFont(titleFont);
    titleLayout->addWidget(m_titleIcon);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    // 对比度（透明度）快捷切换：点击循环
    m_opacityBtn = new QToolButton(container);
    m_opacityBtn->setText(QStringLiteral("◐"));
    m_opacityBtn->setToolTip(tr("点击切换透明度（当前 %1%）")
                                 .arg(qRound(m_config->opacity() * 100)));
    m_opacityBtn->setCursor(Qt::PointingHandCursor);
    m_opacityBtn->setAutoRaise(true);
    connect(m_opacityBtn, &QToolButton::clicked, this, &MonitorWidget::cycleOpacity);
    titleLayout->addWidget(m_opacityBtn);

    contentLayout->addLayout(titleLayout);

    auto *line = new QFrame(container);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    contentLayout->addWidget(line);

    // 指标行（每项一行，独立配色，紧凑省高度；颜色由主题派生，见 applyThemeColors）
    m_cpuRow = new MetricRow(tr("CPU"), QColor(QStringLiteral("#0081ff")), container);
    contentLayout->addWidget(m_cpuRow);

    m_memRow = new MetricRow(tr("内存"), QColor(QStringLiteral("#00a870")), container);
    contentLayout->addWidget(m_memRow);

    // GPU 行始终创建，可见性由 updateRowsVisibility 控制（降级策略 + 设置项）
    m_gpuAvailable = m_monitor->gpuAvailable();
    m_gpuRow = new MetricRow(tr("GPU"), QColor(QStringLiteral("#ff6b9d")), container);
    contentLayout->addWidget(m_gpuRow);

    m_diskRow = new MetricRow(tr("系统盘"), QColor(QStringLiteral("#ffd93d")), container);
    contentLayout->addWidget(m_diskRow);

    // CPU/GPU 趋势迷你折线（加分项）：窗口始终约 60 秒，缓冲随刷新间隔自适应
    m_sparkline = new Sparkline(container);
    m_sparkline->setSeriesCount(2);
    m_sparkline->setBufferSize(qMax(12, 60000 / qMax(1, m_config->refreshInterval())));
    m_sparkline->setSeriesName(0, tr("CPU"));
    m_sparkline->setSeriesName(1, tr("GPU"));
    contentLayout->addWidget(m_sparkline);

    // 网络
    m_netWidget = new NetWidget(container);
    contentLayout->addWidget(m_netWidget);

    // 番茄钟（加分项，默认隐藏，托盘可切换）
    m_pomodoro = new Pomodoro(container);
    m_pomodoro->setVisible(false);
    contentLayout->addWidget(m_pomodoro);

    // 底部右侧留出宽度调整热区（右下角 16px）
    auto *resizeHint = new QLabel(tr("⟋"), container);
    resizeHint->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    QFont hintFont = resizeHint->font();
    hintFont.setPointSizeF(hintFont.pointSizeF() - 3);
    resizeHint->setFont(hintFont);
    contentLayout->addWidget(resizeHint, 0, Qt::AlignRight);
}

void MonitorWidget::setupTray()
{
    // 图标：优先主题图标，回退程序绘制
    QIcon icon = QIcon::fromTheme(QStringLiteral("deskmon"));
    if (icon.isNull()) {
        QPixmap pm(64, 64);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        // 使用活跃色（跟随主题）
        const QColor accent = DGuiApplicationHelper::instance()->applicationPalette().color(DPalette::Highlight);
        p.setBrush(accent);
        p.drawEllipse(8, 8, 48, 48);
        p.end();
        icon = QIcon(pm);
    }

    auto *tray = new QSystemTrayIcon(icon, this);
    tray->setToolTip(tr("系统监控 DeskMon"));

    auto *menu = new QMenu;
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
        if (on)
            positionWindow();
    });

    // 设置面板
    QAction *settingsAction = menu->addAction(tr("设置"));
    connect(settingsAction, &QAction::triggered, this, &MonitorWidget::openSettings);

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
    m_gpuRow->setVisible(m_gpuAvailable && m_config->showGpu());
    m_diskRow->setVisible(m_config->showDisk());
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
    // 标题电力图标固定金黄色（不随主题色），无需在此更新
    // 网络箭头：上传橙、下载绿
    if (m_netWidget) {
        const auto arrows = ThemeColors::netArrowColors();
        m_netWidget->setArrowColors(arrows.first, arrows.second);
    }
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
    if (cpu >= 0) {
        m_cpuRow->setValue(int(cpu + 0.5));
        m_cpuRow->setInfo(tr("温度 %1℃").arg(int(m_monitor->cpuTemp())));
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
    if (m_gpuRow) {
        const auto gpuStats = m_monitor->gpuStats();
        gpu = gpuStats.util;
        if (gpu >= 0) {
            m_gpuRow->setValue(int(gpu + 0.5));
            m_gpuRow->setInfo(QStringLiteral("%1℃·%2/%3G").arg(int(gpuStats.temp))
                                                .arg(gpuStats.memUsedMB / 1024.0, 0, 'f', 1)
                                                .arg(gpuStats.memTotalMB / 1024.0, 0, 'f', 1));
        }
    }

    // 系统盘
    double diskPercent = 0;
    qint64 diskUsed = 0, diskTotal = 0;
    m_monitor->diskUsage(diskPercent, diskUsed, diskTotal);
    if (diskPercent >= 0) {
        m_diskRow->setValue(int(diskPercent + 0.5));
        m_diskRow->setInfo(QStringLiteral("%1/%2G").arg(diskUsed).arg(diskTotal));
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
    m_pomodoro->setVisible(!m_pomodoro->isVisible());
    if (m_pomodoro->isVisible())
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
    // 进入右下角热区显示调整光标
    const QPoint local = e->position().toPoint();
    const bool inCorner = local.x() >= width() - 16 && local.y() >= height() - 16;
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

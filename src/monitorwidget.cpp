// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#include "monitorwidget.h"

#include "config.h"
#include "systemmonitor.h"
#include "themecolors.h"
#include "settings_dialog.h"
#include "widgets/metricrow.h"
#include "widgets/netwidget.h"

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
#include <QScreen>
#include <QGuiApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QActionGroup>
#include <QFont>
#include <QApplication>
#include <QIcon>

DWIDGET_USE_NAMESPACE

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
    container->setMaskAlpha(150);
    container->setBlendMode(DBlurEffectWidget::BehindWindowBlend);
    mainLayout->addWidget(container);

    auto *contentLayout = new QVBoxLayout(container);
    contentLayout->setContentsMargins(12, 10, 12, 10);
    contentLayout->setSpacing(8);

    // 标题行
    auto *titleLayout = new QHBoxLayout;
    auto *titleLabel = new QLabel(tr("⚡ 系统监控"), container);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
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

    // 网络
    m_netWidget = new NetWidget(container);
    contentLayout->addWidget(m_netWidget);

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
        // 刷新间隔可能被修改，重启定时器
        m_timer->setInterval(m_config->refreshInterval());
        m_timer->start();
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
}

void MonitorWidget::setOpacity(double v)
{
    const double clamped = qBound(0.3, v, 1.0);
    setWindowOpacity(clamped);
    m_config->set(QStringLiteral("opacity"), clamped);
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
    // CPU
    const double cpu = m_monitor->cpuUsage();
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
        const auto gpu = m_monitor->gpuStats();
        if (gpu.util >= 0) {
            m_gpuRow->setValue(int(gpu.util + 0.5));
            m_gpuRow->setInfo(QStringLiteral("%1℃·%2/%3G").arg(int(gpu.temp))
                                                .arg(gpu.memUsedMB / 1024.0, 0, 'f', 1)
                                                .arg(gpu.memTotalMB / 1024.0, 0, 'f', 1));
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

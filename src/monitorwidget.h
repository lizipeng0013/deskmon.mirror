// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DESKMON_MONITORWIDGET_H
#define DESKMON_MONITORWIDGET_H

#include <DWidget>
#include <DBlurEffectWidget>

#include <memory>

class QTimer;
class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class QToolButton;
class QFrame;
class QStackedLayout;
class QAction;
class SystemMonitor;
class Config;
class MetricRow;
class NetWidget;
class Sparkline;
class Pomodoro;
class PowerIcon;
class MiniBar;

/**
 * @brief 主悬浮监控窗口
 *
 * 右下角无边框小窗，可拖动/调整宽度，定时刷新 CPU/内存/GPU/磁盘/网络。
 *
 * 说明：选型从 DMainWindow 改为 DWidget —— DMainWindow 面向带标题栏/侧栏的
 * 标准应用，会强加最小尺寸约束，不适合无边框悬浮小窗；磨砂+圆角效果由
 * DBlurEffectWidget 容器承担，效果一致。
 */
class MonitorWidget : public DTK_WIDGET_NAMESPACE::DWidget
{
    Q_OBJECT
public:
    explicit MonitorWidget(QWidget *parent = nullptr);
    ~MonitorWidget() override;

public slots:
    void activateWindow();          // 单实例唤起

protected:
    void showEvent(QShowEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private slots:
    void refresh();

private:
    void setupUi();
    void setupTray();
    void positionWindow();
    void snapToBottomRight();   // 模式切换后贴右下角（当前屏，4px 吸边）
    void savePosition();
    void applyConfig();
    void applyThemeColors();      // 主题跟随：重算并套用指标色
    void setOpacity(double v);    // 设置窗口透明度并持久化
    void updateRowsVisibility();  // 按 show_gpu/show_disk 显示/隐藏指标行
    void applyStayOnTop();        // 按配置应用窗口置顶
    void openSettings();          // 打开设置面板并应用更改
    void togglePomodoro();        // 托盘切换番茄钟行显隐
    void checkAlerts(double cpu, double gpu);  // 阈值告警（边沿触发）
    void notifyAlert(const QString &message);  // 系统通知发告警
    void cycleOpacity();          // 点击 ◐ 循环切换透明度
    void setDisplayMode(const QString &mode);  // 切换完整/迷你模式

    std::unique_ptr<Config> m_config;
    std::unique_ptr<SystemMonitor> m_monitor;
    QTimer *m_timer = nullptr;

    // 拖动状态
    QPoint m_dragPos;
    bool m_dragging = false;
    // 宽度调整状态（右下角热区）
    bool m_resizing = false;
    QPoint m_resizeStartPos;
    int m_resizeStartWidth = 0;

    // UI（每项一行：点+名称+百分比+附加信息+细进度条，独立配色）
    MetricRow *m_cpuRow = nullptr;
    MetricRow *m_memRow = nullptr;
    MetricRow *m_gpuRow = nullptr;
    MetricRow *m_diskRow = nullptr;
    NetWidget *m_netWidget = nullptr;
    Sparkline *m_sparkline = nullptr;   // CPU/GPU 60s 趋势（加分项）
    Pomodoro *m_pomodoro = nullptr;     // 番茄钟（加分项，默认隐藏）
    PowerIcon *m_titleIcon = nullptr;   // 标题电力图标（金色）
    QToolButton *m_opacityBtn = nullptr; // 标题 ◐ 透明度切换
    QToolButton *m_collapseBtn = nullptr; // 标题 ⊟ 收缩为迷你条

    QFrame *m_titleSep = nullptr;       // 标题与指标区分隔线
    QFrame *m_metricsSep = nullptr;     // 指标区与趋势区分隔线
    QFrame *m_netTopSep = nullptr;      // 趋势图与网络区分隔线
    QFrame *m_netSep = nullptr;         // 网络区与番茄钟分隔线

    QWidget *m_fullPanel = nullptr;     // 完整模式内容容器（QStackedLayout 页 1）
    MiniBar *m_miniBar = nullptr;        // 迷你模式浮动条（QStackedLayout 页 2）
    QStackedLayout *m_stack = nullptr;  // full/mini 内容互换
    DTK_WIDGET_NAMESPACE::DBlurEffectWidget *m_container = nullptr;  // 磨砂容器（切模式时调圆角）

    QAction *m_fullModeAct = nullptr;   // 托盘「显示模式」勾选项，随 setDisplayMode 同步
    QAction *m_miniModeAct = nullptr;

    bool m_gpuAvailable = false;
    bool m_cpuAlerted = false;          // 告警边沿状态
    bool m_gpuAlerted = false;
};

#endif // DESKMON_MONITORWIDGET_H

// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKMON_MONITORWIDGET_H
#define DESKMON_MONITORWIDGET_H

#include <DWidget>

#include <memory>

class QTimer;
class QLabel;
class QVBoxLayout;
class SystemMonitor;
class Config;
class MetricRow;
class NetWidget;

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
    void savePosition();
    void applyConfig();
    void applyThemeColors();      // 主题跟随：重算并套用指标色
    void setOpacity(double v);    // 设置窗口透明度并持久化
    void updateRowsVisibility();  // 按 show_gpu/show_disk 显示/隐藏指标行
    void applyStayOnTop();        // 按配置应用窗口置顶
    void openSettings();          // 打开设置面板并应用更改

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

    bool m_gpuAvailable = false;
};

#endif // DESKMON_MONITORWIDGET_H

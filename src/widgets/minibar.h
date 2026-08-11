// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DESKMON_MINIBAR_H
#define DESKMON_MINIBAR_H

#include <DWidget>

#include <QColor>
#include <QLabel>

class QToolButton;

/**
 * @brief 迷你浮动条：横向胶囊，单行排开各指标百分比 + 网络速度 + 展开按钮
 *
 * 与完整窗（FullPanel）并列，由 MonitorWidget 用 QStackedLayout 切换。
 * 数据与配色与完整窗同源（SystemMonitor / ThemeColors），仅呈现更紧凑。
 *
 * 精简策略：纯圆点+数字（无名称），靠圆点颜色区分指标，悬停 tooltip 显示名称。
 * 按钮只留 ⊞ 展开；透明度/隐藏走托盘菜单。双击空白处也可展开。
 */
class MiniBar : public DTK_WIDGET_NAMESPACE::DWidget
{
    Q_OBJECT
public:
    explicit MiniBar(QWidget *parent = nullptr);

    // idx: 0=CPU 1=内存 2=GPU 3=系统盘
    void setMetric(int idx, int percent);
    void setMetricInfo(int idx, const QString &info);  // 附加信息（温度/容量），合入 tooltip
    void setMetricColor(int idx, const QColor &color);
    void setRowsVisible(bool gpu, bool disk);   // 跟随降级 + 设置项

    void setNetSpeed(double uploadBps, double downloadBps);
    void setNetColors(const QColor &up, const QColor &down);

    void updateLabelColors();   // 主题切换后刷新提示色

signals:
    void expandRequested();     // ⊞ 按钮 / 双击空白
    void opacityClicked();
    void hideRequested();

protected:
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    struct Cell {
        QWidget *container = nullptr;  // 容器，tooltip 挂这里，整个区域悬停都显示
        QLabel *dot = nullptr;
        QLabel *letter = nullptr;   // 英文首字母（C/M/G/D）
        QLabel *percent = nullptr;
        QColor color;
        QString name;     // tooltip 用
        QString info;     // 附加信息（温度/容量等），合入 tooltip
    };
    void buildCell(Cell &c, const QString &name, const QString &letter, QWidget *parent);

    Cell m_cells[4];   // CPU / 内存 / GPU / 系统盘
    QLabel *m_upArrow = nullptr;
    QLabel *m_up = nullptr;
    QLabel *m_downArrow = nullptr;
    QLabel *m_down = nullptr;
    QToolButton *m_expandBtn = nullptr;

    QString formatSpeed(double bps) const;
};

#endif // DESKMON_MINIBAR_H

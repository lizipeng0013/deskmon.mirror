// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKMON_METRICROW_H
#define DESKMON_METRICROW_H

#include <DWidget>
#include <DProgressBar>

#include <QColor>
#include <QLabel>
#include <QVBoxLayout>

/**
 * @brief 紧凑指标行：彩色圆点 + 名称 + 百分比 + 附加信息，下方细进度条
 *
 * 相比环形/条形控件占用高度更小，每个指标用独立颜色区分。
 * 进度条颜色通过 DPaletteHelper 的 Highlight 角色设置（DTK 语义化方式）。
 */
class MetricRow : public DTK_WIDGET_NAMESPACE::DWidget
{
    Q_OBJECT
public:
    explicit MetricRow(const QString &title, const QColor &color, QWidget *parent = nullptr);

    void setValue(int percent);            // 0-100，更新进度条与百分比
    void setInfo(const QString &text);     // 右侧附加信息（温度/容量等）
    void setColor(const QColor &color);    // 主题跟随：更新点/百分比/进度条颜色

private:
    QLabel *m_dot = nullptr;      // 彩色圆点
    QLabel *m_title = nullptr;
    QLabel *m_percent = nullptr;  // 彩色百分比
    QLabel *m_info = nullptr;
    DTK_WIDGET_NAMESPACE::DProgressBar *m_bar = nullptr;
};

#endif // DESKMON_METRICROW_H

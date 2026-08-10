// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKMON_SPARKLINE_H
#define DESKMON_SPARKLINE_H

#include <DWidget>

#include <QColor>
#include <QVector>

/**
 * @brief 迷你折线图：多序列 60 秒趋势
 *
 * 每条序列维护固定长度环形缓冲，抗锯齿折线绘制 + 末端圆点 + 网格。
 */
class Sparkline : public DTK_WIDGET_NAMESPACE::DWidget
{
    Q_OBJECT
public:
    explicit Sparkline(QWidget *parent = nullptr);

    void setSeriesCount(int count);
    void setColor(int series, const QColor &color);
    void setValue(int series, double value);   // 0-100
    void clear();

    int bufferSize() const { return m_bufferSize; }

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    static constexpr int kMaxSeries = 4;
    static constexpr int m_bufferSize = 60;

    int m_seriesCount = 0;
    QVector<QVector<double>> m_data;      // [series][0..59] 最新在末尾
    QVector<int> m_sampleCounts;          // 每序列已采集样本数
    QVector<QColor> m_colors;
};

#endif // DESKMON_SPARKLINE_H

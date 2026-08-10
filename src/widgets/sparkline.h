// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

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
    void setSeriesName(int series, const QString &name);  // 图例标签
    void setValue(int series, double value);   // 0-100
    void clear();

    // 缓冲样本数随刷新间隔自适应：窗口保持约 60 秒
    void setBufferSize(int size);
    int bufferSize() const { return m_bufferSize; }

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    static constexpr int kMaxSeries = 4;
    int m_bufferSize = 60;   // 默认 60 样本（1s 间隔 = 60 秒窗口）

    int m_seriesCount = 0;
    QVector<QVector<double>> m_data;      // [series][0..59] 最新在末尾
    QVector<int> m_sampleCounts;          // 每序列已采集样本数
    QVector<QColor> m_colors;
    QVector<QString> m_names;             // 图例标签（右上角）
};

#endif // DESKMON_SPARKLINE_H

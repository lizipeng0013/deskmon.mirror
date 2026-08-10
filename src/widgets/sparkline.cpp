// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sparkline.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QLinearGradient>

DWIDGET_USE_NAMESPACE

Sparkline::Sparkline(QWidget *parent)
    : DWidget(parent)
{
    setMinimumHeight(36);
}

void Sparkline::setSeriesCount(int count)
{
    m_seriesCount = qBound(0, count, kMaxSeries);
    m_data = QVector<QVector<double>>(m_seriesCount, QVector<double>(m_bufferSize, 0.0));
    m_sampleCounts = QVector<int>(m_seriesCount, 0);
    if (m_colors.size() < m_seriesCount)
        m_colors.resize(m_seriesCount);
    update();
}

void Sparkline::setColor(int series, const QColor &color)
{
    if (series < 0 || series >= m_seriesCount)
        return;
    m_colors[series] = color;
    update();
}

void Sparkline::setValue(int series, double value)
{
    if (series < 0 || series >= m_seriesCount)
        return;
    QVector<double> &buf = m_data[series];
    // 环形缓冲：整体左移一格，最新值入末尾
    for (int i = 1; i < m_bufferSize; ++i)
        buf[i - 1] = buf[i];
    buf[m_bufferSize - 1] = qBound(0.0, value, 100.0);
    m_sampleCounts[series] = qMin(m_bufferSize, m_sampleCounts[series] + 1);
    update();
}

void Sparkline::clear()
{
    for (auto &buf : m_data)
        buf.fill(0.0);
    m_sampleCounts.fill(0);
    update();
}

void Sparkline::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const double w = width(), h = height();
    const double topPad = 3, bottomPad = 3;

    // 网格线：0% / 50% / 100%
    p.setPen(QPen(QColor(255, 255, 255, 18), 1));
    for (double frac : {0.0, 0.5, 1.0}) {
        const double y = topPad + (h - topPad - bottomPad) * (1.0 - frac);
        p.drawLine(QPointF(0, y), QPointF(w, y));
    }

    for (int s = 0; s < m_seriesCount; ++s) {
        if (s >= m_colors.size() || !m_colors[s].isValid())
            continue;
        const QVector<double> &buf = m_data[s];

        // 只绘制已采集的样本（n 不足 60 时铺满整个宽度，形成“从左填充”效果）
        const int n = m_sampleCounts[s];
        if (n < 2)
            continue;

        QPainterPath path;
        // 环形缓冲最新在尾部：样本 i 实际位于 buf[m_bufferSize - n + i]
        const int base = m_bufferSize - n;
        for (int i = 0; i < n; ++i) {
            const double x = (n == 1) ? 0 : w * double(i) / double(n - 1);
            const double y = topPad + (h - topPad - bottomPad) * (1.0 - buf[base + i] / 100.0);
            if (i == 0)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
        }

        // 主序列（第一条）加渐变填充，提升质感
        if (s == 0) {
            QPainterPath fillPath = path;
            fillPath.lineTo(w, h - bottomPad);
            fillPath.lineTo(0, h - bottomPad);
            fillPath.closeSubpath();
            QLinearGradient grad(0, topPad, 0, h - bottomPad);
            QColor base = m_colors[s];
            base.setAlpha(70);
            grad.setColorAt(0, base);
            QColor transparent = m_colors[s];
            transparent.setAlpha(0);
            grad.setColorAt(1, transparent);
            p.fillPath(fillPath, grad);
        }

        // 折线（1.5px）
        QColor lineColor = m_colors[s];
        lineColor.setAlpha(220);
        p.setPen(QPen(lineColor, 1.5));
        p.drawPath(path);

        // 末端圆点
        const double lastY = topPad + (h - topPad - bottomPad) * (1.0 - buf[m_bufferSize - 1] / 100.0);
        p.setBrush(lineColor);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(w - 0.5, lastY), 2.2, 2.2);
    }
}

void Sparkline::resizeEvent(QResizeEvent *e)
{
    DWidget::resizeEvent(e);
    update();
}

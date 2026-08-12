// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sparkline.h"

#include <DPalette>
#include <DPaletteHelper>
#include <DGuiApplicationHelper>

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QLinearGradient>
#include <QFontMetrics>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

Sparkline::Sparkline(QWidget *parent)
    : DWidget(parent)
{
    setMinimumHeight(52);
}

void Sparkline::setSeriesCount(int count)
{
    m_seriesCount = qBound(0, count, kMaxSeries);
    m_data = QVector<QVector<double>>(m_seriesCount, QVector<double>(m_bufferSize, 0.0));
    m_sampleCounts = QVector<int>(m_seriesCount, 0);
    if (m_colors.size() < m_seriesCount)
        m_colors.resize(m_seriesCount);
    if (m_names.size() < m_seriesCount)
        m_names.resize(m_seriesCount);
    update();
}

void Sparkline::setColor(int series, const QColor &color)
{
    if (series < 0 || series >= m_seriesCount)
        return;
    m_colors[series] = color;
    update();
}

void Sparkline::setSeriesName(int series, const QString &name)
{
    if (series < 0 || series >= m_seriesCount)
        return;
    m_names[series] = name;
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

void Sparkline::setBufferSize(int size)
{
    size = qMax(10, size);
    if (size == m_bufferSize)
        return;
    // 调整缓冲，保留尾部最新样本
    for (int s = 0; s < m_seriesCount; ++s) {
        QVector<double> nb(size, 0.0);
        const int keep = qMin(size, m_bufferSize);
        const int srcStart = m_bufferSize - keep;
        const int dstStart = size - keep;
        for (int i = 0; i < keep; ++i)
            nb[dstStart + i] = m_data[s][srcStart + i];
        m_data[s] = nb;
        m_sampleCounts[s] = qMin(m_sampleCounts[s], size);
    }
    m_bufferSize = size;
    update();
}

void Sparkline::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const bool dark = DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType;
    const DPalette pal = DPaletteHelper::instance()->palette(this);
    const QColor textColor = pal.color(DPalette::TextTips);
    const QColor frameColor = pal.color(DPalette::FrameBorder);

    const double w = width(), h = height();
    const double leftPad = 22;     // Y 轴标签
    const double rightPad = 4;
    const double topPad = 4;       // 顶部留一点给图例
    const double bottomPad = 13;   // X 轴标签
    const double plotW = qMax(10.0, w - leftPad - rightPad);
    const double plotH = qMax(10.0, h - topPad - bottomPad);

    // 背景框：轻微底色 + 圆角边框（增强 alpha，提升区块边界感）
    const QRectF bgRect(0, 0, w, h);
    QColor bgColor = frameColor;
    bgColor.setAlpha(dark ? 55 : 35);
    p.setPen(Qt::NoPen);
    p.setBrush(bgColor);
    p.drawRoundedRect(bgRect, 6, 6);
    QColor borderColor = frameColor;
    borderColor.setAlpha(dark ? 90 : 55);
    p.setPen(QPen(borderColor, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);

    // 网格线：0% / 50% / 100%
    QColor gridColor = dark ? QColor(255, 255, 255, 25) : QColor(0, 0, 0, 18);
    p.setPen(QPen(gridColor, 1));
    for (double frac : {0.0, 0.5, 1.0}) {
        const double y = topPad + plotH * (1.0 - frac);
        p.drawLine(QPointF(leftPad, y), QPointF(leftPad + plotW, y));
    }

    // Y 轴标签：100 / 0
    {
        QFont axisFont = font();
        axisFont.setPointSizeF(axisFont.pointSizeF() - 3);
        p.setFont(axisFont);
        p.setPen(textColor);
        const QFontMetrics fm(axisFont);
        const int fh = fm.ascent();
        p.drawText(QRectF(0, topPad - 3, leftPad - 3, fh), Qt::AlignRight | Qt::AlignTop, QStringLiteral("100"));
        p.drawText(QRectF(0, topPad + plotH - fh + 3, leftPad - 3, fh), Qt::AlignRight | Qt::AlignBottom, QStringLiteral("0"));
    }

    // X 轴标签：-60s / 现在
    {
        QFont axisFont = font();
        axisFont.setPointSizeF(axisFont.pointSizeF() - 3);
        p.setFont(axisFont);
        p.setPen(textColor);
        const QFontMetrics fm(axisFont);
        const int fh = fm.height();
        p.drawText(QRectF(leftPad, h - bottomPad + 1, 30, fh), Qt::AlignLeft | Qt::AlignTop, QStringLiteral("-60s"));
        p.drawText(QRectF(leftPad + plotW - 30, h - bottomPad + 1, 30, fh), Qt::AlignRight | Qt::AlignTop, QStringLiteral("现在"));
    }

    // 图例（右上角）：彩色圆点 + 标签
    {
        QFont legendFont = font();
        legendFont.setPointSizeF(legendFont.pointSizeF() - 3);
        p.setFont(legendFont);
        const QFontMetrics fm(legendFont);
        double x = leftPad + plotW - 2;
        for (int s = m_seriesCount - 1; s >= 0; --s) {
            if (s >= m_names.size() || m_names[s].isEmpty())
                continue;
            const double textW = fm.horizontalAdvance(m_names[s]);
            const double itemW = textW + 9;
            x -= itemW;
            p.setPen(Qt::NoPen);
            p.setBrush(m_colors[s].isValid() ? m_colors[s] : Qt::gray);
            p.drawEllipse(QPointF(x + 2, topPad + 6), 2.5, 2.5);
            p.setPen(m_colors[s].isValid() ? m_colors[s] : Qt::gray);
            p.drawText(QPointF(x + 7, topPad + 9), m_names[s]);
            x -= 2;
        }
    }

    for (int s = 0; s < m_seriesCount; ++s) {
        if (s >= m_colors.size() || !m_colors[s].isValid())
            continue;
        const QVector<double> &buf = m_data[s];

        const int n = m_sampleCounts[s];
        if (n < 2)
            continue;

        QPainterPath path;
        const int base = m_bufferSize - n;
        double peakX = 0, peakY = 0;
        double peakValue = -1;
        for (int i = 0; i < n; ++i) {
            const double x = leftPad + ((n == 1) ? 0 : plotW * double(i) / double(n - 1));
            const double y = topPad + plotH * (1.0 - buf[base + i] / 100.0);
            if (i == 0)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
            if (buf[base + i] > peakValue) {
                peakValue = buf[base + i];
                peakX = x;
                peakY = y;
            }
        }

        // 主序列（第一条）加渐变填充
        if (s == 0) {
            QPainterPath fillPath = path;
            fillPath.lineTo(leftPad + plotW, topPad + plotH);
            fillPath.lineTo(leftPad, topPad + plotH);
            fillPath.closeSubpath();
            QLinearGradient grad(0, topPad, 0, topPad + plotH);
            QColor baseColor = m_colors[s];
            baseColor.setAlpha(dark ? 90 : 70);
            grad.setColorAt(0, baseColor);
            QColor transparent = m_colors[s];
            transparent.setAlpha(0);
            grad.setColorAt(1, transparent);
            p.fillPath(fillPath, grad);
        }

        // 折线
        QColor lineColor = m_colors[s];
        lineColor.setAlpha(dark ? 235 : 220);
        p.setPen(QPen(lineColor, 1.5));
        p.drawPath(path);

        // 末端圆点
        const double lastY = topPad + plotH * (1.0 - buf[m_bufferSize - 1] / 100.0);
        p.setBrush(lineColor);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(leftPad + plotW - 0.5, lastY), 2.2, 2.2);

        // 峰值标记
        if (peakValue >= 0) {
            QColor peakColor = m_colors[s];
            peakColor.setAlpha(180);
            p.setBrush(peakColor);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(peakX, peakY), 2.5, 2.5);
        }
    }
}

void Sparkline::resizeEvent(QResizeEvent *e)
{
    DWidget::resizeEvent(e);
    update();
}

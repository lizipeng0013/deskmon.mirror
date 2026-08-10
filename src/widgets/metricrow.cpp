// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#include "metricrow.h"

#include <DPaletteHelper>
#include <DPalette>

#include <QFont>
#include <QFontMetrics>

DWIDGET_USE_NAMESPACE

MetricRow::MetricRow(const QString &title, const QColor &color, QWidget *parent)
    : DWidget(parent)
{
    // 彩色圆点 + 名称
    m_dot = new QLabel(QStringLiteral("●"), this);
    m_dot->setFixedWidth(12);

    m_title = new QLabel(title, this);
    QFont titleFont = m_title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() - 1);
    m_title->setFont(titleFont);

    // 百分比（用指标色强调）
    m_percent = new QLabel(QStringLiteral("0%"), this);
    QFont percentFont = m_percent->font();
    percentFont.setPointSizeF(percentFont.pointSizeF() - 1);
    m_percent->setFont(percentFont);

    // 附加信息（温度/容量）
    m_info = new QLabel(this);
    QFont infoFont = m_info->font();
    infoFont.setPointSizeF(infoFont.pointSizeF() - 2);
    m_info->setFont(infoFont);

    auto *head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(2);
    head->addWidget(m_dot);
    head->addWidget(m_title);
    head->addStretch();
    head->addWidget(m_percent);
    head->addSpacing(2);
    head->addWidget(m_info);

    // 细进度条（不显示文字，颜色由 Highlight 角色控制）
    m_bar = new DProgressBar(this);
    m_bar->setRange(0, 100);
    m_bar->setValue(0);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(7);
    m_bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);
    layout->addLayout(head);
    layout->addWidget(m_bar);

    setColor(color);
}

void MetricRow::setColor(const QColor &color)
{
    // 圆点与百分比用指标色（装饰性强调，随主题动态更新）
    const QString css = QStringLiteral("color: %1;").arg(color.name());
    m_dot->setStyleSheet(css + QStringLiteral(" font-size: 8px;"));
    m_percent->setStyleSheet(css);

    // 进度条 Highlight 角色（DTK 语义化方式）
    DPalette pal = DPaletteHelper::instance()->palette(m_bar);
    pal.setColor(QPalette::Highlight, color);
    DPaletteHelper::instance()->setPalette(m_bar, pal);
}

void MetricRow::setValue(int percent)
{
    const int v = qBound(0, percent, 100);
    m_bar->setValue(v);
    m_percent->setText(QStringLiteral("%1%").arg(v));
}

void MetricRow::setInfo(const QString &text)
{
    m_info->setText(text);
    m_info->setToolTip(text);
    m_info->setVisible(!text.isEmpty());
    // 超宽省略号截断，防止撑宽
    const QFontMetrics fm(m_info->font());
    m_info->setText(fm.elidedText(text, Qt::ElideRight, qMax(40, width() - 80)));
    m_info->setToolTip(text);
}

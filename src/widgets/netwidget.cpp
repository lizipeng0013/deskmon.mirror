// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "netwidget.h"

#include <DPalette>
#include <DPaletteHelper>

#include <QFont>
#include <QFontMetrics>
#include <QStringList>
#include <QColor>

DWIDGET_USE_NAMESPACE

NetWidget::NetWidget(QWidget *parent)
    : DWidget(parent)
{
    // 上传 / 下载标签与数值分离，便于对齐和着色
    m_upLabel = new QLabel(tr("上传"), this);
    m_downLabel = new QLabel(tr("下载"), this);
    m_ipLabel = new QLabel(tr("IP"), this);

    m_up = new QLabel(QStringLiteral("—"), this);
    m_down = new QLabel(QStringLiteral("—"), this);
    m_ips = new QLabel(this);
    m_ips->setToolTip(QString());

    QFont labelFont = m_upLabel->font();
    labelFont.setPointSizeF(labelFont.pointSizeF() - 1);
    QFont valueFont = m_up->font();
    valueFont.setPointSizeF(valueFont.pointSizeF() - 1);
    for (QLabel *l : {m_upLabel, m_downLabel, m_ipLabel}) {
        l->setFont(labelFont);
    }

    for (QLabel *l : {m_up, m_down, m_ips}) {
        l->setFont(valueFont);
    }

    // 上传 / 下载：两列等宽
    auto *upRow = new QHBoxLayout;
    upRow->setSpacing(3);
    upRow->addWidget(m_upLabel);
    upRow->addWidget(m_up);
    upRow->addStretch();

    auto *downRow = new QHBoxLayout;
    downRow->setSpacing(3);
    downRow->addWidget(m_downLabel);
    downRow->addWidget(m_down);
    downRow->addStretch();

    auto *speedGrid = new QHBoxLayout;
    speedGrid->setSpacing(12);
    speedGrid->addLayout(upRow, 1);
    speedGrid->addLayout(downRow, 1);

    // IP 行
    auto *ipRow = new QHBoxLayout;
    ipRow->setSpacing(4);
    ipRow->addWidget(m_ipLabel);
    ipRow->addWidget(m_ips);
    ipRow->addStretch();

    // 首次设置标签色（后续由 updateLabelColors 在主题切换时刷新）
    updateLabelColors();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);
    layout->addLayout(speedGrid);
    layout->addLayout(ipRow);
}

QString NetWidget::formatSpeed(double bps)
{
    if (bps < 1024)
        return QStringLiteral("%1 B/s").arg(bps, 0, 'f', 0);
    if (bps < 1024 * 1024)
        return QStringLiteral("%1 KB/s").arg(bps / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB/s").arg(bps / (1024.0 * 1024.0), 0, 'f', 2);
}

void NetWidget::setSpeed(double uploadBps, double downloadBps)
{
    m_up->setText(formatSpeed(uploadBps));
    m_down->setText(formatSpeed(downloadBps));
}

void NetWidget::setArrowColors(const QColor &up, const QColor &down)
{
    // 速度数值着色：上传橙、下载绿；IP 保持提示色
    m_up->setStyleSheet(QStringLiteral("color: %1;").arg(up.name()));
    m_down->setStyleSheet(QStringLiteral("color: %1;").arg(down.name()));
}

void NetWidget::setIps(const QStringList &ips)
{
    const QString joined = ips.join(QStringLiteral("  "));
    m_ips->setToolTip(joined);
    // 省略号截断，防止 IP 撑宽小窗
    const QFontMetrics fm(m_ips->font());
    const int maxWidth = qMax(60, width() - 60);
    m_ips->setText(fm.elidedText(joined, Qt::ElideRight, maxWidth));
    m_ips->setVisible(!joined.isEmpty());
}

void NetWidget::updateLabelColors()
{
    const DPalette pal = DPaletteHelper::instance()->palette(this);
    const QColor tipsColor = pal.color(DPalette::TextTips);
    const QString tipsCss = QStringLiteral("color: %1;").arg(
        tipsColor.isValid() ? tipsColor.name() : QStringLiteral("#909399"));
    m_upLabel->setStyleSheet(tipsCss);
    m_downLabel->setStyleSheet(tipsCss);
    m_ipLabel->setStyleSheet(tipsCss);
    m_ips->setStyleSheet(tipsCss);
}

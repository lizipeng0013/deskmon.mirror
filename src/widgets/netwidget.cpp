// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#include "netwidget.h"

#include <QFont>
#include <QFontMetrics>
#include <QStringList>

DWIDGET_USE_NAMESPACE

NetWidget::NetWidget(QWidget *parent)
    : DWidget(parent)
{
    m_up = new QLabel(tr("↑ 0 B/s"), this);
    m_down = new QLabel(tr("↓ 0 B/s"), this);
    m_ips = new QLabel(this);
    m_ips->setToolTip(QString());

    QFont f = m_up->font();
    f.setPointSizeF(f.pointSizeF() - 1);
    m_up->setFont(f);
    m_down->setFont(f);
    m_ips->setFont(f);

    // 第一行：上/下行速度；第二行：IP（超宽省略号截断，不撑宽窗口）
    auto *speedRow = new QHBoxLayout;
    speedRow->setSpacing(6);
    speedRow->addWidget(m_up);
    speedRow->addWidget(m_down);
    speedRow->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(2);
    layout->addLayout(speedRow);
    layout->addWidget(m_ips);
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
    m_up->setText(QStringLiteral("↑ %1").arg(formatSpeed(uploadBps)));
    m_down->setText(QStringLiteral("↓ %1").arg(formatSpeed(downloadBps)));
}

void NetWidget::setIps(const QStringList &ips)
{
    const QString joined = ips.join(QStringLiteral("  "));
    m_ips->setToolTip(joined);
    // 省略号截断，防止 IP 撑宽小窗
    const QFontMetrics fm(m_ips->font());
    const int maxWidth = qMax(60, width() - 20);
    m_ips->setText(fm.elidedText(joined, Qt::ElideRight, maxWidth));
    m_ips->setVisible(!joined.isEmpty());
}

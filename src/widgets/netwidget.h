// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKMON_NETWIDGET_H
#define DESKMON_NETWIDGET_H

#include <DWidget>

#include <QLabel>
#include <QHBoxLayout>

/**
 * @brief 网络组件：上/下行速度 + IP 地址
 */
class NetWidget : public DTK_WIDGET_NAMESPACE::DWidget
{
    Q_OBJECT
public:
    explicit NetWidget(QWidget *parent = nullptr);

    void setSpeed(double uploadBps, double downloadBps);
    void setIps(const QStringList &ips);
    void setArrowColors(const QColor &up, const QColor &down);  // 主题跟随

private:
    static QString formatSpeed(double bps);

    QLabel *m_up = nullptr;
    QLabel *m_down = nullptr;
    QLabel *m_ips = nullptr;
};

#endif // DESKMON_NETWIDGET_H

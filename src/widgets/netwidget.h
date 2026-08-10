// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

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
    void updateLabelColors();   // 主题切换后更新标签/IP 的提示色

private:
    static QString formatSpeed(double bps);

    QLabel *m_up = nullptr;
    QLabel *m_down = nullptr;
    QLabel *m_ips = nullptr;
    QLabel *m_upLabel = nullptr;
    QLabel *m_downLabel = nullptr;
    QLabel *m_ipLabel = nullptr;
};

#endif // DESKMON_NETWIDGET_H

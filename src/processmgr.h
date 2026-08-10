// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKMON_PROCESSMGR_H
#define DESKMON_PROCESSMGR_H

#include <QObject>
#include <QVector>
#include <QHash>

/**
 * @brief 进程查询与管理（/proc 原生读取，无需 psutil）
 */
struct ProcessInfo
{
    int pid = 0;
    QString name;
    double cpuPercent = 0;   // 进程 CPU 占用率（%）
    double memPercent = 0;   // 进程内存占用率（%）
};

class ProcessMgr : public QObject
{
    Q_OBJECT
public:
    explicit ProcessMgr(QObject *parent = nullptr);

    // 全量进程列表（CPU% 需两次调用间差分，首次返回 0）
    QVector<ProcessInfo> list();

    // 终止进程：SIGTERM → 等待 → SIGKILL；返回是否成功
    static bool kill(int pid);

private:
    // pid -> (进程累计 jiffies, 记录时刻 msec)
    QHash<int, QPair<qulonglong, qint64>> m_cpuCache;
};

#endif // DESKMON_PROCESSMGR_H

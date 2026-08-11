// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "processmgr.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QDateTime>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>

#include <signal.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>

namespace {

// /proc/[pid]/stat 字段：comm(2) 是带括号名称，utime(14) stime(15)
bool readStat(const QString &path, QString &name, qulonglong &utime, qulonglong &stime)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray raw = f.readAll();
    // comm 含空格/括号，用最后一个 ')' 定位字段起点
    const int close = raw.lastIndexOf(')');
    if (close <= 0)
        return false;
    name = QString::fromUtf8(raw.mid(1, close - 1));   // 去掉开头 '('
    const QList<QByteArray> rest = raw.mid(close + 1).trimmed().split(' ');
    // rest[0]=state(3) ... rest[11]=utime(14) rest[12]=stime(15)
    if (rest.size() < 13)
        return false;
    utime = rest.at(11).toULongLong();
    stime = rest.at(12).toULongLong();
    return true;
}

// /proc/[pid]/status 的 VmRSS（kB）
qulonglong readVmRss(const QString &statusPath)
{
    QFile f(statusPath);
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    const QList<QByteArray> lines = f.readAll().split('\n');
    for (const QByteArray &line : lines) {
        if (line.startsWith("VmRSS:"))
            return line.mid(6).trimmed().split(' ').value(0).toULongLong();
    }
    return 0;
}

} // namespace

ProcessMgr::ProcessMgr(QObject *parent)
    : QObject(parent)
{
}

QVector<ProcessInfo> ProcessMgr::list()
{
    QVector<ProcessInfo> result;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 总内存（字节）
    struct sysinfo si {};
    const qulonglong totalMem = (sysinfo(&si) == 0) ? qulonglong(si.totalram) * si.mem_unit : 1;

    const QDir procDir(QStringLiteral("/proc"));
    const QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QSet<int> seen;
    seen.reserve(entries.size());

    for (const QString &entry : entries) {
        bool ok = false;
        const int pid = entry.toInt(&ok);
        if (!ok || pid <= 0)
            continue;
        seen.insert(pid);

        const QString base = procDir.filePath(entry);
        QString name;
        qulonglong utime = 0, stime = 0;
        if (!readStat(base + QStringLiteral("/stat"), name, utime, stime))
            continue;

        ProcessInfo info;
        info.pid = pid;
        info.name = name;
        info.memPercent = 100.0 * double(readVmRss(base + QStringLiteral("/status")) * 1024) / double(totalMem);

        // CPU%：与上次取样的差分 / 时间差，再乘核数（与 psutil 语义一致）
        const qulonglong jiffies = utime + stime;
        const auto it = m_cpuCache.constFind(pid);
        if (it != m_cpuCache.constEnd()) {
            const qint64 dtMs = qMax<qint64>(1, now - it->second);
            const qulonglong dj = jiffies - it->first;
            const double jiffiesPerSec = 1000.0 * double(dj) / double(dtMs);
            info.cpuPercent = qBound(0.0, jiffiesPerSec / double(sysconf(_SC_CLK_TCK)) / double(QThread::idealThreadCount()) * 100.0, 999.0);
        }
        m_cpuCache.insert(pid, {jiffies, now});
        result.append(info);
    }

    // 清理已退出进程的缓存
    for (auto it = m_cpuCache.begin(); it != m_cpuCache.end();) {
        if (!seen.contains(it.key()))
            it = m_cpuCache.erase(it);
        else
            ++it;
    }
    return result;
}

void ProcessMgr::kill(int pid)
{
    if (::kill(pid, SIGTERM) != 0)
        return;
    // 3s 后若仍存活则 SIGKILL 兜底（非阻塞：QTimer 在主线程事件循环触发，
    // 不再像旧实现那样用 QThread::msleep 阻塞 UI 长达 3s/进程）。
    auto *t = new QTimer();
    t->setSingleShot(true);
    t->setInterval(3000);
    QObject::connect(t, &QTimer::timeout, t, [t, pid]() {
        if (QFile::exists(QStringLiteral("/proc/%1").arg(pid)))
            ::kill(pid, SIGKILL);
        t->deleteLater();
    });
    t->start();
}

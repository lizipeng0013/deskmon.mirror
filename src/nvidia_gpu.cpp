// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "nvidia_gpu.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QFile>
#include <QDebug>
#include <QThreadPool>
#include <QPointer>

namespace {

// 在 PATH 中查找 nvidia-smi；找不到返回空。仅做文件存在判断，不阻塞。
QString locateNvidiaSmi()
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString &dir : env.value(QStringLiteral("PATH")).split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        if (QFile::exists(dir + QStringLiteral("/nvidia-smi")))
            return dir + QStringLiteral("/nvidia-smi");
    }
    return {};
}

// 解析 --query-gpu 的 csv 输出（无表头无单位），返回与 fields 等长的列表
QStringList parseQuery(const QByteArray &output, const QStringList &fields)
{
    const QStringList parts = QString::fromUtf8(output).trimmed().split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList out;
    for (int i = 0; i < fields.size(); ++i)
        out << (i < parts.size() ? parts[i].trimmed() : QString());
    return out;
}

// 解析 --query-compute-apps 的 csv 输出
QVector<NvidiaGpu::GpuProcess> parseProcesses(const QByteArray &output)
{
    QVector<NvidiaGpu::GpuProcess> result;
    const QStringList lines = QString::fromUtf8(output).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList parts = line.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (parts.size() < 3)
            continue;
        bool ok = false;
        const int pid = parts[0].trimmed().toInt(&ok);
        if (!ok)
            continue;
        NvidiaGpu::GpuProcess p;
        p.pid = pid;
        p.name = parts[1].trimmed();
        p.memoryMB = parts[2].trimmed().toLongLong();
        result.append(p);
    }
    return result;
}

} // namespace

NvidiaGpu::NvidiaGpu(QObject *parent)
    : QObject(parent)
{
}

bool NvidiaGpu::available() const { return m_available; }
QString NvidiaGpu::name() const { return m_name; }

// ---------------- 异步探测 ----------------

void NvidiaGpu::probeAsync()
{
    // PATH 遍历很快（仅文件存在判断），放主线程即可；真正慢的是 nvidia-smi 调用，
    // 丢到线程池，完成后回主线程更新成员并发信号。
    const QString binary = locateNvidiaSmi();
    if (binary.isEmpty()) {
        qWarning() << "未找到 nvidia-smi，GPU 功能降级隐藏";
        m_available = false;
        Q_EMIT availabilityChanged(false);
        return;
    }
    QPointer<NvidiaGpu> guard(this);
    QThreadPool::globalInstance()->start([guard, binary]() {
        bool ok = false;
        QString name;
        QProcess proc;
        proc.start(binary, {QStringLiteral("--query-gpu=name"), QStringLiteral("--format=csv,noheader")});
        if (proc.waitForFinished(2000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
            const QStringList lines = QString::fromUtf8(proc.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            name = lines.isEmpty() ? QStringLiteral("GPU") : lines.first().trimmed();
            ok = true;
        }
        QMetaObject::invokeMethod(guard.data(), [guard, binary, name, ok]() {
            if (!guard)
                return;
            guard->m_binary = binary;
            guard->m_name = name;
            guard->m_available = ok;
            if (ok)
                qInfo() << "NVIDIA GPU 可用:" << guard->m_name;
            else
                qWarning() << "nvidia-smi 探测失败，GPU 功能降级";
            Q_EMIT guard->availabilityChanged(guard->m_available);
        }, Qt::QueuedConnection);
    });
}

// ---------------- 异步查询 ----------------

void NvidiaGpu::queryAsync(const QStringList &fields, int timeoutMs)
{
    if (!m_available) {
        // 不可用：仍按约定发一次空结果，便于上层统一处理
        Q_EMIT queryFinished(fields, QStringList());
        return;
    }
    QPointer<NvidiaGpu> guard(this);
    const QString binary = m_binary;
    QThreadPool::globalInstance()->start([guard, fields, binary, timeoutMs]() {
        QStringList result;
        QProcess proc;
        proc.start(binary, {QStringLiteral("--query-gpu=") + fields.join(QLatin1Char(',')),
                            QStringLiteral("--format=csv,noheader,nounits")});
        if (proc.waitForFinished(timeoutMs) && proc.exitCode() == 0)
            result = parseQuery(proc.readAllStandardOutput(), fields);
        QMetaObject::invokeMethod(guard.data(), [guard, fields, result]() {
            if (guard)
                Q_EMIT guard->queryFinished(fields, result);
        }, Qt::QueuedConnection);
    });
}

void NvidiaGpu::queryProcessesAsync(int timeoutMs)
{
    if (!m_available) {
        Q_EMIT processesFinished(QVector<GpuProcess>());
        return;
    }
    QPointer<NvidiaGpu> guard(this);
    const QString binary = m_binary;
    QThreadPool::globalInstance()->start([guard, binary, timeoutMs]() {
        QVector<GpuProcess> result;
        QProcess proc;
        proc.start(binary, {QStringLiteral("--query-compute-apps=pid,process_name,used_memory"),
                            QStringLiteral("--format=csv,noheader,nounits")});
        if (proc.waitForFinished(timeoutMs) && proc.exitCode() == 0)
            result = parseProcesses(proc.readAllStandardOutput());
        QMetaObject::invokeMethod(guard.data(), [guard, result]() {
            if (guard)
                Q_EMIT guard->processesFinished(result);
        }, Qt::QueuedConnection);
    });
}

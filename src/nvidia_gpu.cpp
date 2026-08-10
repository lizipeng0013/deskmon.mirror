// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#include "nvidia_gpu.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QFile>
#include <QDebug>

NvidiaGpu::NvidiaGpu(QObject *parent)
    : QObject(parent)
{
    probe();
}

void NvidiaGpu::probe()
{
    // 在 PATH 中查找 nvidia-smi；找不到则降级
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString pathEnv = env.value(QStringLiteral("PATH"));
    for (const QString &dir : pathEnv.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        if (QFile::exists(dir + QStringLiteral("/nvidia-smi"))) {
            m_binary = dir + QStringLiteral("/nvidia-smi");
            break;
        }
    }
    if (m_binary.isEmpty()) {
        qWarning() << "未找到 nvidia-smi，GPU 功能降级隐藏";
        return;
    }

    QProcess proc;
    proc.start(m_binary, {QStringLiteral("--query-gpu=name"), QStringLiteral("--format=csv,noheader")});
    if (!proc.waitForFinished(2000)) {
        qWarning() << "nvidia-smi 执行超时/失败，GPU 功能降级";
        return;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        qWarning() << "nvidia-smi 探测失败，GPU 功能降级";
        return;
    }

    m_available = true;
    const QStringList lines = QString::fromUtf8(proc.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    m_name = lines.isEmpty() ? QStringLiteral("GPU") : lines.first().trimmed();
    qInfo() << "NVIDIA GPU 可用:" << m_name;
}

bool NvidiaGpu::available() const { return m_available; }
QString NvidiaGpu::name() const { return m_name; }

QStringList NvidiaGpu::query(const QStringList &fields, int timeoutMs) const
{
    QStringList out;
    if (!m_available)
        return out;

    QStringList args = {QStringLiteral("--query-gpu=") + fields.join(QLatin1Char(',')),
                        QStringLiteral("--format=csv,noheader,nounits")};
    QProcess proc;
    proc.start(m_binary, args);
    if (!proc.waitForFinished(timeoutMs) || proc.exitCode() != 0)
        return out;

    const QStringList parts = QString::fromUtf8(proc.readAllStandardOutput()).trimmed().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (int i = 0; i < fields.size(); ++i)
        out << (i < parts.size() ? parts[i].trimmed() : QString());
    return out;
}

QVector<NvidiaGpu::GpuProcess> NvidiaGpu::processes() const
{
    QVector<GpuProcess> result;
    if (!m_available)
        return result;

    QProcess proc;
    proc.start(m_binary, {QStringLiteral("--query-compute-apps=pid,process_name,used_memory"),
                          QStringLiteral("--format=csv,noheader,nounits")});
    if (!proc.waitForFinished(5000) || proc.exitCode() != 0)
        return result;

    const QStringList lines = QString::fromUtf8(proc.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList parts = line.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (parts.size() < 3)
            continue;
        bool ok = false;
        const int pid = parts[0].trimmed().toInt(&ok);
        if (!ok)
            continue;
        GpuProcess p;
        p.pid = pid;
        p.name = parts[1].trimmed();
        p.memoryMB = parts[2].trimmed().toLongLong();
        result.append(p);
    }
    return result;
}

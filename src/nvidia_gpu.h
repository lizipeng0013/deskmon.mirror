// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DESKMON_NVIDIA_GPU_H
#define DESKMON_NVIDIA_GPU_H

#include <QObject>
#include <QStringList>
#include <QVector>

/**
 * @brief NVIDIA GPU 信息查询，封装 nvidia-smi 调用（全异步，不阻塞 UI 线程）
 *
 * 启动时 probeAsync() 探测 nvidia-smi 是否可用；探测完成后发
 * availabilityChanged()。查询通过 queryAsync()/queryProcessesAsync() 触发，
 * 结果由 queryFinished()/processesFinished() 信号异步回传，避免主线程上
 * waitForFinished() 造成的卡顿（见原版同步实现的响应性问题）。
 */
class NvidiaGpu : public QObject
{
    Q_OBJECT
public:
    struct GpuProcess {
        int pid = 0;
        QString name;
        qint64 memoryMB = 0;
    };

    explicit NvidiaGpu(QObject *parent = nullptr);

    bool available() const;
    QString name() const;

    // 异步探测 nvidia-smi 是否可用；完成后发 availabilityChanged
    void probeAsync();

    // 异步查询：结果通过 queryFinished 信号回传（不可用时回传空列表）
    void queryAsync(const QStringList &fields, int timeoutMs = 2000);
    void queryProcessesAsync(int timeoutMs = 5000);

signals:
    void availabilityChanged(bool available);
    void queryFinished(const QStringList &fields, const QStringList &values);
    void processesFinished(const QVector<GpuProcess> &procs);

private:
    bool m_available = false;
    QString m_name;
    QString m_binary;   // nvidia-smi 可执行路径
};

#endif // DESKMON_NVIDIA_GPU_H

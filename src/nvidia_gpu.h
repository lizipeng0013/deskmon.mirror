// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DESKMON_NVIDIA_GPU_H
#define DESKMON_NVIDIA_GPU_H

#include <QObject>
#include <QStringList>
#include <QVector>

/**
 * @brief NVIDIA GPU 信息查询，封装 nvidia-smi 调用
 *
 * 启动时探测 nvidia-smi 是否可用；不可用时 available() 返回 false，
 * 上层据此隐藏 GPU 面板（降级策略见开发方案 §4.1）。
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

    // 查询指定字段，返回与 fields 等长的值列表（失败/缺失为空串）
    QStringList query(const QStringList &fields, int timeoutMs = 2000) const;

    // 占用显存的进程列表
    QVector<GpuProcess> processes() const;

signals:
    void availabilityChanged(bool available);

private:
    void probe();

    bool m_available = false;
    QString m_name;
    QString m_binary;   // nvidia-smi 可执行路径
};

#endif // DESKMON_NVIDIA_GPU_H

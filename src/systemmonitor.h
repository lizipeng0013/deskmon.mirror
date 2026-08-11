// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DESKMON_SYSTEMMONITOR_H
#define DESKMON_SYSTEMMONITOR_H

#include "nvidia_gpu.h"

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QVector>

/**
 * @brief 系统资源监控数据层（CPU/内存/磁盘/网络/GPU）
 *
 * 数据来源：/proc、sysfs、QStorageInfo、QNetworkInterface，无需 psutil。
 * GPU 通过 nvidia-smi 异步查询（见 NvidiaGpu），结果缓存到 m_gpuStatsCache，
 * UI 层读缓存即可，不阻塞主线程。
 */
class SystemMonitor : public QObject
{
    Q_OBJECT
public:
    explicit SystemMonitor(QObject *parent = nullptr);

    bool gpuAvailable() const;
    QString gpuName() const;

    // 单项查询接口（对应原版 psutil 逻辑）
    double cpuUsage();
    double cpuTemp() const;                 // 摄氏度，无则返回 -1
    void memoryUsage(double &percent, qint64 &usedMB, qint64 &totalMB) const;
    void diskUsage(double &percent, qint64 &usedGB, qint64 &totalGB) const;
    QPair<double, double> networkSpeed();   // (上传, 下载) 字节/秒
    QStringList ipAddresses() const;

    // GPU：直接读缓存（查询在后台异步进行，不阻塞 UI）
    struct GpuStats {
        double util = -1;      // %
        qint64 memUsedMB = 0;
        qint64 memTotalMB = 0;
        double temp = -1;      // ℃
        double encoderUtil = 0;
        double decoderUtil = 0;
    };
    GpuStats gpuStats() const;
    QVector<NvidiaGpu::GpuProcess> gpuProcesses() const;

signals:
    void gpuAvailabilityChanged(bool available);
    // 后台 GPU 查询完成时发出，UI 可据此刷新（也可直接读缓存）
    void gpuStatsReady(const GpuStats &stats);
    void gpuProcessesReady(const QVector<NvidiaGpu::GpuProcess> &procs);

private slots:
    void onGpuAvailabilityChanged(bool available);
    void onGpuQueryFinished(const QStringList &fields, const QStringList &values);
    void onGpuProcessesFinished(const QVector<NvidiaGpu::GpuProcess> &procs);
    void refreshGpu();

private:
    NvidiaGpu *m_gpu = nullptr;
    QTimer m_gpuTimer;                  // GPU 后台刷新定时器（与 UI refresh 解耦）

    // GPU 缓存（由后台异步查询更新）
    GpuStats m_gpuStatsCache;
    QVector<NvidiaGpu::GpuProcess> m_gpuProcsCache;

    // CPU 计算状态（从函数静态挪到成员，避免多实例互相污染）
    struct CpuTimes {
        qulonglong user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0;
    };
    CpuTimes m_prevCpu;
    bool m_havePrevCpu = false;
    bool readCpuTimes(CpuTimes &t) const;

    // 网络计数状态
    struct NetCounters {
        quint64 bytesRecv = 0, bytesSent = 0;
    };
    NetCounters m_lastNet;
    QElapsedTimer m_netTimer;

    // GPU 滑动平均窗口
    static constexpr int kSmoothWindow = 5;
    QVector<double> m_gpuUtilHistory, m_encoderHistory, m_decoderHistory;
    double smoothValue(double value, QVector<double> &history);
};

#endif // DESKMON_SYSTEMMONITOR_H

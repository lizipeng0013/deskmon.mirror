// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKMON_SYSTEMMONITOR_H
#define DESKMON_SYSTEMMONITOR_H

#include "nvidia_gpu.h"

#include <QObject>
#include <QElapsedTimer>
#include <QVector>

/**
 * @brief 系统资源监控数据层（CPU/内存/磁盘/网络/GPU）
 *
 * 数据来源：/proc、sysfs、QStorageInfo、QNetworkInterface，无需 psutil。
 * GPU 通过 nvidia-smi 查询（见 NvidiaGpu）。
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

    // GPU（带滑动平均平滑）
    struct GpuStats {
        double util = -1;      // %
        qint64 memUsedMB = 0;
        qint64 memTotalMB = 0;
        double temp = -1;      // ℃
        double encoderUtil = 0;
        double decoderUtil = 0;
    };
    GpuStats gpuStats();

    // 占用 GPU 显存的进程列表
    QVector<NvidiaGpu::GpuProcess> gpuProcesses();

signals:
    void gpuAvailabilityChanged(bool available);

private:
    NvidiaGpu *m_gpu = nullptr;

    // CPU 计算状态
    struct CpuTimes {
        qulonglong user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0;
    };
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

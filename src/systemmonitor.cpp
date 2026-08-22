// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "systemmonitor.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDir>
#include <QStorageInfo>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QAbstractSocket>
#include <QRegularExpression>

#include <sys/sysinfo.h>

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
    m_gpu = new NvidiaGpu(this);
    connect(m_gpu, &NvidiaGpu::availabilityChanged,
            this, &SystemMonitor::gpuAvailabilityChanged);
    connect(m_gpu, &NvidiaGpu::availabilityChanged,
            this, &SystemMonitor::onGpuAvailabilityChanged);
    connect(m_gpu, &NvidiaGpu::queryFinished,
            this, &SystemMonitor::onGpuQueryFinished);
    connect(m_gpu, &NvidiaGpu::processesFinished,
            this, &SystemMonitor::onGpuProcessesFinished);

    // GPU 后台刷新定时器：与 UI refresh 解耦，间隔 2s，避免每秒同步调用 nvidia-smi
    m_gpuTimer.setInterval(2000);
    connect(&m_gpuTimer, &QTimer::timeout, this, &SystemMonitor::refreshGpu);

    // 异步探测 nvidia-smi（完成后 onGpuAvailabilityChanged 决定是否启动定时器）
    m_gpu->probeAsync();
}

bool SystemMonitor::gpuAvailable() const { return m_gpu->available(); }
QString SystemMonitor::gpuName() const { return m_gpu->name(); }

// ---------------- CPU ----------------

bool SystemMonitor::readCpuTimes(CpuTimes &t) const
{
    QFile f(QStringLiteral("/proc/stat"));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QString line = f.readLine().trimmed();
    if (!line.startsWith(QLatin1String("cpu")))
        return false;

    const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    // parts: cpu user nice system idle iowait irq softirq steal ...
    if (parts.size() < 8)
        return false;
    t.user = parts[1].toULongLong();
    t.nice = parts[2].toULongLong();
    t.system = parts[3].toULongLong();
    t.idle = parts[4].toULongLong();
    t.iowait = parts[5].toULongLong();
    t.irq = parts[6].toULongLong();
    t.softirq = parts[7].toULongLong();
    // steal：虚拟机里被宿主占用的时间；老内核无此列，取 0
    t.steal = parts.size() > 8 ? parts[8].toULongLong() : 0;
    return true;
}

double SystemMonitor::cpuUsage()
{
    CpuTimes cur;
    if (!readCpuTimes(cur))
        return -1;

    if (!m_havePrevCpu) {
        m_prevCpu = cur;
        m_havePrevCpu = true;
        // 首次采样：/proc/stat 是开机以来的累计值，直接算开机平均，
        // 比显示 0% 更接近真实
        const qulonglong idleAll = cur.idle + cur.iowait;
        const qulonglong totalAll = cur.user + cur.nice + cur.system + cur.idle
            + cur.iowait + cur.irq + cur.softirq + cur.steal;
        if (totalAll == 0)
            return 0;
        return qBound(0.0, 100.0 * (1.0 - double(idleAll) / double(totalAll)), 100.0);
    }

    const qulonglong idleDelta = (cur.idle - m_prevCpu.idle) + (cur.iowait - m_prevCpu.iowait);
    const qulonglong totalDelta =
        (cur.user - m_prevCpu.user) + (cur.nice - m_prevCpu.nice) + (cur.system - m_prevCpu.system)
        + (cur.idle - m_prevCpu.idle) + (cur.iowait - m_prevCpu.iowait)
        + (cur.irq - m_prevCpu.irq) + (cur.softirq - m_prevCpu.softirq)
        + (cur.steal - m_prevCpu.steal);
    m_prevCpu = cur;

    if (totalDelta == 0)
        return 0;
    return qBound(0.0, 100.0 * (1.0 - double(idleDelta) / double(totalDelta)), 100.0);
}

double SystemMonitor::cpuTemp() const
{
    // sysfs 热区：取标着 core/package 或第一个非 0 读数
    double best = -1;
    QDir thermalDir(QStringLiteral("/sys/class/thermal"));
    const QStringList zones = thermalDir.entryList({QStringLiteral("thermal_zone*")}, QDir::Dirs);
    for (const QString &zone : zones) {
        QFile typeFile(thermalDir.filePath(zone + QStringLiteral("/type")));
        QString type;
        if (typeFile.open(QIODevice::ReadOnly))
            type = QString::fromUtf8(typeFile.readLine()).trimmed().toLower();

        QFile tempFile(thermalDir.filePath(zone + QStringLiteral("/temp")));
        if (!tempFile.open(QIODevice::ReadOnly))
            continue;
        const double milli = tempFile.readAll().trimmed().toDouble();
        if (milli <= 0)
            continue;
        const double celsius = milli / 1000.0;

        // 优先 core/package/cpu 相关热区
        if (type.contains(QLatin1String("core")) || type.contains(QLatin1String("package"))
            || type.contains(QLatin1String("cpu")))
            return celsius;
        if (best < 0)
            best = celsius;
    }
    return best;
}

// ---------------- 内存 / 磁盘 ----------------

void SystemMonitor::memoryUsage(double &percent, qint64 &usedMB, qint64 &totalMB) const
{
    // 已用 = MemTotal - MemAvailable，与深度系统监视器 / free 同口径。
    // 不能用 sysinfo 的 freeram+bufferram+sharedram：sysinfo 没有页缓存字段，
    // 会把数 GB 的 Cached 算进已用，且开机越久偏差越大。
    qint64 total = -1, available = -1;
    QFile f(QStringLiteral("/proc/meminfo"));
    if (f.open(QIODevice::ReadOnly)) {
        const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            const qint64 kb = line.section(QLatin1Char(':'), 1)
                                  .split(QLatin1Char(' '), Qt::SkipEmptyParts)
                                  .value(0).toLongLong() * 1024;
            if (line.startsWith(QLatin1String("MemTotal:")))
                total = kb;
            else if (line.startsWith(QLatin1String("MemAvailable:")))
                available = kb;
        }
    }
    if (total <= 0 || available < 0) {
        // 回退：无 MemAvailable 的老内核（<3.14）用 free+buffers 近似；
        // shmem 是真占用不可直接回收，不计入可用
        struct sysinfo si {};
        if (sysinfo(&si) != 0) {
            percent = -1;
            usedMB = totalMB = 0;
            return;
        }
        total = qint64(si.totalram) * si.mem_unit;
        available = (qint64(si.freeram) + qint64(si.bufferram)) * si.mem_unit;
    }
    const qint64 used = total - available;
    totalMB = total / (1024 * 1024);
    usedMB = used / (1024 * 1024);
    percent = total > 0 ? 100.0 * double(used) / double(total) : 0;
}

void SystemMonitor::diskUsage(double &percent, qint64 &usedGB, qint64 &totalGB) const
{
    const QStorageInfo root(QStringLiteral("/"));
    if (!root.isValid()) {
        percent = -1;
        usedGB = totalGB = 0;
        return;
    }
    totalGB = root.bytesTotal() / (1024 * 1024 * 1024);
    usedGB = (root.bytesTotal() - root.bytesFree()) / (1024 * 1024 * 1024);
    percent = root.bytesTotal() > 0 ? 100.0 * double(root.bytesTotal() - root.bytesFree()) / double(root.bytesTotal()) : 0;
}

// ---------------- 网络 ----------------

QPair<double, double> SystemMonitor::networkSpeed()
{
    // /proc/net/dev 汇总所有网卡（跳过 lo）
    QFile f(QStringLiteral("/proc/net/dev"));
    quint64 recv = 0, sent = 0;
    if (f.open(QIODevice::ReadOnly)) {
        const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.contains(QLatin1String("Inter-|")))
                continue;
            const int colon = trimmed.indexOf(QLatin1Char(':'));
            if (colon <= 0)
                continue;
            const QString iface = trimmed.left(colon);
            if (iface == QLatin1String("lo"))
                continue;
            const QStringList nums = trimmed.mid(colon + 1).split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (nums.size() >= 9) {
                recv += nums[0].toULongLong();   // 收包字节
                sent += nums[8].toULongLong();   // 发包字节
            }
        }
    }

    const bool first = !m_netTimer.isValid();
    if (first) {
        m_netTimer.start();
        m_lastNet = {recv, sent};
        return {0, 0};
    }
    const qint64 elapsedMs = m_netTimer.restart();
    const double up = double(sent - m_lastNet.bytesSent) * 1000.0 / qMax<qint64>(elapsedMs, 1);
    const double down = double(recv - m_lastNet.bytesRecv) * 1000.0 / qMax<qint64>(elapsedMs, 1);
    m_lastNet = {recv, sent};
    return {qMax(0.0, up), qMax(0.0, down)};
}

namespace {
// 常见虚拟/容器接口：默认路由缺失时跳过，避免 docker0/virbr 等占位 IP
bool isVirtualIface(const QString &name)
{
    return name.startsWith(QLatin1String("docker")) || name.startsWith(QLatin1String("br-"))
        || name.startsWith(QLatin1String("virbr")) || name.startsWith(QLatin1String("veth"))
        || name.startsWith(QLatin1String("tun")) || name.startsWith(QLatin1String("tap"))
        || name.startsWith(QLatin1String("vpn"));
}
} // namespace

QStringList SystemMonitor::ipAddresses() const
{
    // 优先取默认路由所在接口（即真正上网的本机 IP）
    QString defaultIface;
    QFile routeFile(QStringLiteral("/proc/net/route"));
    if (routeFile.open(QIODevice::ReadOnly)) {
        const QStringList lines = QString::fromUtf8(routeFile.readAll()).split(QLatin1Char('\n'));
        for (int i = 1; i < lines.size(); ++i) {   // 跳过表头
            const QStringList parts = lines.at(i).trimmed().split(QLatin1Char('\t'));
            if (parts.size() >= 3 && parts.at(1) == QLatin1String("00000000")) {
                defaultIface = parts.at(0);
                break;
            }
        }
    }

    QStringList ips;
    const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || iface.flags() & QNetworkInterface::IsLoopBack)
            continue;
        // 有默认路由：只取该接口；无默认路由：跳过虚拟接口兜底
        if (!defaultIface.isEmpty() && iface.name() != defaultIface)
            continue;
        if (defaultIface.isEmpty() && isVirtualIface(iface.name()))
            continue;

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress addr = entry.ip();
            if (addr.protocol() == QAbstractSocket::IPv4Protocol)
                ips << addr.toString();
        }
        if (!defaultIface.isEmpty() && !ips.isEmpty())
            break;
    }
    return ips;
}

// ---------------- GPU（异步查询 + 缓存） ----------------

void SystemMonitor::onGpuAvailabilityChanged(bool available)
{
    if (available) {
        if (!m_gpuTimer.isActive())
            m_gpuTimer.start();
        refreshGpu();   // 立即查一次，缩短首屏延迟
    } else {
        m_gpuTimer.stop();
        m_gpuStatsCache = GpuStats();
        m_gpuProcsCache.clear();
        m_gpuUtilHistory.clear();
        m_encoderHistory.clear();
        m_decoderHistory.clear();
    }
}

void SystemMonitor::refreshGpu()
{
    if (!m_gpu->available())
        return;
    static const QStringList fields = {QStringLiteral("utilization.gpu"),
                                        QStringLiteral("memory.used"),
                                        QStringLiteral("memory.total"),
                                        QStringLiteral("temperature.gpu"),
                                        QStringLiteral("utilization.encoder"),
                                        QStringLiteral("utilization.decoder")};
    m_gpu->queryAsync(fields);
    m_gpu->queryProcessesAsync();
}

void SystemMonitor::onGpuQueryFinished(const QStringList &fields, const QStringList &values)
{
    Q_UNUSED(fields)
    if (values.size() < 4)
        return;
    GpuStats s;
    const double rawUtil = values[0].toDouble();
    s.memUsedMB = qint64(values[1].toDouble());
    s.memTotalMB = qint64(values[2].toDouble());
    s.temp = values[3].toDouble();

    const double encoder = values.size() > 4 ? values[4].toDouble() : 0;
    const double decoder = values.size() > 5 ? values[5].toDouble() : 0;

    s.util = smoothValue(rawUtil, m_gpuUtilHistory);
    s.encoderUtil = smoothValue(encoder, m_encoderHistory);
    s.decoderUtil = smoothValue(decoder, m_decoderHistory);
    m_gpuStatsCache = s;
    Q_EMIT gpuStatsReady(s);
}

void SystemMonitor::onGpuProcessesFinished(const QVector<NvidiaGpu::GpuProcess> &procs)
{
    m_gpuProcsCache = procs;
    Q_EMIT gpuProcessesReady(procs);
}

double SystemMonitor::smoothValue(double value, QVector<double> &history)
{
    if (value < 0)
        return value;
    history.append(value);
    while (history.size() > kSmoothWindow)
        history.removeFirst();
    double sum = 0;
    for (double v : history)
        sum += v;
    return sum / history.size();
}

SystemMonitor::GpuStats SystemMonitor::gpuStats() const { return m_gpuStatsCache; }
QVector<NvidiaGpu::GpuProcess> SystemMonitor::gpuProcesses() const { return m_gpuProcsCache; }

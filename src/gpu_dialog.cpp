// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gpu_dialog.h"

#include "systemmonitor.h"
#include "processmgr.h"
#include "nvidia_gpu.h"

#include <DPushButton>

#include <QTimer>
#include <QLabel>
#include <QIcon>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>

DWIDGET_USE_NAMESPACE

GpuDialog::GpuDialog(SystemMonitor *monitor, QWidget *parent)
    : DDialog(parent)
    , m_monitor(monitor)
{
    setWindowTitle(tr("GPU 显存管理"));
    setIcon(QIcon::fromTheme(QStringLiteral("deskmon")));
    buildUi();

    addButton(tr("关闭"), false, DDialog::ButtonNormal);

    m_timer = new QTimer(this);
    m_timer->setInterval(3000);
    connect(m_timer, &QTimer::timeout, this, &GpuDialog::refreshProcesses);
    m_timer->start();

    refreshProcesses();
}

void GpuDialog::buildUi()
{
    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // GPU 信息
    m_gpuInfo = new QLabel(content);
    QFont gpuFont = m_gpuInfo->font();
    gpuFont.setBold(true);
    m_gpuInfo->setFont(gpuFont);
    layout->addWidget(m_gpuInfo);

    // 操作行
    auto *header = new QHBoxLayout;
    auto *refreshBtn = new DPushButton(tr("🔄 刷新"), content);
    refreshBtn->setFixedWidth(80);
    connect(refreshBtn, &DPushButton::clicked, this, &GpuDialog::refreshProcesses);
    header->addWidget(refreshBtn);

    m_releaseAllBtn = new DPushButton(tr("💥 释放全部显存"), content);
    m_releaseAllBtn->setFixedWidth(130);
    connect(m_releaseAllBtn, &DPushButton::clicked, this, &GpuDialog::releaseAll);
    header->addWidget(m_releaseAllBtn);
    header->addStretch();
    layout->addLayout(header);

    // 进程表
    m_table = new DTableWidget(content);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("PID"), tr("进程名"), tr("显存占用"), tr("操作")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 80);
    m_table->setColumnWidth(2, 110);
    m_table->setColumnWidth(3, 60);
    layout->addWidget(m_table, 1);

    // 总量 + 提示
    m_totalLabel = new QLabel(content);
    layout->addWidget(m_totalLabel);
    auto *tip = new QLabel(tr("提示：结束进程将释放其占用的显存，请谨慎操作"), content);
    QFont tipFont = tip->font();
    tipFont.setPointSizeF(tipFont.pointSizeF() - 2);
    tip->setFont(tipFont);
    layout->addWidget(tip);

    addContent(content);
    setMinimumSize(560, 360);
}

void GpuDialog::refreshProcesses()
{
    m_gpuInfo->setText(tr("GPU: %1").arg(m_monitor->gpuName()));

    const QVector<NvidiaGpu::GpuProcess> procs = m_monitor->gpuProcesses();
    m_pids.clear();
    qint64 total = 0;

    m_table->setRowCount(procs.size());
    for (int row = 0; row < procs.size(); ++row) {
        const auto &p = procs.at(row);
        m_pids.append({p.pid, p.name});
        total += p.memoryMB;

        auto *pidItem = new QTableWidgetItem(QString::number(p.pid));
        pidItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, pidItem);

        m_table->setItem(row, 1, new QTableWidgetItem(p.name));

        auto *memItem = new QTableWidgetItem(tr("%1 MB").arg(p.memoryMB));
        memItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 2, memItem);

        auto *releaseBtn = new DPushButton(tr("释放"), m_table);
        releaseBtn->setFixedSize(48, 24);
        connect(releaseBtn, &DPushButton::clicked, this,
                [this, p] { killProcess(p.pid, p.name); });
        m_table->setCellWidget(row, 3, releaseBtn);
    }

    m_totalLabel->setText(tr("总显存占用: %1 MB（%2 个进程）").arg(total).arg(procs.size()));
    m_releaseAllBtn->setEnabled(!procs.isEmpty());
}

void GpuDialog::killProcess(int pid, const QString &name)
{
    const QString msg = tr("确定要终止进程 \"%1\" (PID: %2) 吗？\n这将释放其占用的显存，可能导致未保存的数据丢失。")
                            .arg(name).arg(pid);
    auto *dlg = new DDialog(tr("确认释放显存"), msg, this);
    dlg->setIcon(QIcon::fromTheme(QStringLiteral("dialog-warning")));
    const int killBtn = dlg->addButton(tr("终止"), false, DDialog::ButtonWarning);
    dlg->addButton(tr("取消"), true, DDialog::ButtonNormal);
    connect(dlg, &DDialog::buttonClicked, this, [this, dlg, killBtn, pid](int index, const QString &) {
        if (index == killBtn && ProcessMgr::kill(pid))
            refreshProcesses();
    });
    dlg->exec();
    dlg->deleteLater();
}

void GpuDialog::releaseAll()
{
    if (m_pids.isEmpty())
        return;
    QStringList names;
    for (const auto &p : m_pids) {
        if (names.size() < 10)
            names << QStringLiteral("  • %1 (PID: %2)").arg(p.second).arg(p.first);
    }
    const QString msg = tr("确定要终止以下 %1 个占用显存的进程吗？\n\n%2\n\n警告：可能导致未保存的数据丢失！")
                            .arg(m_pids.size()).arg(names.join(QLatin1Char('\n')));
    auto *dlg = new DDialog(tr("确认释放全部显存"), msg, this);
    dlg->setIcon(QIcon::fromTheme(QStringLiteral("dialog-warning")));
    const int killBtn = dlg->addButton(tr("全部终止"), false, DDialog::ButtonWarning);
    dlg->addButton(tr("取消"), true, DDialog::ButtonNormal);
    connect(dlg, &DDialog::buttonClicked, this, [this, dlg, killBtn](int index, const QString &) {
        if (index != killBtn)
            return;
        int killed = 0;
        for (const auto &p : m_pids) {
            if (ProcessMgr::kill(p.first))
                ++killed;
        }
        refreshProcesses();
    });
    dlg->exec();
    dlg->deleteLater();
}

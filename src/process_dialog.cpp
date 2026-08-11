// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "process_dialog.h"

#include "processmgr.h"

#include <DComboBox>
#include <DPushButton>

#include <QTimer>
#include <QLabel>
#include <QIcon>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>

#include <algorithm>

DWIDGET_USE_NAMESPACE

ProcessDialog::ProcessDialog(QWidget *parent)
    : DDialog(parent)
    , m_mgr(new ProcessMgr(this))
{
    setWindowTitle(tr("进程管理"));
    setIcon(QIcon::fromTheme(QStringLiteral("deskmon")));
    buildUi();

    addButton(tr("关闭"), false, DDialog::ButtonNormal);

    // 2s 自动刷新
    m_timer = new QTimer(this);
    m_timer->setInterval(2000);
    connect(m_timer, &QTimer::timeout, this, &ProcessDialog::refreshProcesses);
    m_timer->start();

    refreshProcesses();
}

void ProcessDialog::buildUi()
{
    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // 头部：刷新 + 排序 + 计数
    auto *header = new QHBoxLayout;
    auto *refreshBtn = new DPushButton(tr("🔄 刷新"), content);
    refreshBtn->setFixedWidth(80);
    connect(refreshBtn, &DPushButton::clicked, this, &ProcessDialog::refreshProcesses);
    header->addWidget(refreshBtn);

    header->addStretch();

    header->addWidget(new QLabel(tr("排序:"), content));
    m_sortCombo = new DComboBox(content);
    m_sortCombo->addItems({tr("CPU 使用率"), tr("内存使用"), tr("进程名"), tr("PID")});
    connect(m_sortCombo, qOverload<int>(&DComboBox::currentIndexChanged),
            this, &ProcessDialog::refreshProcesses);
    header->addWidget(m_sortCombo);

    m_countLabel = new QLabel(content);
    header->addWidget(m_countLabel);
    layout->addLayout(header);

    // 进程表
    m_table = new DTableWidget(content);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("PID"), tr("进程名"), tr("CPU %"), tr("内存 %"), tr("操作")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 80);
    m_table->setColumnWidth(2, 70);
    m_table->setColumnWidth(3, 70);
    m_table->setColumnWidth(4, 60);
    layout->addWidget(m_table, 1);

    addContent(content);
    setMinimumSize(640, 420);
}

void ProcessDialog::refreshProcesses()
{
    m_processes = m_mgr->list();

    // 排序
    switch (m_sortCombo->currentIndex()) {
    case 0: // CPU
        std::sort(m_processes.begin(), m_processes.end(),
                  [](const ProcessInfo &a, const ProcessInfo &b) { return a.cpuPercent > b.cpuPercent; });
        break;
    case 1: // 内存
        std::sort(m_processes.begin(), m_processes.end(),
                  [](const ProcessInfo &a, const ProcessInfo &b) { return a.memPercent > b.memPercent; });
        break;
    case 2: // 进程名
        std::sort(m_processes.begin(), m_processes.end(),
                  [](const ProcessInfo &a, const ProcessInfo &b) { return a.name < b.name; });
        break;
    default: // PID
        std::sort(m_processes.begin(), m_processes.end(),
                  [](const ProcessInfo &a, const ProcessInfo &b) { return a.pid < b.pid; });
        break;
    }

    m_table->setRowCount(m_processes.size());
    const QColor warn(QStringLiteral("#ff4444")), mid(QStringLiteral("#ffaa00"));
    for (int row = 0; row < m_processes.size(); ++row) {
        const ProcessInfo &p = m_processes.at(row);

        auto *pidItem = new QTableWidgetItem(QString::number(p.pid));
        pidItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, pidItem);

        auto *nameItem = new QTableWidgetItem(p.name);
        m_table->setItem(row, 1, nameItem);

        auto *cpuItem = new QTableWidgetItem(QStringLiteral("%1").arg(p.cpuPercent, 0, 'f', 1));
        cpuItem->setTextAlignment(Qt::AlignCenter);
        if (p.cpuPercent > 50) cpuItem->setForeground(warn);
        else if (p.cpuPercent > 20) cpuItem->setForeground(mid);
        m_table->setItem(row, 2, cpuItem);

        auto *memItem = new QTableWidgetItem(QStringLiteral("%1").arg(p.memPercent, 0, 'f', 1));
        memItem->setTextAlignment(Qt::AlignCenter);
        if (p.memPercent > 50) memItem->setForeground(warn);
        else if (p.memPercent > 20) memItem->setForeground(mid);
        m_table->setItem(row, 3, memItem);

        auto *killBtn = new DPushButton(tr("结束"), m_table);
        killBtn->setFixedSize(48, 24);
        killBtn->setProperty("__pid", p.pid);
        connect(killBtn, &DPushButton::clicked, this, [this, p] { killProcess(p); });
        m_table->setCellWidget(row, 4, killBtn);
    }
    m_countLabel->setText(tr("共 %1 个进程").arg(m_processes.size()));
}

void ProcessDialog::killProcess(const ProcessInfo &proc)
{
    const QString msg = tr("确定要终止进程 \"%1\" (PID: %2) 吗？\n这可能导致未保存的数据丢失。")
                            .arg(proc.name).arg(proc.pid);
    auto *dlg = new DDialog(tr("确认结束进程"), msg, this);
    dlg->setIcon(QIcon::fromTheme(QStringLiteral("dialog-warning")));
    const int killBtn = dlg->addButton(tr("结束"), false, DDialog::ButtonWarning);
    dlg->addButton(tr("取消"), true, DDialog::ButtonNormal);
    connect(dlg, &DDialog::buttonClicked, this, [this, dlg, killBtn, proc](int index, const QString &) {
        if (index == killBtn) {
            ProcessMgr::kill(proc.pid);
            refreshProcesses();
        }
    });
    dlg->exec();
    dlg->deleteLater();
}

// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKMON_GPU_DIALOG_H
#define DESKMON_GPU_DIALOG_H

#include <DDialog>
#include <DTableWidget>
#include <DPushButton>

#include <QVector>
#include <QPair>
#include <QString>

class SystemMonitor;
class QLabel;
class QTimer;

/**
 * @brief GPU 显存管理对话框
 *
 * 列出占用显存的进程（PID/名称/显存），支持单进程释放与一键释放。
 */
class GpuDialog : public DTK_WIDGET_NAMESPACE::DDialog
{
    Q_OBJECT
public:
    explicit GpuDialog(SystemMonitor *monitor, QWidget *parent = nullptr);

private:
    void buildUi();
    void refreshProcesses();
    void killProcess(int pid, const QString &name);
    void releaseAll();

    SystemMonitor *m_monitor = nullptr;
    QTimer *m_timer = nullptr;
    DTK_WIDGET_NAMESPACE::DTableWidget *m_table = nullptr;
    QLabel *m_gpuInfo = nullptr;
    QLabel *m_totalLabel = nullptr;
    DTK_WIDGET_NAMESPACE::DPushButton *m_releaseAllBtn = nullptr;
    QVector<QPair<int, QString>> m_pids;   // (pid, name) 用于一键释放
};

#endif // DESKMON_GPU_DIALOG_H

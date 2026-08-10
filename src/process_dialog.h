// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKMON_PROCESS_DIALOG_H
#define DESKMON_PROCESS_DIALOG_H

#include <DDialog>
#include <DTableWidget>
#include <DComboBox>
#include <DPushButton>

#include <QVector>

class ProcessMgr;
struct ProcessInfo;
class QLabel;
class QTimer;

/**
 * @brief 进程管理对话框
 *
 * 列表（PID/进程名/CPU%/内存%）+ 排序 + 每行「结束」按钮，2s 自动刷新。
 */
class ProcessDialog : public DTK_WIDGET_NAMESPACE::DDialog
{
    Q_OBJECT
public:
    explicit ProcessDialog(QWidget *parent = nullptr);

private:
    void buildUi();
    void refreshProcesses();
    void killProcess(const ProcessInfo &proc);

    ProcessMgr *m_mgr = nullptr;
    QTimer *m_timer = nullptr;
    DTK_WIDGET_NAMESPACE::DTableWidget *m_table = nullptr;
    DTK_WIDGET_NAMESPACE::DComboBox *m_sortCombo = nullptr;
    QLabel *m_countLabel = nullptr;
    QVector<ProcessInfo> m_processes;
};

#endif // DESKMON_PROCESS_DIALOG_H

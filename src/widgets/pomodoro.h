// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DESKMON_POMODORO_H
#define DESKMON_POMODORO_H

#include <DWidget>
#include <DPushButton>

#include <QLabel>
#include <QElapsedTimer>

class QTimer;
class QPropertyAnimation;

/**
 * @brief 番茄钟：25 分钟专注 / 5 分钟休息，自动轮换 + 完成通知
 *
 * 紧凑一行：🍅 时间 | 开始/暂停 | 重置
 */
class Pomodoro : public DTK_WIDGET_NAMESPACE::DWidget
{
    Q_OBJECT
public:
    explicit Pomodoro(QWidget *parent = nullptr);

    void reset();

private slots:
    void tick();

private:
    void setRemaining(int sec);
    void finishPhase();
    void startPulse();
    void stopPulse();

    enum State { Idle, RunningWork, Paused, RunningBreak };

    State m_state = Idle;
    int m_remaining = 0;          // 当前阶段剩余秒
    static constexpr int kWorkSec = 25 * 60;
    static constexpr int kBreakSec = 5 * 60;

    QTimer *m_timer = nullptr;
    QWidget *m_icon = nullptr;    // 自绘彩色番茄图标
    QLabel *m_time = nullptr;
    DTK_WIDGET_NAMESPACE::DPushButton *m_toggle = nullptr;
    QPropertyAnimation *m_pulseAnim = nullptr;
};

#endif // DESKMON_POMODORO_H

// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pomodoro.h"

#include <DNotifySender>

#include <QTimer>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

namespace {
// 彩色番茄图标：红果体 + 绿叶 + 高光（不依赖系统 emoji 字体）
class TomatoIcon : public QWidget
{
public:
    explicit TomatoIcon(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(18, 18);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 叶与茎
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QStringLiteral("#2e9e5b")));
        QPainterPath leaf;
        leaf.moveTo(9, 4);
        leaf.cubicTo(3, 1, 4, 7, 9, 5);      // 左叶
        p.drawPath(leaf);
        QPainterPath leaf2;
        leaf2.moveTo(9, 4);
        leaf2.cubicTo(14, 1, 14, 7, 9, 5);    // 右叶
        p.drawPath(leaf2);

        // 果体
        p.setBrush(QColor(QStringLiteral("#e04141")));
        p.drawEllipse(2, 5, 14, 12);

        // 高光
        p.setBrush(QColor(255, 255, 255, 70));
        p.drawEllipse(5, 8, 4, 3);
    }
};
} // namespace

Pomodoro::Pomodoro(QWidget *parent)
    : DWidget(parent)
{
    m_icon = new TomatoIcon(this);

    m_time = new QLabel(this);
    QFont timeFont = m_time->font();
    timeFont.setBold(true);
    m_time->setFont(timeFont);

    m_toggle = new DPushButton(tr("开始"), this);
    m_toggle->setFixedSize(52, 24);
    connect(m_toggle, &DPushButton::clicked, this, [this] {
        switch (m_state) {
        case Idle:
            m_state = RunningWork;
            m_remaining = kWorkSec;
            m_toggle->setText(tr("暂停"));
            m_timer->start();
            break;
        case Paused:
            m_state = RunningWork;
            m_toggle->setText(tr("暂停"));
            m_timer->start();
            break;
        case RunningWork:
            m_state = Paused;
            m_toggle->setText(tr("继续"));
            m_timer->stop();
            break;
        case RunningBreak:
            m_state = Paused;
            m_toggle->setText(tr("继续"));
            m_timer->stop();
            break;
        }
        setRemaining(m_remaining);
    });

    auto *resetBtn = new DPushButton(tr("重置"), this);
    resetBtn->setFixedSize(48, 24);
    connect(resetBtn, &DPushButton::clicked, this, &Pomodoro::reset);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(6);
    layout->addWidget(m_icon);
    layout->addWidget(m_time);
    layout->addStretch();
    layout->addWidget(m_toggle);
    layout->addWidget(resetBtn);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &Pomodoro::tick);

    reset();
}

void Pomodoro::setRemaining(int sec)
{
    m_remaining = qMax(0, sec);
    const int mm = m_remaining / 60;
    const int ss = m_remaining % 60;
    m_time->setText(QStringLiteral("%1:%2")
                        .arg(mm, 2, 10, QLatin1Char('0'))
                        .arg(ss, 2, 10, QLatin1Char('0')));
    // 休息阶段标题变绿提示
    const bool breakPhase = (m_state == RunningBreak);
    m_time->setStyleSheet(breakPhase ? QStringLiteral("color: #00a870;")
                                     : QString());
}

void Pomodoro::tick()
{
    if (m_state != RunningWork && m_state != RunningBreak)
        return;
    setRemaining(m_remaining - 1);
    if (m_remaining <= 0)
        finishPhase();
}

void Pomodoro::finishPhase()
{
    m_timer->stop();
    if (m_state == RunningWork) {
        DUtil::DNotifySender(tr("番茄钟"))
            .appName(QStringLiteral("DeskMon"))
            .appIcon(QStringLiteral("deskmon"))
            .appBody(tr("🍅 专注完成！休息 5 分钟"))
            .timeOut(3000)
            .call();
        m_state = RunningBreak;
        m_remaining = kBreakSec;
        m_toggle->setText(tr("暂停"));
        m_timer->start();
    } else {
        DUtil::DNotifySender(tr("番茄钟"))
            .appName(QStringLiteral("DeskMon"))
            .appIcon(QStringLiteral("deskmon"))
            .appBody(tr("☕ 休息结束！开始新一轮专注"))
            .timeOut(3000)
            .call();
        m_state = RunningWork;
        m_remaining = kWorkSec;
        m_toggle->setText(tr("暂停"));
        m_timer->start();
    }
    setRemaining(m_remaining);
}

void Pomodoro::reset()
{
    m_timer->stop();
    m_state = Idle;
    m_remaining = kWorkSec;
    m_toggle->setText(tr("开始"));
    setRemaining(m_remaining);
}

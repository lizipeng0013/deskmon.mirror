// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "pomodoro.h"

#include <DNotifySender>

#include <QTimer>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QEasingCurve>

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

// 彩色番茄图标：红果体 + 绿叶 + 高光（不依赖系统 emoji 字体）
// 运行中通过 pulseScale 属性实现呼吸脉动
class PomodoroTomatoIcon : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal pulseScale READ pulseScale WRITE setPulseScale)
public:
    explicit PomodoroTomatoIcon(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(18, 18);
    }

    qreal pulseScale() const { return m_pulseScale; }
    void setPulseScale(qreal v)
    {
        m_pulseScale = qBound(0.8, v, 1.2);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(width() / 2.0, height() / 2.0);
        p.scale(m_pulseScale, m_pulseScale);
        p.translate(-width() / 2.0, -height() / 2.0);

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

private:
    qreal m_pulseScale = 1.0;
};

Pomodoro::Pomodoro(QWidget *parent)
    : DWidget(parent)
{
    m_icon = new PomodoroTomatoIcon(this);

    m_time = new QLabel(this);
    QFont timeFont = m_time->font();
    timeFont.setBold(true);
    m_time->setFont(timeFont);

    m_toggle = new DPushButton(tr("开始"), this);
    m_toggle->setFixedSize(44, 22);
    m_toggle->setStyleSheet(QStringLiteral("padding: 0px;"));
    connect(m_toggle, &DPushButton::clicked, this, [this] {
        switch (m_state) {
        case Idle:
            m_state = RunningWork;
            m_remaining = kWorkSec;
            m_toggle->setText(tr("暂停"));
            m_timer->start();
            startPulse();
            break;
        case Paused:
            m_state = RunningWork;
            m_toggle->setText(tr("暂停"));
            m_timer->start();
            startPulse();
            break;
        case RunningWork:
            m_state = Paused;
            m_toggle->setText(tr("继续"));
            m_timer->stop();
            stopPulse();
            break;
        case RunningBreak:
            m_state = Paused;
            m_toggle->setText(tr("继续"));
            m_timer->stop();
            stopPulse();
            break;
        }
        setRemaining(m_remaining);
    });

    auto *resetBtn = new DPushButton(tr("重置"), this);
    resetBtn->setFixedSize(40, 22);
    resetBtn->setStyleSheet(QStringLiteral("padding: 0px;"));
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

    m_pulseAnim = new QPropertyAnimation(static_cast<PomodoroTomatoIcon *>(m_icon), "pulseScale", this);
    m_pulseAnim->setDuration(1000);
    m_pulseAnim->setStartValue(1.0);
    m_pulseAnim->setEndValue(1.12);
    m_pulseAnim->setEasingCurve(QEasingCurve::InOutSine);
    m_pulseAnim->setDirection(QAbstractAnimation::Forward);
    connect(m_pulseAnim, &QPropertyAnimation::finished, this, [this] {
        m_pulseAnim->setDirection(m_pulseAnim->direction() == QAbstractAnimation::Forward
                                     ? QAbstractAnimation::Backward
                                     : QAbstractAnimation::Forward);
        m_pulseAnim->start();
    });

    reset();
}

void Pomodoro::startPulse()
{
    if (m_pulseAnim && m_pulseAnim->state() != QAbstractAnimation::Running)
        m_pulseAnim->start();
}

void Pomodoro::stopPulse()
{
    if (m_pulseAnim) {
        m_pulseAnim->stop();
        static_cast<PomodoroTomatoIcon *>(m_icon)->setPulseScale(1.0);
    }
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
        startPulse();
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
        startPulse();
    }
    setRemaining(m_remaining);
}

void Pomodoro::reset()
{
    m_timer->stop();
    stopPulse();
    m_state = Idle;
    m_remaining = kWorkSec;
    m_toggle->setText(tr("开始"));
    setRemaining(m_remaining);
}

#include "pomodoro.moc"

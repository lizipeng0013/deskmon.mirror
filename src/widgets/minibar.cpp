// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "minibar.h"

#include <DPalette>
#include <DPaletteHelper>

#include <QHBoxLayout>
#include <QToolButton>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QFrame>

DWIDGET_USE_NAMESPACE

MiniBar::MiniBar(QWidget *parent)
    : DWidget(parent)
{
    setFixedHeight(28);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 4, 2);
    layout->setSpacing(4);

    QFont pctFont = font();
    pctFont.setPointSizeF(pctFont.pointSizeF() - 1);
    pctFont.setBold(true);

    // 指标单元：● 字母 百分比（无中文名，靠圆点颜色+首字母区分，悬停 tooltip 显示名称+附加信息）
    buildCell(m_cells[0], tr("CPU"), QStringLiteral("C"), this);
    buildCell(m_cells[1], tr("内存"), QStringLiteral("M"), this);
    buildCell(m_cells[2], tr("GPU"), QStringLiteral("G"), this);
    buildCell(m_cells[3], tr("系统盘"), QStringLiteral("D"), this);

    for (const auto &c : m_cells) {
        c.percent->setFont(pctFont);
        // cell 内部布局已由 buildCell 挂在 container 上，这里只把容器加入主布局
        layout->addWidget(c.container);
    }

    // 网络：↑{up} ↓{down}（无标签，紧凑）
    m_upArrow = new QLabel(QStringLiteral("↑"), this);
    m_up = new QLabel(QStringLiteral("—"), this);
    m_downArrow = new QLabel(QStringLiteral("↓"), this);
    m_down = new QLabel(QStringLiteral("—"), this);

    for (auto *l : {m_upArrow, m_up, m_downArrow, m_down})
        l->setFont(pctFont);

    layout->addWidget(m_upArrow);
    layout->addWidget(m_up);
    layout->addSpacing(3);
    layout->addWidget(m_downArrow);
    layout->addWidget(m_down);

    // 右端只留展开按钮（透明度/隐藏走托盘菜单）
    m_expandBtn = new QToolButton(this);
    m_expandBtn->setText(QStringLiteral("⊞"));
    m_expandBtn->setToolTip(tr("展开为完整窗口"));
    m_expandBtn->setCursor(Qt::PointingHandCursor);
    m_expandBtn->setAutoRaise(true);
    connect(m_expandBtn, &QToolButton::clicked, this, &MiniBar::expandRequested);
    layout->addSpacing(2);
    layout->addWidget(m_expandBtn);

    // 初始提示色
    updateLabelColors();
}

void MiniBar::buildCell(Cell &c, const QString &name, const QString &letter, QWidget *parent)
{
    c.name = name;
    c.container = new QWidget(parent);
    auto *row = new QHBoxLayout(c.container);
    row->setSpacing(1);
    row->setContentsMargins(0, 0, 0, 0);

    c.dot = new QLabel(QStringLiteral("●"), c.container);
    c.dot->setFixedWidth(12);
    QFont dotFont = c.dot->font();
    dotFont.setPointSizeF(dotFont.pointSizeF() - 2);
    c.dot->setFont(dotFont);

    c.letter = new QLabel(letter, c.container);
    QFont lf = c.letter->font();
    lf.setPointSizeF(lf.pointSizeF() - 1);
    lf.setBold(true);
    c.letter->setFont(lf);

    c.percent = new QLabel(QStringLiteral("100%"), c.container);
    QFont pf = c.percent->font();
    pf.setPointSizeF(pf.pointSizeF() - 1);
    pf.setBold(true);
    c.percent->setFont(pf);
    // 保证百分比完整显示，不被布局压缩截断（多留 4px 余量）
    const QFontMetrics fm(c.percent->font());
    c.percent->setMinimumWidth(fm.horizontalAdvance(QStringLiteral("100%")) + 4);
    c.percent->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    row->addWidget(c.dot);
    row->addWidget(c.letter);
    row->addWidget(c.percent);

    // tooltip 只挂在容器上，子 label 不设 -> Qt 冒泡，整个 cell 区域悬停都显示
    c.container->setToolTip(name);
}

void MiniBar::setMetric(int idx, int percent)
{
    if (idx < 0 || idx >= 4)
        return;
    const int v = qBound(0, percent, 100);
    m_cells[idx].percent->setText(QStringLiteral("%1%").arg(v));
}

void MiniBar::setMetricInfo(int idx, const QString &info)
{
    if (idx < 0 || idx >= 4)
        return;
    m_cells[idx].info = info;
    // tooltip：名称 + 百分比 + 附加信息（如「CPU 42% · 温度 55℃」）
    const QString tip = m_cells[idx].info.isEmpty()
        ? QStringLiteral("%1 %2").arg(m_cells[idx].name, m_cells[idx].percent->text())
        : QStringLiteral("%1 %2 · %3").arg(m_cells[idx].name, m_cells[idx].percent->text(), m_cells[idx].info);
    m_cells[idx].container->setToolTip(tip);
}

void MiniBar::setMetricColor(int idx, const QColor &color)
{
    if (idx < 0 || idx >= 4)
        return;
    m_cells[idx].color = color;
    const QString css = QStringLiteral("color: %1;").arg(color.name());
    m_cells[idx].dot->setStyleSheet(css);
    m_cells[idx].letter->setStyleSheet(css);
    m_cells[idx].percent->setStyleSheet(css);
}

void MiniBar::setRowsVisible(bool gpu, bool disk)
{
    m_cells[2].container->setVisible(gpu);
    m_cells[3].container->setVisible(disk);
}

QString MiniBar::formatSpeed(double bps) const
{
    if (bps < 1024)
        return QStringLiteral("%1B").arg(bps, 0, 'f', 0);
    if (bps < 1024 * 1024)
        return QStringLiteral("%1K").arg(bps / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1M").arg(bps / (1024.0 * 1024.0), 0, 'f', 2);
}

void MiniBar::setNetSpeed(double uploadBps, double downloadBps)
{
    m_up->setText(formatSpeed(uploadBps));
    m_down->setText(formatSpeed(downloadBps));
}

void MiniBar::setNetColors(const QColor &up, const QColor &down)
{
    m_upArrow->setStyleSheet(QStringLiteral("color: %1;").arg(up.name()));
    m_downArrow->setStyleSheet(QStringLiteral("color: %1;").arg(down.name()));
}

void MiniBar::updateLabelColors()
{
    const DPalette pal = DPaletteHelper::instance()->palette(this);
    const QColor tipsColor = pal.color(DPalette::TextTips);
    const QString tipsCss = QStringLiteral("color: %1;").arg(
        tipsColor.isValid() ? tipsColor.name() : QStringLiteral("#909399"));
    m_up->setStyleSheet(tipsCss);
    m_down->setStyleSheet(tipsCss);
}

void MiniBar::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        Q_EMIT expandRequested();
        e->accept();
        return;
    }
    DWidget::mouseDoubleClickEvent(e);
}

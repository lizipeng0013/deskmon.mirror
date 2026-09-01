// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DESKMON_EMOJIICON_H
#define DESKMON_EMOJIICON_H

// 菜单/按钮文字里直接写 emoji 字符会显示成缺字方框：Qt 的字体回退不会
// 自动命中彩色 emoji 字体（界面字体思源黑体不含 emoji 字形，dde-shell
// 渲染托盘菜单时同理），需要用 emoji 字体显式绘成 QIcon 使用。

#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QString>

// 用彩色 emoji 字体绘制单个 emoji，绘制失败（系统无 emoji 字体）时
// 回退 Symbola 单色字形
inline QIcon emojiIcon(char32_t ucs4, int pixelSize = 48)
{
    QPixmap pm(pixelSize + pixelSize / 3, pixelSize + pixelSize / 3);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    QFont f;
    f.setFamilies({QStringLiteral("Noto Color Emoji"), QStringLiteral("Symbola")});
    f.setPixelSize(pixelSize);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, QString::fromUcs4(&ucs4, 1));
    p.end();
    return QIcon(pm);
}

#endif // DESKMON_EMOJIICON_H

// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "themecolors.h"

#include <DGuiApplicationHelper>
#include <DPalette>

DGUI_USE_NAMESPACE

QVector<QColor> ThemeColors::metricColors()
{
    auto *helper = DGuiApplicationHelper::instance();
    const QColor active = helper->applicationPalette().color(DPalette::Highlight);
    const bool dark = helper->themeType() == DGuiApplicationHelper::DarkType;

    // 亮度：暗主题需提亮、亮主题压暗，保证两种背景下进度条都清晰
    const double v = dark ? 0.82 : 0.58;
    const double s = 0.85;

    return {
        active.isValid() && active.alpha() > 0 ? active : QColor(QStringLiteral("#0081ff")),  // CPU：活跃色
        QColor::fromHsvF(180.0 / 360.0, s, v),   // 内存：青
        QColor::fromHsvF(330.0 / 360.0, s, v),   // GPU：粉
        QColor::fromHsvF(48.0 / 360.0, s, v),    // 系统盘：黄
    };
}

QPair<QColor, QColor> ThemeColors::netArrowColors()
{
    auto *helper = DGuiApplicationHelper::instance();
    const bool dark = helper->themeType() == DGuiApplicationHelper::DarkType;
    const double v = dark ? 0.8 : 0.55;
    const double s = 0.85;
    return {
        QColor::fromHsvF(28.0 / 360.0, s, v),    // 上传：橙
        QColor::fromHsvF(150.0 / 360.0, s, v),   // 下载：绿
    };
}

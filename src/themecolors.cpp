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

    // 迷你条空间极小，饱和度过高会显得刺眼、细碎；降饱和并微调色相，
    // 让四个指标在 DTK 半透明蓝底上更和谐。
    const double v = dark ? 0.76 : 0.52;
    const double s = 0.55;

    // CPU 亮色主题下调亮度，避免在浅底上过浓
    const QColor cpuColor = active.isValid() && active.alpha() > 0
        ? (dark ? active : active.darker(108))
        : QColor(QStringLiteral("#5B8FF9"));
    // GPU 亮色单独处理：更亮、更低饱和，提升浅紫灰底上的可读性
    const QColor gpuColor = dark
        ? QColor::fromHsvF(340.0 / 360.0, s, v)
        : QColor::fromHsvF(340.0 / 360.0, 0.42, 0.62);

    return {
        cpuColor,                                       // CPU：活跃色
        QColor::fromHsvF(170.0 / 360.0, s, v),         // 内存：薄荷 teal
        gpuColor,                                       // GPU：灰粉 rose
        QColor::fromHsvF(40.0 / 360.0, s + 0.05, v + (dark ? 0.02 : 0.03)),  // 系统盘：琥珀 amber
    };
}

QPair<QColor, QColor> ThemeColors::netArrowColors()
{
    auto *helper = DGuiApplicationHelper::instance();
    const bool dark = helper->themeType() == DGuiApplicationHelper::DarkType;
    const double v = dark ? 0.74 : 0.52;
    return {
        QColor::fromHsvF(270.0 / 360.0, 0.50, v),   // 上传：紫（与系统盘琥珀黄拉开色相距离）
        QColor::fromHsvF(150.0 / 360.0, 0.45, v),  // 下载：鼠尾草 sage
    };
}

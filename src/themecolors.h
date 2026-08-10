// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKMON_THEMECOLORS_H
#define DESKMON_THEMECOLORS_H

#include <QColor>
#include <QVector>
#include <QPair>

/**
 * @brief 主题跟随的指标配色
 *
 * 由系统活跃色（DPalette::Highlight）派生各指标色：
 * - CPU 直接用系统活跃色（字面跟随主题强调色）
 * - 内存/GPU/系统盘保持熟悉色相，随明/暗主题调整亮度保证可见
 */
namespace ThemeColors {

// 顺序：CPU / 内存 / GPU / 系统盘
QVector<QColor> metricColors();

// 网络箭头配色：(上传, 下载)，随明/暗主题调亮度
QPair<QColor, QColor> netArrowColors();

} // namespace ThemeColors

#endif // DESKMON_THEMECOLORS_H

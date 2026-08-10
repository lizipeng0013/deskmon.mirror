// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DESKMON_SETTINGS_DIALOG_H
#define DESKMON_SETTINGS_DIALOG_H

#include <DDialog>
#include <DSlider>
#include <DComboBox>
#include <DCheckBox>

class Config;
class QLabel;

/**
 * @brief 原生设置面板
 *
 * 包含：透明度滑条、刷新间隔、GPU/系统盘显隐、窗口置顶、开机自启。
 * 确定后由 MonitorWidget 从 Config 重新应用 UI（见 buttonClicked 连接）。
 */
class SettingsDialog : public DTK_WIDGET_NAMESPACE::DDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(Config *config, bool gpuAvailable, QWidget *parent = nullptr);

signals:
    void settingsApplied();     // 点「确定」保存到 Config 后发出

private:
    void buildUi();
    void saveToConfig();

    Config *m_config = nullptr;
    bool m_gpuAvailable = false;

    DTK_WIDGET_NAMESPACE::DSlider *m_opacitySlider = nullptr;
    QLabel *m_opacityValue = nullptr;
    DTK_WIDGET_NAMESPACE::DComboBox *m_refreshCombo = nullptr;
    DTK_WIDGET_NAMESPACE::DCheckBox *m_showGpuCheck = nullptr;
    DTK_WIDGET_NAMESPACE::DCheckBox *m_showDiskCheck = nullptr;
    DTK_WIDGET_NAMESPACE::DCheckBox *m_stayOnTopCheck = nullptr;
    DTK_WIDGET_NAMESPACE::DCheckBox *m_autostartCheck = nullptr;
};

#endif // DESKMON_SETTINGS_DIALOG_H

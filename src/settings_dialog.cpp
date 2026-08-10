// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings_dialog.h"

#include "config.h"

#include <DSlider>
#include <DComboBox>
#include <DCheckBox>

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

DWIDGET_USE_NAMESPACE

SettingsDialog::SettingsDialog(Config *config, bool gpuAvailable, QWidget *parent)
    : DDialog(parent)
    , m_config(config)
    , m_gpuAvailable(gpuAvailable)
{
    setWindowTitle(tr("DeskMon 设置"));
    setIcon(QIcon::fromTheme(QStringLiteral("deskmon")));
    buildUi();

    // 确定/取消（index 由 addButton 返回值决定）
    const int ok = addButton(tr("确定"), true, DDialog::ButtonRecommend);
    addButton(tr("取消"), false, DDialog::ButtonNormal);
    connect(this, &DDialog::buttonClicked, this, [this, ok](int index, const QString &) {
        if (index == ok) {
            saveToConfig();
            Q_EMIT settingsApplied();
        }
    });
}

void SettingsDialog::buildUi()
{
    auto *content = new QWidget(this);
    auto *form = new QFormLayout(content);
    form->setContentsMargins(20, 10, 20, 10);
    form->setSpacing(12);

    // 透明度
    auto *opacityRow = new QWidget(content);
    auto *opacityLayout = new QHBoxLayout(opacityRow);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    m_opacitySlider = new DSlider(Qt::Horizontal, opacityRow);
    m_opacitySlider->setMinimum(50);
    m_opacitySlider->setMaximum(100);
    m_opacitySlider->setValue(qRound(m_config->opacity() * 100));
    m_opacitySlider->setPageStep(5);
    m_opacityValue = new QLabel(opacityRow);
    m_opacityValue->setMinimumWidth(36);
    opacityLayout->addWidget(m_opacitySlider, 1);
    opacityLayout->addWidget(m_opacityValue);
    connect(m_opacitySlider, &DSlider::valueChanged, this, [this](int v) {
        m_opacityValue->setText(QStringLiteral("%1%").arg(v));
    });
    m_opacityValue->setText(QStringLiteral("%1%").arg(m_opacitySlider->value()));
    form->addRow(tr("透明度"), opacityRow);

    // 刷新间隔
    m_refreshCombo = new DComboBox(content);
    m_refreshCombo->addItem(tr("0.5 秒"), 500);
    m_refreshCombo->addItem(tr("1 秒"), 1000);
    m_refreshCombo->addItem(tr("2 秒"), 2000);
    m_refreshCombo->addItem(tr("3 秒"), 3000);
    m_refreshCombo->addItem(tr("5 秒"), 5000);
    const int cur = m_refreshCombo->findData(m_config->refreshInterval());
    m_refreshCombo->setCurrentIndex(cur >= 0 ? cur : 1);
    form->addRow(tr("刷新间隔"), m_refreshCombo);

    // GPU 显隐（无 N 卡时禁用，降级策略）
    m_showGpuCheck = new DCheckBox(tr("显示 GPU 指标"), content);
    m_showGpuCheck->setChecked(m_config->showGpu());
    m_showGpuCheck->setEnabled(m_gpuAvailable);
    form->addRow(QString(), m_showGpuCheck);

    // 系统盘显隐
    m_showDiskCheck = new DCheckBox(tr("显示系统盘"), content);
    m_showDiskCheck->setChecked(m_config->showDisk());
    form->addRow(QString(), m_showDiskCheck);

    // 窗口置顶
    m_stayOnTopCheck = new DCheckBox(tr("窗口置顶"), content);
    m_stayOnTopCheck->setChecked(m_config->stayOnTop());
    form->addRow(QString(), m_stayOnTopCheck);

    // 开机自启
    m_autostartCheck = new DCheckBox(tr("开机自启动"), content);
    m_autostartCheck->setChecked(m_config->isAutostartEnabled());
    form->addRow(QString(), m_autostartCheck);

    addContent(content);
    setMinimumWidth(360);
}

void SettingsDialog::saveToConfig()
{
    m_config->set(QStringLiteral("opacity"), m_opacitySlider->value() / 100.0);
    m_config->set(QStringLiteral("refresh_interval"), m_refreshCombo->currentData());
    m_config->set(QStringLiteral("show_gpu"), m_showGpuCheck->isChecked());
    m_config->set(QStringLiteral("show_disk"), m_showDiskCheck->isChecked());
    m_config->set(QStringLiteral("stay_on_top"), m_stayOnTopCheck->isChecked());
    m_config->setAutostart(m_autostartCheck->isChecked());
}

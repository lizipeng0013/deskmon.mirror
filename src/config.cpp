// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDebug>

const QVariantMap Config::s_defaults = {
    {QStringLiteral("opacity"), 0.85},
    {QStringLiteral("refresh_interval"), 1000},
    {QStringLiteral("position_x"), -1},
    {QStringLiteral("position_y"), -1},
    {QStringLiteral("window_width"), 220},
    {QStringLiteral("show_gpu"), true},
    {QStringLiteral("show_disk"), true},
    {QStringLiteral("autostart"), false},
    {QStringLiteral("stay_on_top"), true},
    {QStringLiteral("display_mode"), QStringLiteral("full")},
};

Config::Config()
    : m_data(s_defaults)
{
    load();
}

QString Config::configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/deskmon");
}

QString Config::configFile()
{
    return configDir() + QStringLiteral("/config.json");
}

QString Config::autostartDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart");
}

QString Config::autostartFile()
{
    return autostartDir() + QStringLiteral("/deskmon.desktop");
}

void Config::load()
{
    QFile file(configFile());
    if (!file.open(QIODevice::ReadOnly)) {
        qInfo() << "配置不存在，使用默认值:" << configFile();
        return;
    }

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "配置文件解析失败:" << err.errorString();
        return;
    }

    const QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        m_data.insert(it.key(), it.value().toVariant());
}

void Config::save()
{
    QDir().mkpath(configDir());
    QJsonObject obj;
    for (auto it = m_data.constBegin(); it != m_data.constEnd(); ++it)
        obj.insert(it.key(), QJsonValue::fromVariant(it.value()));

    QFile file(configFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "保存配置失败:" << configFile();
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

QVariant Config::get(const QString &key, const QVariant &defaultValue) const
{
    return m_data.value(key, defaultValue);
}

void Config::set(const QString &key, const QVariant &value)
{
    m_data.insert(key, value);
    save();
}

void Config::setMany(const QVariantMap &items)
{
    for (auto it = items.constBegin(); it != items.constEnd(); ++it)
        m_data.insert(it.key(), it.value());
    save();
}

double Config::opacity() const { return m_data.value(QStringLiteral("opacity")).toDouble(); }
int Config::refreshInterval() const { return m_data.value(QStringLiteral("refresh_interval")).toInt(); }
int Config::positionX() const { return m_data.value(QStringLiteral("position_x")).toInt(); }
int Config::positionY() const { return m_data.value(QStringLiteral("position_y")).toInt(); }
int Config::windowWidth() const { return m_data.value(QStringLiteral("window_width")).toInt(); }
bool Config::showGpu() const { return m_data.value(QStringLiteral("show_gpu")).toBool(); }
bool Config::showDisk() const { return m_data.value(QStringLiteral("show_disk")).toBool(); }
bool Config::autostart() const { return m_data.value(QStringLiteral("autostart")).toBool(); }
bool Config::stayOnTop() const { return m_data.value(QStringLiteral("stay_on_top")).toBool(); }
QString Config::displayMode() const { return m_data.value(QStringLiteral("display_mode")).toString(); }
void Config::setDisplayMode(const QString &mode) { set(QStringLiteral("display_mode"), mode); }

bool Config::isAutostartEnabled() const
{
    return QFile::exists(autostartFile());
}

void Config::setAutostart(bool enabled)
{
    if (enabled) {
        QDir().mkpath(autostartDir());
        const QString exec = QCoreApplication::applicationFilePath();
        const QString content = QStringLiteral(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=DeskMon\n"
            "Comment=桌面系统监控小工具\n"
            "Exec=%1\n"
            "Icon=deskmon\n"
            "Terminal=false\n"
            "Categories=System;Monitor;\n"
            "X-GNOME-Autostart-enabled=true\n").arg(exec);
        QFile f(autostartFile());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(content.toUtf8());
    } else {
        QFile::remove(autostartFile());
    }
    set(QStringLiteral("autostart"), enabled);
}

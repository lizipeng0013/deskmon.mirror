// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKMON_CONFIG_H
#define DESKMON_CONFIG_H

#include <QString>
#include <QVariantMap>

/**
 * @brief 配置管理，读写 ~/.config/deskmon/config.json（兼容原版 PyQt5 格式）
 */
class Config
{
public:
    explicit Config();

    void load();
    void save();

    QVariant get(const QString &key) const;
    void set(const QString &key, const QVariant &value);

    // 便捷访问
    double opacity() const;
    int refreshInterval() const;
    int positionX() const;
    int positionY() const;
    int windowWidth() const;
    bool showGpu() const;
    bool showDisk() const;
    bool autostart() const;
    bool stayOnTop() const;

    static QString configDir();
    static QString configFile();
    static QString autostartDir();
    static QString autostartFile();

    bool isAutostartEnabled() const;
    void setAutostart(bool enabled);

private:
    QVariantMap m_data;
    static const QVariantMap s_defaults;
};

#endif // DESKMON_CONFIG_H

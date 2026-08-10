// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-2.0-or-later

#include "monitorwidget.h"

#include <DApplication>
#include <DGuiApplicationHelper>

#include <QGuiApplication>
#include <QDBusInterface>
#include <QDBusConnection>

DWIDGET_USE_NAMESPACE
DGUI_USE_NAMESPACE

static bool appearanceIsDark()
{
    QDBusInterface iface(QStringLiteral("org.deepin.dde.Appearance1"),
                         QStringLiteral("/org/deepin/dde/Appearance1"),
                         QStringLiteral("org.deepin.dde.Appearance1"),
                         QDBusConnection::sessionBus());
    if (!iface.isValid())
        return false;
    const QString global = iface.property("GlobalTheme").toString();
    const QString gtk = iface.property("GtkTheme").toString();
    return global.contains(QStringLiteral("dark"), Qt::CaseInsensitive)
           || gtk.contains(QStringLiteral("dark"), Qt::CaseInsensitive);
}

static void syncThemeToAppearance()
{
    auto *helper = DGuiApplicationHelper::instance();
    if (!helper)
        return;
    const bool dark = appearanceIsDark();
    helper->setPaletteType(dark ? DGuiApplicationHelper::DarkType : DGuiApplicationHelper::LightType);
}

class ThemeSyncHelper : public QObject
{
    Q_OBJECT
public slots:
    void onAppearanceChanged(const QString &ty, const QString &)
    {
        // DDE Changed 信号传的 ty 是小写（"globaltheme"/"gtk"），做大小写无关比较
        if (ty.compare(QLatin1String("GlobalTheme"), Qt::CaseInsensitive) == 0
            || ty.compare(QLatin1String("GtkTheme"), Qt::CaseInsensitive) == 0)
            syncThemeToAppearance();
    }
};

int main(int argc, char *argv[])
{
    // 必须在创建 QGuiApplication 之前设置（DApplication 内部会创建）
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // DApplication 在 QApplication 基础上额外处理 DTK 主题、字体、单实例等。
    // 注意：不使用 DLogManager 注册 Appender —— jm-prefix 的 DTK6 LogManager
    // 替换 Qt 消息处理器后会破坏事件循环（QTimer 不再触发）。
    // 直接使用 Qt 原生日志（qDebug/qInfo -> stderr）。
    DApplication app(argc, argv);
    app.setOrganizationName("deskmon");
    app.setApplicationName("deskmon");
    app.setApplicationVersion("1.0.0");
    app.setProductName(QObject::tr("DeskMon"));
    app.setApplicationDisplayName(QObject::tr("DeskMon"));
    // 目前未内置翻译资源，跳过 loadTranslator 以免空转警告（打包时如有翻译再启用）
    // app.loadTranslator();

    // 单实例：第二个实例启动时激活已有窗口，而非新开
    if (!app.setSingleInstance("deskmon")) {
        qWarning() << "DeskMon 已在运行，本实例退出";
        return 0;
    }

    // DTK 默认不会自动跟随 DDE 的 GlobalTheme 变化（尤其非商店安装的应用），
    // 这里通过 dbus 同步一次，并监听 Changed 信号实现切主题实时响应。
    syncThemeToAppearance();
    ThemeSyncHelper themeHelper;
    QDBusConnection::sessionBus().connect(QStringLiteral("org.deepin.dde.Appearance1"),
                                          QStringLiteral("/org/deepin/dde/Appearance1"),
                                          QStringLiteral("org.deepin.dde.Appearance1"),
                                          QStringLiteral("Changed"),
                                          &themeHelper,
                                          SLOT(onAppearanceChanged(QString, QString)));

    MonitorWidget w;
    QObject::connect(&app, &DApplication::newInstanceStarted, &w, &MonitorWidget::activateWindow);
    w.show();

    return app.exec();
}

#include "main.moc"

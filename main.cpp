// SPDX-FileCopyrightText: 2026 kookboy
// SPDX-License-Identifier: GPL-3.0-or-later

#include "monitorwidget.h"

#include <DApplication>

#include <QGuiApplication>

DWIDGET_USE_NAMESPACE

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

    MonitorWidget w;
    QObject::connect(&app, &DApplication::newInstanceStarted, &w, &MonitorWidget::activateWindow);
    w.show();

    return app.exec();
}

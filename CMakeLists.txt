cmake_minimum_required(VERSION 3.16)
project(deskmon VERSION 1.0.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 若未通过系统默认路径找到 DTK6/Qt6，允许外部传入 CMAKE_PREFIX_PATH。
# 本机工具链位于 /home/kookboy/jm-prefix/usr（Qt 6.8.0 + DTK6 6.7.44）
if(NOT DEFINED CMAKE_PREFIX_PATH AND EXISTS "$ENV{HOME}/jm-prefix/usr/lib/x86_64-linux-gnu/cmake/Dtk6Widget")
    list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/jm-prefix/usr")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(Dtk6Core REQUIRED)
find_package(Dtk6Gui REQUIRED)
find_package(Dtk6Widget REQUIRED)

add_executable(deskmon
    main.cpp
    src/config.cpp
    src/systemmonitor.cpp
    src/nvidia_gpu.cpp
    src/processmgr.cpp
    src/themecolors.cpp
    src/settings_dialog.cpp
    src/process_dialog.cpp
    src/gpu_dialog.cpp
    src/widgets/metricrow.cpp
    src/widgets/netwidget.cpp
    src/widgets/sparkline.cpp
    src/widgets/pomodoro.cpp
    src/widgets/minibar.cpp
    src/monitorwidget.cpp
)

target_include_directories(deskmon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
)

# 版本号单一来源：project VERSION → 编译宏 → 关于对话框 / applicationVersion
target_compile_definitions(deskmon PRIVATE DESKMON_VERSION="${PROJECT_VERSION}")

target_link_libraries(deskmon PRIVATE
    Qt6::Core
    Qt6::Widgets
    Dtk6::Core
    Dtk6::Gui
    Dtk6::Widget
)

# 安装规则（deb 打包用）
install(TARGETS deskmon RUNTIME DESTINATION bin)
# 主图标：scalable SVG（系统主题 QIcon::fromTheme("deskmon") 的解析来源）
install(FILES icons/deskmon.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME deskmon.svg)
# 主图标：多尺寸 PNG（hicolor 主题按 size 查找，含 16/22/24/32/48/64/128/256）
install(DIRECTORY icons/hicolor/
        DESTINATION share/icons/hicolor/
        FILES_MATCHING PATTERN "*.png"
        PATTERN "*" EXCLUDE)
# 托盘 / symbolic 变体（备选资源，随包发布）
install(FILES icons/deskmon-tray.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME deskmon-tray.svg)
install(FILES icons/deskmon-symbolic.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME deskmon-symbolic.svg)
install(DIRECTORY icons/tray/
        DESTINATION share/icons/deskmon/tray/)
install(DIRECTORY icons/symbolic/
        DESTINATION share/icons/deskmon/symbolic/)
install(FILES deskmon.desktop
        DESTINATION share/applications)

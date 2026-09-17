cmake_minimum_required(VERSION 3.19)
project(markdown_helper LANGUAGES CXX)

# 项目使用 C++17，保证 Qt Creator 和命令行构建采用相同标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets PrintSupport)

qt_standard_project_setup()

qt_add_executable(markdown_helper
    WIN32 MACOSX_BUNDLE
    main.cpp
    note.h
    notestorage.cpp
    notestorage.h
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_link_libraries(markdown_helper
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::PrintSupport
)

install(TARGETS markdown_helper
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET markdown_helper
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

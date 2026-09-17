#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    // 每个 Qt Widgets 程序都需要唯一的 QApplication 对象
    QApplication a(argc, argv);

    // 应用名称会用于系统数据目录以及桌面环境中的显示
    QApplication::setApplicationName(QStringLiteral("markdown_helper"));
    QApplication::setApplicationDisplayName(QStringLiteral("Markdown 笔记管理器"));

    // 创建并显示应用的主窗口
    MainWindow w;
    w.show();

    // 进入 Qt 事件循环并等待用户操作
    return QApplication::exec();
}

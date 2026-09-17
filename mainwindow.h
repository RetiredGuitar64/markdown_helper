#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTextBrowser;
class QTreeView;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // 根据编辑区中的 Markdown 文本刷新右侧预览
    void updatePreview();

private:
    // 创建主窗口中需要的控件和布局
    void setupInterface();

    // 设置简单统一的界面样式
    void setupStyle();

    // Qt Designer 生成的界面对象
    Ui::MainWindow *ui;

    // 左侧用于查找笔记的输入框
    QLineEdit *searchEdit;

    // 左侧用于显示文件夹和笔记的树
    QTreeView *noteTreeView;

    // 编辑区上方显示当前笔记标题
    QLineEdit *titleEdit;

    // 编辑区上方用于填写逗号分隔的标签
    QLineEdit *tagEdit;

    // 中间的 Markdown 源码编辑器
    QPlainTextEdit *markdownEditor;

    // 右侧的 Markdown 渲染预览框
    QTextBrowser *previewBrowser;

    // 状态栏中显示字数信息的标签
    QLabel *wordCountLabel;
};
#endif // MAINWINDOW_H

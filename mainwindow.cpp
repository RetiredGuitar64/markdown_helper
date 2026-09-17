#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFontDatabase>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QTextBrowser>
#include <QTreeView>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , searchEdit(nullptr)
    , noteTreeView(nullptr)
    , titleEdit(nullptr)
    , tagEdit(nullptr)
    , markdownEditor(nullptr)
    , previewBrowser(nullptr)
    , wordCountLabel(nullptr)
{
    // 先载入 Qt Designer 中的主窗口基础结构
    ui->setupUi(this);

    // 再创建项目实际使用的编辑界面
    setupInterface();
    setupStyle();

    // 编辑内容改变时立即更新预览
    connect(markdownEditor, &QPlainTextEdit::textChanged,
            this, &MainWindow::updatePreview);

    // 放入一段示例内容方便第一次运行时查看效果
    markdownEditor->setPlainText(
        QStringLiteral("# 欢迎使用 Markdown 笔记\n\n"
                       "在左侧管理笔记，在这里输入 **Markdown** 内容\n\n"
                       "- 支持标题和列表\n"
                       "- 支持代码块与表格\n\n"
                       "```cpp\n"
                       "qDebug() << \"Hello Markdown\";\n"
                       "```\n"));
}

MainWindow::~MainWindow()
{
    // ui 对象拥有 Designer 创建的控件，需要在退出时释放
    delete ui;
}

void MainWindow::setupInterface()
{
    // 设置应用标题和适合编辑工作的初始大小
    setWindowTitle(QStringLiteral("Markdown 笔记管理器"));
    resize(1180, 720);
    setMinimumSize(900, 560);

    // 最外层布局只放置一个横向分割器
    auto *mainLayout = new QVBoxLayout(ui->centralwidget);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // 分割器允许用户自由调整导航、编辑和预览的宽度
    auto *mainSplitter = new QSplitter(Qt::Horizontal, ui->centralwidget);
    mainLayout->addWidget(mainSplitter);

    // 左侧区域放置搜索框和笔记树
    auto *navigationWidget = new QWidget(mainSplitter);
    auto *navigationLayout = new QVBoxLayout(navigationWidget);
    navigationLayout->setContentsMargins(0, 0, 0, 0);

    auto *navigationTitle = new QLabel(QStringLiteral("笔记列表"), navigationWidget);
    navigationTitle->setObjectName(QStringLiteral("sectionTitle"));
    navigationLayout->addWidget(navigationTitle);

    searchEdit = new QLineEdit(navigationWidget);
    searchEdit->setPlaceholderText(QStringLiteral("搜索标题或正文"));
    searchEdit->setClearButtonEnabled(true);
    navigationLayout->addWidget(searchEdit);

    noteTreeView = new QTreeView(navigationWidget);
    noteTreeView->setHeaderHidden(true);
    noteTreeView->setAlternatingRowColors(true);
    navigationLayout->addWidget(noteTreeView, 1);

    // 中间区域由标题、标签和源码编辑器组成
    auto *editorWidget = new QWidget(mainSplitter);
    auto *editorLayout = new QVBoxLayout(editorWidget);
    editorLayout->setContentsMargins(8, 0, 8, 0);

    auto *editorTitle = new QLabel(QStringLiteral("Markdown 编辑"), editorWidget);
    editorTitle->setObjectName(QStringLiteral("sectionTitle"));
    editorLayout->addWidget(editorTitle);

    auto *informationLayout = new QFormLayout;
    informationLayout->setContentsMargins(0, 0, 0, 0);

    titleEdit = new QLineEdit(editorWidget);
    titleEdit->setPlaceholderText(QStringLiteral("笔记标题"));
    informationLayout->addRow(QStringLiteral("标题"), titleEdit);

    tagEdit = new QLineEdit(editorWidget);
    tagEdit->setPlaceholderText(QStringLiteral("学习, Qt, 随笔"));
    informationLayout->addRow(QStringLiteral("标签"), tagEdit);
    editorLayout->addLayout(informationLayout);

    markdownEditor = new QPlainTextEdit(editorWidget);
    markdownEditor->setPlaceholderText(QStringLiteral("在这里输入 Markdown 内容"));
    markdownEditor->setTabStopDistance(32);
    editorLayout->addWidget(markdownEditor, 1);

    // 右侧区域只负责显示渲染后的内容
    auto *previewWidget = new QWidget(mainSplitter);
    auto *previewLayout = new QVBoxLayout(previewWidget);
    previewLayout->setContentsMargins(8, 0, 0, 0);

    auto *previewTitle = new QLabel(QStringLiteral("实时预览"), previewWidget);
    previewTitle->setObjectName(QStringLiteral("sectionTitle"));
    previewLayout->addWidget(previewTitle);

    previewBrowser = new QTextBrowser(previewWidget);
    previewBrowser->setOpenExternalLinks(true);
    previewLayout->addWidget(previewBrowser, 1);

    // 让编辑区和预览区占用较多空间
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 2);
    mainSplitter->setStretchFactor(2, 2);
    mainSplitter->setSizes({220, 470, 470});

    // 状态栏右侧常驻显示当前字符数
    wordCountLabel = new QLabel(QStringLiteral("字符数: 0"), this);
    ui->statusbar->addPermanentWidget(wordCountLabel);
    ui->statusbar->showMessage(QStringLiteral("准备就绪"));
}

void MainWindow::setupStyle()
{
    // 使用系统自带的等宽字体显示 Markdown 源码
    QFont editorFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    editorFont.setPointSize(11);
    markdownEditor->setFont(editorFont);

    // 少量样式用于区分标题并改善输入框间距
    setStyleSheet(QStringLiteral(
        "QLabel#sectionTitle { font-size: 16px; font-weight: bold; padding: 4px 0; }"
        "QLineEdit { padding: 5px; }"
        "QTreeView, QPlainTextEdit, QTextBrowser { border: 1px solid #c8c8c8; }"));
}

void MainWindow::updatePreview()
{
    // QTextDocument 原生支持常见 Markdown 语法
    previewBrowser->document()->setMarkdown(markdownEditor->toPlainText());

    // 字符数包含空格和换行，计算方式直观且稳定
    const int characterCount = markdownEditor->toPlainText().length();
    wordCountLabel->setText(QStringLiteral("字符数: %1").arg(characterCount));
}

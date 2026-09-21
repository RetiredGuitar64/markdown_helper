#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPrinter>
#include <QSaveFile>
#include <QSplitter>
#include <QStatusBar>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , wordCountLabel(nullptr)
    , noteTreeModel(nullptr)
    , searchSourceModel(nullptr)
    , searchProxyModel(nullptr)
    , saveTimer(nullptr)
{
    // 先载入 Qt Designer 中的主窗口基础结构
    ui->setupUi(this);

    // 再创建项目实际使用的编辑界面
    setupInterface();
    setupStyle();
    setupActions();

    // 单次计时器在停止输入一小段时间后执行保存
    saveTimer = new QTimer(this);
    saveTimer->setSingleShot(true);
    saveTimer->setInterval(600);

    // 编辑内容改变时立即更新预览
    connect(ui->markdownEditor, &QPlainTextEdit::textChanged,
            this, &MainWindow::updatePreview);
    const auto queueSave = [this] {
        // 只有打开笔记后才允许启动自动保存计时器
        if (!currentNoteId.isEmpty()) {
            saveTimer->start();
            ui->statusbar->showMessage(QStringLiteral("内容已修改，等待自动保存"));
        }
    };
    connect(ui->markdownEditor, &QPlainTextEdit::textChanged, this, queueSave);
    connect(ui->titleEdit, &QLineEdit::textChanged, this, queueSave);
    connect(saveTimer, &QTimer::timeout,
            this, &MainWindow::saveCurrentNote);

    // 单击树中的笔记时将它载入编辑区
    connect(ui->noteTreeView, &QTreeView::clicked,
            this, &MainWindow::openTreeItem);

    // 搜索文字改变时马上刷新左侧结果
    connect(ui->searchEdit, &QLineEdit::textChanged,
            this, &MainWindow::updateSearch);
    connect(ui->searchResultView, &QListView::clicked,
            this, &MainWindow::openSearchResult);

    // 最后读取磁盘数据并显示上次保存的笔记
    loadNotes();
}

MainWindow::~MainWindow()
{
    // ui 对象拥有 Designer 创建的控件，需要在退出时释放
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 窗口关闭前立即执行一次保存，避免等待中的内容丢失
    saveCurrentNote();
    event->accept();
}

void MainWindow::setupInterface()
{
    // 让编辑区和预览区占用较多空间
    ui->mainSplitter->setStretchFactor(0, 1);
    ui->mainSplitter->setStretchFactor(1, 2);
    ui->mainSplitter->setStretchFactor(2, 2);
    ui->mainSplitter->setSizes({220, 470, 470});

    // 设置源码编辑器中制表符的显示宽度
    ui->markdownEditor->setTabStopDistance(32);

    // 状态栏右侧常驻显示当前字符数
    wordCountLabel = new QLabel(QStringLiteral("字符数: 0"), this);
    ui->statusbar->addPermanentWidget(wordCountLabel);
    ui->statusbar->showMessage(QStringLiteral("准备就绪"));
}

void MainWindow::setupActions()
{
    // 文件菜单集中放置最常用的创建和保存操作
    QMenu *fileMenu = ui->menubar->addMenu(QStringLiteral("文件"));
    QToolBar *toolBar = addToolBar(QStringLiteral("常用操作"));
    toolBar->setMovable(false);

    QAction *newNoteAction = new QAction(QStringLiteral("新建笔记"), this);
    newNoteAction->setShortcut(QKeySequence::New);
    connect(newNoteAction, &QAction::triggered, this, &MainWindow::createNote);
    fileMenu->addAction(newNoteAction);
    toolBar->addAction(newNoteAction);

    QAction *newFolderAction = new QAction(QStringLiteral("新建文件夹"), this);
    newFolderAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")));
    connect(newFolderAction, &QAction::triggered, this, &MainWindow::createFolder);
    fileMenu->addAction(newFolderAction);
    toolBar->addAction(newFolderAction);

    QAction *saveAction = new QAction(QStringLiteral("保存"), this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveCurrentNote);
    fileMenu->addAction(saveAction);
    toolBar->addAction(saveAction);

    fileMenu->addSeparator();

    QAction *exportHtmlAction = new QAction(QStringLiteral("导出为 HTML"), this);
    connect(exportHtmlAction, &QAction::triggered,
            this, &MainWindow::exportCurrentNoteAsHtml);
    fileMenu->addAction(exportHtmlAction);

    QAction *exportPdfAction = new QAction(QStringLiteral("导出为 PDF"), this);
    connect(exportPdfAction, &QAction::triggered,
            this, &MainWindow::exportCurrentNoteAsPdf);
    fileMenu->addAction(exportPdfAction);

    // 编辑菜单提供当前树节点的管理操作
    QMenu *editMenu = ui->menubar->addMenu(QStringLiteral("编辑"));

    QAction *renameAction = new QAction(QStringLiteral("重命名"), this);
    renameAction->setShortcut(Qt::Key_F2);
    connect(renameAction, &QAction::triggered,
            this, &MainWindow::renameSelectedItem);
    editMenu->addAction(renameAction);

    QAction *moveAction = new QAction(QStringLiteral("移动笔记"), this);
    connect(moveAction, &QAction::triggered,
            this, &MainWindow::moveSelectedNote);
    editMenu->addAction(moveAction);

    QAction *deleteAction = new QAction(QStringLiteral("删除"), this);
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered,
            this, &MainWindow::deleteSelectedItem);
    editMenu->addAction(deleteAction);

    // 树视图使用同一组 QAction 生成右键菜单
    ui->noteTreeView->setContextMenuPolicy(Qt::ActionsContextMenu);
    ui->noteTreeView->addAction(renameAction);
    ui->noteTreeView->addAction(moveAction);
    ui->noteTreeView->addAction(deleteAction);
}

void MainWindow::setupStyle()
{
    // 使用系统自带的等宽字体显示 Markdown 源码
    QFont editorFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    editorFont.setPointSize(11);
    ui->markdownEditor->setFont(editorFont);

    // 少量样式用于区分标题并改善输入框间距
    setStyleSheet(QStringLiteral(
        "QLabel#sectionTitle, QLabel#editorTitle, QLabel#previewTitle "
        "{ font-size: 16px; font-weight: bold; padding: 4px 0; }"
        "QLineEdit { padding: 5px; }"
        "QTreeView, QPlainTextEdit, QTextBrowser { border: 1px solid #c8c8c8; }"));
}

void MainWindow::updatePreview()
{
    // QTextDocument 原生支持常见 Markdown 语法
    const QString content = ui->markdownEditor->toPlainText();
    ui->previewBrowser->document()->setMarkdown(content);

    // 字符数包含空格和换行，计算方式直观且稳定
    wordCountLabel->setText(QStringLiteral("字符数: %1").arg(content.length()));
}

QString MainWindow::safeExportFileName() const
{
    QString fileName = ui->titleEdit->text().trimmed();
    if (fileName.isEmpty()) {
        fileName = QStringLiteral("未命名笔记");
    }

    // 替换各平台文件名中常见的非法字符
    const QString invalidCharacters = QStringLiteral("\\/:*?\"<>|");
    for (const QChar character : invalidCharacters) {
        fileName.replace(character, QLatin1Char('_'));
    }

    return fileName;
}

void MainWindow::exportCurrentNoteAsHtml()
{
    if (currentNoteId.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("当前没有可以导出的笔记"));
        return;
    }

    // 先保存编辑内容，让导出结果和本地笔记保持一致
    saveCurrentNote();

    const QString suggestedPath = QDir::home().filePath(
        safeExportFileName() + QStringLiteral(".html"));
    const QString filePath = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出 HTML"), suggestedPath,
        QStringLiteral("HTML 文件 (*.html *.htm)"));
    if (filePath.isEmpty()) {
        return;
    }

    // QTextDocument 可以把当前渲染结果转换成完整 HTML
    QSaveFile outputFile(filePath);
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("无法写入选择的文件"));
        return;
    }

    outputFile.write(ui->previewBrowser->document()->toHtml().toUtf8());
    if (!outputFile.commit()) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("文件保存时发生错误"));
        return;
    }

    ui->statusbar->showMessage(QStringLiteral("HTML 导出成功"), 3000);
}

void MainWindow::exportCurrentNoteAsPdf()
{
    if (currentNoteId.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("当前没有可以导出的笔记"));
        return;
    }

    saveCurrentNote();

    const QString suggestedPath = QDir::home().filePath(
        safeExportFileName() + QStringLiteral(".pdf"));
    QString filePath = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出 PDF"), suggestedPath,
        QStringLiteral("PDF 文件 (*.pdf)"));
    if (filePath.isEmpty()) {
        return;
    }

    // 用户没有输入扩展名时自动补充 pdf 后缀
    if (!filePath.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
        filePath += QStringLiteral(".pdf");
    }

    // QPrinter 使用 PDF 输出模式时不需要系统中安装真实打印机
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setDocName(ui->titleEdit->text().trimmed());

    // 直接打印预览文档可以保留标题、列表、表格等排版
    ui->previewBrowser->document()->print(&printer);

    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("PDF 文件没有成功生成"));
        return;
    }

    ui->statusbar->showMessage(QStringLiteral("PDF 导出成功"), 3000);
}

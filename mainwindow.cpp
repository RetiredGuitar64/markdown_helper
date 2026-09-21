#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPrinter>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
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

void MainWindow::loadNotes()
{
    // 存储对象负责读取 JSON 并在首次运行时建立欢迎笔记
    if (!storage.load()) {
        ui->statusbar->showMessage(QStringLiteral("笔记数据载入失败"), 3000);
    }

    // 首次载入需要完整建立三个界面模型
    rebuildNoteTree();
    rebuildSearchIndex();
    // 默认打开第一篇笔记，避免界面显示为空
    if (!storage.notes().isEmpty()) {
        loadNote(storage.notes().first().id);
    } else {
        ui->titleEdit->setEnabled(false);
        ui->markdownEditor->setEnabled(false);
        ui->previewBrowser->setEnabled(false);
    }
}

void MainWindow::rebuildNoteTree()
{
    // 模型只创建一次，后续刷新时清除其中的旧节点
    if (noteTreeModel == nullptr) {
        noteTreeModel = new QStandardItemModel(this);
        ui->noteTreeView->setModel(noteTreeModel);
    } else {
        noteTreeModel->clear();
    }

    noteTreeModel->setHorizontalHeaderLabels({QStringLiteral("笔记")});

    // 未分类节点始终存在，方便接收没有文件夹的笔记
    QStringList visibleFolders = storage.folders();
    if (!visibleFolders.contains(QStringLiteral("未分类"))) {
        visibleFolders.prepend(QStringLiteral("未分类"));
    }

    for (const QString &folderName : visibleFolders) {
        QStandardItem *folderItem = new QStandardItem(folderName);
        folderItem->setEditable(false);
        folderItem->setData(FolderItem, ItemTypeRole);

        // 将属于当前文件夹的笔记依次添加为子节点
        for (const Note &note : storage.notes()) {
            const QString actualFolder = note.folder.isEmpty()
                ? QStringLiteral("未分类") : note.folder;

            if (actualFolder == folderName) {
                QStandardItem *noteItem = new QStandardItem(note.title);
                noteItem->setEditable(false);
                noteItem->setData(NoteItem, ItemTypeRole);
                noteItem->setData(note.id, NoteIdRole);
                folderItem->appendRow(noteItem);
            }
        }

        noteTreeModel->appendRow(folderItem);
    }

    ui->noteTreeView->expandAll();

    // 模型刷新后尽量恢复用户正在编辑的笔记选择
    selectNoteInTree(currentNoteId);
}

void MainWindow::rebuildSearchIndex()
{
    // 搜索数据源为每篇笔记保存一个扁平条目
    if (searchSourceModel == nullptr) {
        searchSourceModel = new QStandardItemModel(this);
    } else {
        searchSourceModel->clear();
    }

    for (const Note &note : storage.notes()) {
        const QString content = storage.readNoteContent(note);

        const QString folder = note.folder.isEmpty()
            ? QStringLiteral("未分类") : note.folder;
        QStandardItem *item = new QStandardItem(
            QStringLiteral("%1  [%2]").arg(note.title, folder));
        item->setEditable(false);
        item->setData(note.id, NoteIdRole);

        // 代理模型会在这段组合文字中执行不区分大小写的子串匹配
        const QString searchableText = note.title + QLatin1Char('\n')
            + content + QLatin1Char('\n') + note.tags.join(QLatin1Char(' '));
        item->setData(searchableText, SearchTextRole);
        searchSourceModel->appendRow(item);
    }

    if (searchProxyModel == nullptr) {
        searchProxyModel = new QSortFilterProxyModel(this);
        searchProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        searchProxyModel->setFilterRole(SearchTextRole);
        ui->searchResultView->setModel(searchProxyModel);
    }

    searchProxyModel->setSourceModel(searchSourceModel);

    // 保持当前搜索框中的关键字继续生效
    searchProxyModel->setFilterFixedString(ui->searchEdit->text().trimmed());
}

void MainWindow::updateSearchItem(const Note &note, const QString &content)
{
    if (noteTreeModel == nullptr || searchSourceModel == nullptr) {
        return;
    }

    // Qt 的递归 match 可以直接找到树中的笔记，不需要手写双层遍历
    const QModelIndexList treeMatches = noteTreeModel->match(
        noteTreeModel->index(0, 0), NoteIdRole, note.id, 1,
        Qt::MatchExactly | Qt::MatchRecursive);
    if (!treeMatches.isEmpty()) {
        noteTreeModel->setData(treeMatches.first(), note.title);
    }

    // 搜索模型同样按笔记编号找到并更新唯一条目
    const QModelIndexList searchMatches = searchSourceModel->match(
        searchSourceModel->index(0, 0), NoteIdRole, note.id, 1,
        Qt::MatchExactly);
    QStandardItem *item = searchMatches.isEmpty()
        ? new QStandardItem : searchSourceModel->itemFromIndex(searchMatches.first());
    if (searchMatches.isEmpty()) {
        item->setEditable(false);
        searchSourceModel->appendRow(item);
    }

    const QString folder = note.folder.isEmpty()
        ? QStringLiteral("未分类") : note.folder;
    item->setText(QStringLiteral("%1  [%2]").arg(note.title, folder));
    item->setData(note.id, NoteIdRole);

    // 只重新组合这一篇笔记的搜索内容
    const QString searchableText = note.title + QLatin1Char('\n')
        + content + QLatin1Char('\n') + note.tags.join(QLatin1Char(' '));
    item->setData(searchableText, SearchTextRole);

    // 重新设置当前关键字可让结果数量立即反映这一条数据的变化
    searchProxyModel->setFilterFixedString(ui->searchEdit->text().trimmed());
}

void MainWindow::loadNote(const QString &noteId)
{
    const Note *note = storage.findNote(noteId);
    if (note == nullptr) {
        return;
    }

    // 切换笔记前先保存上一篇正在编辑的内容
    if (!currentNoteId.isEmpty() && currentNoteId != noteId) {
        saveCurrentNote();
    }

    currentNoteId = note->id;

    // 阻止载入文本时把读取操作误判为用户编辑
    const QSignalBlocker titleBlocker(ui->titleEdit);
    const QSignalBlocker editorBlocker(ui->markdownEditor);
    ui->titleEdit->setText(note->title);

    // 存储对象按需读取这一篇笔记的 Markdown 正文
    ui->markdownEditor->setPlainText(storage.readNoteContent(*note));

    ui->titleEdit->setEnabled(true);
    ui->markdownEditor->setEnabled(true);
    ui->previewBrowser->setEnabled(true);
    updatePreview();
    selectNoteInTree(noteId);
    ui->statusbar->showMessage(QStringLiteral("已打开 %1").arg(note->title), 2000);
}

void MainWindow::openTreeItem(const QModelIndex &index)
{
    // 文件夹节点只负责展开和折叠，不载入编辑区
    if (index.data(ItemTypeRole).toInt() != NoteItem) {
        return;
    }

    loadNote(index.data(NoteIdRole).toString());
}

void MainWindow::selectNoteInTree(const QString &noteId)
{
    if (noteTreeModel == nullptr || noteTreeModel->rowCount() == 0) {
        return;
    }

    // Qt 模型直接递归匹配笔记编号并恢复选择
    const QModelIndexList matches = noteTreeModel->match(
        noteTreeModel->index(0, 0), NoteIdRole, noteId, 1,
        Qt::MatchExactly | Qt::MatchRecursive);
    if (!matches.isEmpty()) {
        ui->noteTreeView->setCurrentIndex(matches.first());
    }
}

void MainWindow::createNote()
{
    // 新笔记默认放入当前选中节点所在的文件夹
    const QModelIndex selectedIndex = ui->noteTreeView->currentIndex();
    QString targetFolder = QStringLiteral("未分类");
    if (selectedIndex.data(ItemTypeRole).toInt() == FolderItem) {
        targetFolder = selectedIndex.data().toString();
    } else if (selectedIndex.parent().isValid()) {
        targetFolder = selectedIndex.parent().data().toString();
    }
    bool accepted = false;
    const QString title = QInputDialog::getText(
        this, QStringLiteral("新建笔记"), QStringLiteral("笔记标题"),
        QLineEdit::Normal, QStringLiteral("未命名笔记"), &accepted).trimmed();

    // 用户取消或没有输入标题时不创建数据
    if (!accepted || title.isEmpty()) {
        return;
    }

    saveCurrentNote();

    const QString folder = targetFolder == QStringLiteral("未分类")
        ? QString() : targetFolder;
    const Note *note = storage.addNote(title, folder);
    if (note == nullptr) {
        QMessageBox::warning(this, QStringLiteral("创建失败"),
                             QStringLiteral("无法创建笔记文件"));
        return;
    }

    // 新增数据时重建树结构，并只追加这一条搜索数据
    const QString newNoteId = note->id;
    rebuildNoteTree();
    updateSearchItem(*note, QString());
    loadNote(newNoteId);
}

void MainWindow::createFolder()
{
    bool accepted = false;
    const QString folderName = QInputDialog::getText(
        this, QStringLiteral("新建文件夹"), QStringLiteral("文件夹名称"),
        QLineEdit::Normal, QString(), &accepted).trimmed();

    if (!accepted || folderName.isEmpty()) {
        return;
    }

    // 不允许创建重名文件夹，避免移动笔记时产生歧义
    if (storage.folders().contains(folderName)
        || folderName == QStringLiteral("未分类")) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("这个文件夹已经存在"));
        return;
    }

    if (!storage.addFolder(folderName)) {
        QMessageBox::warning(this, QStringLiteral("创建失败"),
                             QStringLiteral("文件夹保存失败"));
        return;
    }

    rebuildNoteTree();
    ui->statusbar->showMessage(QStringLiteral("文件夹创建成功"), 2000);
}

void MainWindow::saveCurrentNote()
{
    const Note *note = storage.findNote(currentNoteId);
    if (note == nullptr) {
        return;
    }

    saveTimer->stop();

    // 空标题自动恢复为未命名，保证树中始终有可见文字
    QString title = ui->titleEdit->text().trimmed();
    if (title.isEmpty()) {
        title = QStringLiteral("未命名笔记");
    }

    // 标签功能已经从界面移除，存储层会原样保留旧 JSON 中的标签
    const QString content = ui->markdownEditor->toPlainText();
    if (!storage.updateNote(currentNoteId, title, content)) {
        ui->statusbar->showMessage(QStringLiteral("正文保存失败"), 3000);
        return;
    }

    // 保存后重新取得稳定的只读指针
    note = storage.findNote(currentNoteId);
    if (note == nullptr) {
        return;
    }

    // 自动保存只更新当前笔记对应的树节点和搜索条目
    updateSearchItem(*note, content);
    ui->statusbar->showMessage(QStringLiteral("已自动保存"), 1800);
}

void MainWindow::updateSearch(const QString &keyword)
{
    if (searchProxyModel == nullptr) {
        return;
    }

    const QString trimmedKeyword = keyword.trimmed();
    searchProxyModel->setFilterFixedString(trimmedKeyword);

    // 没有关键字时隐藏结果列表，把空间还给笔记树
    ui->searchResultView->setVisible(!trimmedKeyword.isEmpty());
    if (!trimmedKeyword.isEmpty()) {
        ui->statusbar->showMessage(
            QStringLiteral("找到 %1 篇笔记").arg(searchProxyModel->rowCount()));
    } else {
        ui->statusbar->showMessage(QStringLiteral("准备就绪"));
    }
}

void MainWindow::openSearchResult(const QModelIndex &index)
{
    // 自定义角色会由代理模型自动映射到原始条目
    const QString noteId = index.data(NoteIdRole).toString();
    if (!noteId.isEmpty()) {
        loadNote(noteId);
    }
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

void MainWindow::renameSelectedItem()
{
    const QModelIndex currentIndex = ui->noteTreeView->currentIndex();
    if (currentIndex.data(ItemTypeRole).toInt() != NoteItem) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择一篇笔记"));
        return;
    }

    const QString oldName = currentIndex.data(Qt::DisplayRole).toString();
    bool accepted = false;
    const QString newName = QInputDialog::getText(
        this, QStringLiteral("重命名"), QStringLiteral("新的名称"),
        QLineEdit::Normal, oldName, &accepted).trimmed();

    if (!accepted || newName.isEmpty() || newName == oldName) {
        return;
    }

    const QString noteId = currentIndex.data(NoteIdRole).toString();
    if (!storage.renameNote(noteId, newName)) {
        ui->statusbar->showMessage(QStringLiteral("重命名保存失败"), 3000);
        return;
    }

    // 重命名只影响当前条目，无需重新读取其他 Markdown 文件
    const Note *note = storage.findNote(noteId);
    if (note != nullptr) {
        updateSearchItem(*note, storage.readNoteContent(*note));
    }
    if (noteId == currentNoteId) {
        const QSignalBlocker blocker(ui->titleEdit);
        ui->titleEdit->setText(newName);
    }
    ui->statusbar->showMessage(QStringLiteral("重命名完成"), 2000);
}

void MainWindow::deleteSelectedItem()
{
    const QModelIndex currentIndex = ui->noteTreeView->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择笔记或文件夹"));
        return;
    }

    const int itemType = currentIndex.data(ItemTypeRole).toInt();
    const QString itemName = currentIndex.data(Qt::DisplayRole).toString();
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, QStringLiteral("确认删除"),
        QStringLiteral("确定要删除“%1”吗？").arg(itemName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer != QMessageBox::Yes) {
        return;
    }

    if (itemType == NoteItem) {
        const QString noteId = currentIndex.data(NoteIdRole).toString();
        if (storage.findNote(noteId) == nullptr) {
            return;
        }

        // 删除前保存其他正在编辑的笔记
        if (currentNoteId != noteId) {
            saveCurrentNote();
        }

        if (!storage.removeNote(noteId)) {
            ui->statusbar->showMessage(QStringLiteral("删除保存失败"), 3000);
            return;
        }

        if (currentNoteId == noteId) {
            currentNoteId.clear();
        }

        // 删除搜索模型中的对应行，其他笔记无需重新读取
        const QModelIndexList matches = searchSourceModel->match(
            searchSourceModel->index(0, 0), NoteIdRole, noteId, 1,
            Qt::MatchExactly);
        if (!matches.isEmpty()) {
            searchSourceModel->removeRow(matches.first().row());
        }
    } else if (itemType == FolderItem) {
        // 未分类是显示用的固定节点，不能从列表移除
        if (itemName == QStringLiteral("未分类")) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("未分类是系统文件夹，不能删除"));
            return;
        }

        if (!storage.removeFolder(itemName)) {
            ui->statusbar->showMessage(QStringLiteral("删除保存失败"), 3000);
            return;
        }
    }

    rebuildNoteTree();
    if (itemType == FolderItem) {
        // 删除文件夹会同时移动多篇笔记，这种低频批量操作才重建搜索项
        rebuildSearchIndex();
    }
    // 当前笔记被删除后自动打开剩余的第一篇
    if (currentNoteId.isEmpty()) {
        if (!storage.notes().isEmpty()) {
            loadNote(storage.notes().first().id);
        } else {
            const QSignalBlocker titleBlocker(ui->titleEdit);
            const QSignalBlocker editorBlocker(ui->markdownEditor);
            ui->titleEdit->clear();
            ui->markdownEditor->clear();
            ui->titleEdit->setEnabled(false);
            ui->markdownEditor->setEnabled(false);
            ui->previewBrowser->setEnabled(false);
        }
    }

    ui->statusbar->showMessage(QStringLiteral("删除完成"), 2000);
}

void MainWindow::moveSelectedNote()
{
    const QModelIndex selectedIndex = ui->noteTreeView->currentIndex();
    const QString noteId = selectedIndex.data(ItemTypeRole).toInt() == NoteItem
        ? selectedIndex.data(NoteIdRole).toString() : QString();
    const Note *note = storage.findNote(noteId);
    if (note == nullptr) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择一篇笔记"));
        return;
    }

    // 移动目标包括未分类和所有用户文件夹
    QStringList choices = storage.folders();
    choices.removeAll(QStringLiteral("未分类"));
    choices.prepend(QStringLiteral("未分类"));

    const QString currentFolder = note->folder.isEmpty()
        ? QStringLiteral("未分类") : note->folder;
    bool accepted = false;
    const QString targetFolder = QInputDialog::getItem(
        this, QStringLiteral("移动笔记"), QStringLiteral("目标文件夹"),
        choices, choices.indexOf(currentFolder), false, &accepted);

    if (!accepted || targetFolder.isEmpty() || targetFolder == currentFolder) {
        return;
    }

    const QString folder = targetFolder == QStringLiteral("未分类")
        ? QString() : targetFolder;
    if (!storage.moveNote(noteId, folder)) {
        ui->statusbar->showMessage(QStringLiteral("移动保存失败"), 3000);
        return;
    }

    rebuildNoteTree();
    // 移动只改变一篇笔记的文件夹显示和搜索标题
    note = storage.findNote(noteId);
    if (note != nullptr) {
        updateSearchItem(*note, storage.readNoteContent(*note));
    }
    ui->statusbar->showMessage(QStringLiteral("笔记已移动"), 2000);
}

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
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
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , searchEdit(nullptr)
    , tagFilterCombo(nullptr)
    , noteTreeView(nullptr)
    , searchResultView(nullptr)
    , titleEdit(nullptr)
    , tagEdit(nullptr)
    , markdownEditor(nullptr)
    , previewBrowser(nullptr)
    , wordCountLabel(nullptr)
    , noteTreeModel(nullptr)
    , searchSourceModel(nullptr)
    , searchProxyModel(nullptr)
    , saveTimer(nullptr)
    , loadingNote(false)
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
    connect(markdownEditor, &QPlainTextEdit::textChanged,
            this, &MainWindow::updatePreview);
    connect(markdownEditor, &QPlainTextEdit::textChanged,
            this, &MainWindow::scheduleSave);
    connect(titleEdit, &QLineEdit::textChanged,
            this, &MainWindow::scheduleSave);
    connect(tagEdit, &QLineEdit::textChanged,
            this, &MainWindow::scheduleSave);
    connect(saveTimer, &QTimer::timeout,
            this, &MainWindow::saveCurrentNote);

    // 双击树中的笔记时将它载入编辑区
    connect(noteTreeView, &QTreeView::doubleClicked,
            this, &MainWindow::openTreeItem);
    connect(noteTreeView, &QTreeView::clicked,
            this, &MainWindow::openTreeItem);

    // 搜索文字和标签选项改变时马上刷新左侧显示
    connect(searchEdit, &QLineEdit::textChanged,
            this, &MainWindow::updateSearch);
    connect(searchResultView, &QListView::clicked,
            this, &MainWindow::openSearchResult);
    connect(searchResultView, &QListView::doubleClicked,
            this, &MainWindow::openSearchResult);
    connect(tagFilterCombo, &QComboBox::currentTextChanged,
            this, &MainWindow::filterTreeByTag);

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

    tagFilterCombo = new QComboBox(navigationWidget);
    tagFilterCombo->setToolTip(QStringLiteral("按标签筛选笔记树"));
    tagFilterCombo->addItem(QStringLiteral("全部标签"));
    navigationLayout->addWidget(tagFilterCombo);

    // 搜索结果默认隐藏，仅在输入关键字时占用空间
    searchResultView = new QListView(navigationWidget);
    searchResultView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    searchResultView->setAlternatingRowColors(true);
    searchResultView->setMaximumHeight(180);
    searchResultView->hide();
    navigationLayout->addWidget(searchResultView);

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
    noteTreeView->setContextMenuPolicy(Qt::ActionsContextMenu);
    noteTreeView->addAction(renameAction);
    noteTreeView->addAction(moveAction);
    noteTreeView->addAction(deleteAction);
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

void MainWindow::loadNotes()
{
    // 存储对象负责读取 JSON 并在首次运行时建立欢迎笔记
    if (!storage.load()) {
        ui->statusbar->showMessage(QStringLiteral("笔记数据载入失败"), 3000);
    }

    // 首次载入需要完整建立三个界面模型
    rebuildNoteTree();
    rebuildSearchIndex();
    rebuildTagChoices();

    // 默认打开第一篇笔记，避免界面显示为空
    if (!storage.notes().isEmpty()) {
        loadNote(storage.notes().first().id);
    } else {
        setEditorEnabled(false);
    }
}

void MainWindow::rebuildNoteTree()
{
    // 模型只创建一次，后续刷新时清除其中的旧节点
    if (noteTreeModel == nullptr) {
        noteTreeModel = new QStandardItemModel(this);
        noteTreeView->setModel(noteTreeModel);
    } else {
        noteTreeModel->clear();
    }

    noteTreeModel->setHorizontalHeaderLabels({QStringLiteral("笔记")});

    // 全部标签表示不进行标签过滤
    const QString selectedTag = tagFilterCombo == nullptr
        ? QStringLiteral("全部标签") : tagFilterCombo->currentText();

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

            // 选中具体标签时只显示含有该标签的笔记
            const bool tagMatches = selectedTag == QStringLiteral("全部标签")
                || selectedTag.isEmpty() || note.tags.contains(selectedTag);
            if (actualFolder == folderName && tagMatches) {
                QStandardItem *noteItem = new QStandardItem(note.title);
                noteItem->setEditable(false);
                noteItem->setData(NoteItem, ItemTypeRole);
                noteItem->setData(note.id, NoteIdRole);
                folderItem->appendRow(noteItem);
            }
        }

        noteTreeModel->appendRow(folderItem);
    }

    noteTreeView->expandAll();

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
        searchResultView->setModel(searchProxyModel);
    }

    searchProxyModel->setSourceModel(searchSourceModel);

    // 保持当前搜索框中的关键字继续生效
    searchProxyModel->setFilterFixedString(searchEdit->text().trimmed());
}

void MainWindow::rebuildTagChoices()
{
    const QString previousChoice = tagFilterCombo->currentText();
    QStringList allTags;

    // 使用 contains 去重可以保持标签第一次出现时的自然顺序
    for (const Note &note : storage.notes()) {
        for (const QString &tag : note.tags) {
            if (!tag.isEmpty() && !allTags.contains(tag)) {
                allTags.append(tag);
            }
        }
    }
    allTags.sort(Qt::CaseInsensitive);

    // 阻止刷新下拉内容时再次递归刷新笔记树
    const QSignalBlocker blocker(tagFilterCombo);
    tagFilterCombo->clear();
    tagFilterCombo->addItem(QStringLiteral("全部标签"));
    tagFilterCombo->addItems(allTags);

    const int previousIndex = tagFilterCombo->findText(previousChoice);
    tagFilterCombo->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
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

    loadingNote = true;
    currentNoteId = note->id;

    // 标题和标签来自元数据文件
    titleEdit->setText(note->title);
    tagEdit->setText(note->tags.join(QStringLiteral(", ")));

    // 存储对象按需读取这一篇笔记的 Markdown 正文
    markdownEditor->setPlainText(storage.readNoteContent(*note));

    setEditorEnabled(true);
    loadingNote = false;
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

QString MainWindow::selectedFolderName() const
{
    const QModelIndex currentIndex = noteTreeView->currentIndex();
    if (!currentIndex.isValid()) {
        return QStringLiteral("未分类");
    }

    // 选中文件夹时直接使用节点文本
    if (currentIndex.data(ItemTypeRole).toInt() == FolderItem) {
        return currentIndex.data(Qt::DisplayRole).toString();
    }

    // 选中笔记时使用它的父文件夹
    if (currentIndex.parent().isValid()) {
        return currentIndex.parent().data(Qt::DisplayRole).toString();
    }

    return QStringLiteral("未分类");
}

QString MainWindow::selectedNoteId() const
{
    const QModelIndex currentIndex = noteTreeView->currentIndex();
    if (!currentIndex.isValid()
        || currentIndex.data(ItemTypeRole).toInt() != NoteItem) {
        return QString();
    }

    return currentIndex.data(NoteIdRole).toString();
}

void MainWindow::selectNoteInTree(const QString &noteId)
{
    if (noteTreeModel == nullptr || noteId.isEmpty()) {
        return;
    }

    // 树只有文件夹和笔记两层，直接遍历比递归函数更容易理解
    for (int folderRow = 0; folderRow < noteTreeModel->rowCount(); ++folderRow) {
        QStandardItem *folderItem = noteTreeModel->item(folderRow);
        for (int noteRow = 0; noteRow < folderItem->rowCount(); ++noteRow) {
            QStandardItem *noteItem = folderItem->child(noteRow);
            if (noteItem->data(NoteIdRole).toString() == noteId) {
                noteTreeView->setCurrentIndex(noteItem->index());
                return;
            }
        }
    }
}

void MainWindow::createNote()
{
    // 在保存当前笔记前记录目标文件夹
    const QString targetFolder = selectedFolderName();
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
    Note *note = storage.addNote(title, folder);
    if (note == nullptr) {
        QMessageBox::warning(this, QStringLiteral("创建失败"),
                             QStringLiteral("无法创建笔记文件"));
        return;
    }

    // 新增数据时重建树和搜索模型，保证新条目立即可见
    const QString newNoteId = note->id;
    rebuildNoteTree();
    rebuildSearchIndex();
    rebuildTagChoices();
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

void MainWindow::scheduleSave()
{
    // 程序主动载入内容时不需要触发保存计时器
    if (loadingNote || currentNoteId.isEmpty()) {
        return;
    }

    saveTimer->start();
    ui->statusbar->showMessage(QStringLiteral("内容已修改，等待自动保存"));
}

void MainWindow::saveCurrentNote()
{
    Note *note = storage.findNote(currentNoteId);
    if (note == nullptr || loadingNote) {
        return;
    }

    saveTimer->stop();

    // 空标题自动恢复为未命名，保证树中始终有可见文字
    note->title = titleEdit->text().trimmed();
    if (note->title.isEmpty()) {
        note->title = QStringLiteral("未命名笔记");
    }

    // 英文逗号和中文逗号都可以用于分隔多个标签
    QString normalizedTags = tagEdit->text();
    normalizedTags.replace(QChar(0xFF0C), QLatin1Char(','));
    note->tags.clear();
    for (const QString &part : normalizedTags.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString tag = part.trimmed();
        if (!tag.isEmpty() && !note->tags.contains(tag)) {
            note->tags.append(tag);
        }
    }
    note->updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 正文与 JSON 的实际写入统一交给存储对象
    if (!storage.saveNoteContent(*note, markdownEditor->toPlainText())
        || !storage.saveMetadata()) {
        ui->statusbar->showMessage(QStringLiteral("正文保存失败"), 3000);
        return;
    }

    // 这一版先保持原来的模型刷新行为，后续再改成单条更新
    rebuildNoteTree();
    rebuildSearchIndex();
    rebuildTagChoices();
    ui->statusbar->showMessage(QStringLiteral("已自动保存"), 1800);
}

void MainWindow::setEditorEnabled(bool enabled)
{
    // 没有笔记时禁止输入，避免产生无处保存的内容
    titleEdit->setEnabled(enabled);
    tagEdit->setEnabled(enabled);
    markdownEditor->setEnabled(enabled);
    previewBrowser->setEnabled(enabled);
}

void MainWindow::updateSearch(const QString &keyword)
{
    if (searchProxyModel == nullptr) {
        return;
    }

    const QString trimmedKeyword = keyword.trimmed();
    searchProxyModel->setFilterFixedString(trimmedKeyword);

    // 没有关键字时隐藏结果列表，把空间还给笔记树
    searchResultView->setVisible(!trimmedKeyword.isEmpty());
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

void MainWindow::filterTreeByTag()
{
    // 标签改变后重新创建可见树节点即可
    rebuildNoteTree();
}

QString MainWindow::safeExportFileName() const
{
    QString fileName = titleEdit->text().trimmed();
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

    outputFile.write(previewBrowser->document()->toHtml().toUtf8());
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
    printer.setDocName(titleEdit->text().trimmed());

    // 直接打印预览文档可以保留标题、列表、表格等排版
    previewBrowser->document()->print(&printer);

    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("PDF 文件没有成功生成"));
        return;
    }

    ui->statusbar->showMessage(QStringLiteral("PDF 导出成功"), 3000);
}

void MainWindow::renameSelectedItem()
{
    const QModelIndex currentIndex = noteTreeView->currentIndex();
    if (!currentIndex.isValid()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择笔记或文件夹"));
        return;
    }

    const int itemType = currentIndex.data(ItemTypeRole).toInt();
    const QString oldName = currentIndex.data(Qt::DisplayRole).toString();
    bool accepted = false;
    const QString newName = QInputDialog::getText(
        this, QStringLiteral("重命名"), QStringLiteral("新的名称"),
        QLineEdit::Normal, oldName, &accepted).trimmed();

    if (!accepted || newName.isEmpty() || newName == oldName) {
        return;
    }

    if (itemType == NoteItem) {
        Note *note = storage.findNote(currentIndex.data(NoteIdRole).toString());
        if (note == nullptr) {
            return;
        }

        // 同步更新当前打开笔记的标题输入框
        note->title = newName;
        if (note->id == currentNoteId) {
            loadingNote = true;
            titleEdit->setText(newName);
            loadingNote = false;
        }

        if (!storage.saveMetadata()) {
            ui->statusbar->showMessage(QStringLiteral("重命名保存失败"), 3000);
            return;
        }
    } else if (itemType == FolderItem) {
        // 固定的未分类节点不能重命名
        if (oldName == QStringLiteral("未分类")) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("未分类是系统文件夹，不能重命名"));
            return;
        }

        if (storage.folders().contains(newName)
            || newName == QStringLiteral("未分类")) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("这个文件夹已经存在"));
            return;
        }

        if (!storage.renameFolder(oldName, newName)) {
            ui->statusbar->showMessage(QStringLiteral("重命名保存失败"), 3000);
            return;
        }
    }

    rebuildNoteTree();
    rebuildSearchIndex();
    ui->statusbar->showMessage(QStringLiteral("重命名完成"), 2000);
}

void MainWindow::deleteSelectedItem()
{
    const QModelIndex currentIndex = noteTreeView->currentIndex();
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
    rebuildSearchIndex();
    rebuildTagChoices();

    // 当前笔记被删除后自动打开剩余的第一篇
    if (currentNoteId.isEmpty()) {
        if (!storage.notes().isEmpty()) {
            loadNote(storage.notes().first().id);
        } else {
            loadingNote = true;
            titleEdit->clear();
            tagEdit->clear();
            markdownEditor->clear();
            loadingNote = false;
            setEditorEnabled(false);
        }
    }

    ui->statusbar->showMessage(QStringLiteral("删除完成"), 2000);
}

void MainWindow::moveSelectedNote()
{
    const QString noteId = selectedNoteId();
    Note *note = storage.findNote(noteId);
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

    note->folder = targetFolder == QStringLiteral("未分类")
        ? QString() : targetFolder;
    if (!storage.saveMetadata()) {
        ui->statusbar->showMessage(QStringLiteral("移动保存失败"), 3000);
        return;
    }

    rebuildNoteTree();
    rebuildSearchIndex();
    ui->statusbar->showMessage(QStringLiteral("笔记已移动"), 2000);
}

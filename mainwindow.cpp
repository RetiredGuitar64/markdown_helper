#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFormLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QUuid>
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

QString MainWindow::dataDirectoryPath() const
{
    // 使用系统推荐的应用数据目录，避免把用户笔记写入程序目录
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    // 极少数系统无法提供目录时使用用户主目录下的备用路径
    if (path.isEmpty()) {
        path = QDir::homePath() + QStringLiteral("/.markdown_helper");
    }

    return path;
}

QString MainWindow::noteFilePath(const QString &fileName) const
{
    // 所有 Markdown 正文统一放在 notes 子目录中
    return QDir(dataDirectoryPath()).filePath(QStringLiteral("notes/") + fileName);
}

void MainWindow::loadNotes()
{
    // 保证数据目录和正文目录在读取前已经存在
    QDir dataDirectory(dataDirectoryPath());
    dataDirectory.mkpath(QStringLiteral("notes"));

    QFile metadataFile(dataDirectory.filePath(QStringLiteral("notes.json")));
    if (metadataFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // 解析保存文件中的根 JSON 对象
        const QJsonDocument document = QJsonDocument::fromJson(metadataFile.readAll());
        const QJsonObject rootObject = document.object();

        // 读取文件夹数组并忽略空名称
        const QJsonArray folderArray = rootObject.value(QStringLiteral("folders")).toArray();
        for (const QJsonValue &value : folderArray) {
            const QString folderName = value.toString().trimmed();
            if (!folderName.isEmpty() && !folders.contains(folderName)) {
                folders.append(folderName);
            }
        }

        // 读取每篇笔记的元数据，正文稍后按需载入
        const QJsonArray noteArray = rootObject.value(QStringLiteral("notes")).toArray();
        for (const QJsonValue &value : noteArray) {
            const QJsonObject object = value.toObject();
            NoteRecord note;
            note.id = object.value(QStringLiteral("id")).toString();
            note.title = object.value(QStringLiteral("title")).toString();
            note.folder = object.value(QStringLiteral("folder")).toString();
            note.fileName = object.value(QStringLiteral("fileName")).toString();
            note.updatedAt = object.value(QStringLiteral("updatedAt")).toString();

            const QJsonArray tagArray = object.value(QStringLiteral("tags")).toArray();
            for (const QJsonValue &tagValue : tagArray) {
                note.tags.append(tagValue.toString());
            }

            // 缺少编号或文件名的记录无法使用，因此不加入列表
            if (!note.id.isEmpty() && !note.fileName.isEmpty()) {
                notes.append(note);
            }
        }
    }

    // 第一次运行时自动创建示例笔记
    if (notes.isEmpty()) {
        createWelcomeNote();
    }

    rebuildNoteTree();

    // 默认打开第一篇笔记，避免界面显示为空
    if (!notes.isEmpty()) {
        loadNote(notes.first().id);
    } else {
        setEditorEnabled(false);
    }
}

void MainWindow::saveMetadata()
{
    // 把文件夹列表转换成 JSON 数组
    QJsonArray folderArray;
    for (const QString &folder : std::as_const(folders)) {
        folderArray.append(folder);
    }

    // 把每篇笔记的轻量信息写入 JSON 数组
    QJsonArray noteArray;
    for (const NoteRecord &note : std::as_const(notes)) {
        QJsonArray tagArray;
        for (const QString &tag : note.tags) {
            tagArray.append(tag);
        }

        QJsonObject object;
        object.insert(QStringLiteral("id"), note.id);
        object.insert(QStringLiteral("title"), note.title);
        object.insert(QStringLiteral("folder"), note.folder);
        object.insert(QStringLiteral("tags"), tagArray);
        object.insert(QStringLiteral("fileName"), note.fileName);
        object.insert(QStringLiteral("updatedAt"), note.updatedAt);
        noteArray.append(object);
    }

    QJsonObject rootObject;
    rootObject.insert(QStringLiteral("folders"), folderArray);
    rootObject.insert(QStringLiteral("notes"), noteArray);

    // QSaveFile 先写临时文件，成功后再替换旧文件
    QSaveFile metadataFile(QDir(dataDirectoryPath()).filePath(QStringLiteral("notes.json")));
    if (!metadataFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ui->statusbar->showMessage(QStringLiteral("元数据保存失败"), 3000);
        return;
    }

    metadataFile.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
    metadataFile.commit();
}

void MainWindow::createWelcomeNote()
{
    // 欢迎笔记也使用普通的数据结构，用户可以自由编辑或删除
    NoteRecord note;
    note.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    note.title = QStringLiteral("欢迎使用");
    note.folder = QStringLiteral("入门");
    note.tags = {QStringLiteral("示例"), QStringLiteral("Markdown")};
    note.fileName = note.id + QStringLiteral(".md");
    note.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    folders.append(note.folder);
    notes.append(note);

    // 写入覆盖常用语法的初始 Markdown 内容
    QSaveFile contentFile(noteFilePath(note.fileName));
    if (contentFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        const QString welcomeText = QStringLiteral(
            "# 欢迎使用 Markdown 笔记\n\n"
            "在中间输入 **Markdown**，右侧会实时显示效果\n\n"
            "## 常用内容\n\n"
            "- 新建和管理笔记\n"
            "- 使用文件夹与标签分类\n"
            "- 搜索标题和正文\n\n"
            "| 功能 | 状态 |\n"
            "| --- | --- |\n"
            "| 实时预览 | 可用 |\n"
            "| 本地保存 | 可用 |\n\n"
            "```cpp\n"
            "qDebug() << \"Hello Markdown\";\n"
            "```\n");
        contentFile.write(welcomeText.toUtf8());
        contentFile.commit();
    }

    saveMetadata();
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
    QStringList visibleFolders = folders;
    if (!visibleFolders.contains(QStringLiteral("未分类"))) {
        visibleFolders.prepend(QStringLiteral("未分类"));
    }

    for (const QString &folderName : std::as_const(visibleFolders)) {
        QStandardItem *folderItem = new QStandardItem(folderName);
        folderItem->setEditable(false);
        folderItem->setData(FolderItem, ItemTypeRole);

        // 将属于当前文件夹的笔记依次添加为子节点
        for (const NoteRecord &note : std::as_const(notes)) {
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

    // 树刷新通常表示数据发生变化，搜索和标签也需要同步
    rebuildSearchIndex();
    rebuildTagChoices();
}

void MainWindow::rebuildSearchIndex()
{
    // 搜索数据源为每篇笔记保存一个扁平条目
    if (searchSourceModel == nullptr) {
        searchSourceModel = new QStandardItemModel(this);
    } else {
        searchSourceModel->clear();
    }

    for (const NoteRecord &note : std::as_const(notes)) {
        QFile contentFile(noteFilePath(note.fileName));
        QString content;
        if (contentFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            content = QString::fromUtf8(contentFile.readAll());
        }

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
    for (const NoteRecord &note : std::as_const(notes)) {
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

int MainWindow::findNoteIndex(const QString &noteId) const
{
    // 目前数据规模较小，线性遍历简单并且足够快速
    for (int index = 0; index < notes.size(); ++index) {
        if (notes.at(index).id == noteId) {
            return index;
        }
    }

    return -1;
}

void MainWindow::loadNote(const QString &noteId)
{
    const int noteIndex = findNoteIndex(noteId);
    if (noteIndex < 0) {
        return;
    }

    // 切换笔记前先保存上一篇正在编辑的内容
    if (!currentNoteId.isEmpty() && currentNoteId != noteId) {
        saveCurrentNote();
    }

    loadingNote = true;
    const NoteRecord &note = notes.at(noteIndex);
    currentNoteId = note.id;

    // 标题和标签来自元数据文件
    titleEdit->setText(note.title);
    tagEdit->setText(note.tags.join(QStringLiteral(", ")));

    // 正文来自单独的 Markdown 文件
    QFile contentFile(noteFilePath(note.fileName));
    if (contentFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        markdownEditor->setPlainText(QString::fromUtf8(contentFile.readAll()));
    } else {
        markdownEditor->clear();
    }

    setEditorEnabled(true);
    loadingNote = false;
    updatePreview();
    ui->statusbar->showMessage(QStringLiteral("已打开 %1").arg(note.title), 2000);
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

void MainWindow::createNote()
{
    bool accepted = false;
    const QString title = QInputDialog::getText(
        this, QStringLiteral("新建笔记"), QStringLiteral("笔记标题"),
        QLineEdit::Normal, QStringLiteral("未命名笔记"), &accepted).trimmed();

    // 用户取消或没有输入标题时不创建数据
    if (!accepted || title.isEmpty()) {
        return;
    }

    saveCurrentNote();

    NoteRecord note;
    note.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    note.title = title;
    note.folder = selectedFolderName();
    if (note.folder == QStringLiteral("未分类")) {
        note.folder.clear();
    }
    note.fileName = note.id + QStringLiteral(".md");
    note.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    notes.append(note);

    // 新笔记先创建一个空的正文文件
    QSaveFile contentFile(noteFilePath(note.fileName));
    if (contentFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        contentFile.write(QByteArray());
        contentFile.commit();
    }

    saveMetadata();
    rebuildNoteTree();
    loadNote(note.id);
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
    if (folders.contains(folderName) || folderName == QStringLiteral("未分类")) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("这个文件夹已经存在"));
        return;
    }

    folders.append(folderName);
    saveMetadata();
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
    const int noteIndex = findNoteIndex(currentNoteId);
    if (noteIndex < 0 || loadingNote) {
        return;
    }

    saveTimer->stop();
    NoteRecord &note = notes[noteIndex];

    // 空标题自动恢复为未命名，保证树中始终有可见文字
    note.title = titleEdit->text().trimmed();
    if (note.title.isEmpty()) {
        note.title = QStringLiteral("未命名笔记");
    }

    // 英文逗号和中文逗号都可以用于分隔多个标签
    QString normalizedTags = tagEdit->text();
    normalizedTags.replace(QChar(0xFF0C), QLatin1Char(','));
    note.tags.clear();
    for (const QString &part : normalizedTags.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString tag = part.trimmed();
        if (!tag.isEmpty() && !note.tags.contains(tag)) {
            note.tags.append(tag);
        }
    }
    note.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 使用安全写入方式保存当前 Markdown 正文
    QSaveFile contentFile(noteFilePath(note.fileName));
    if (!contentFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ui->statusbar->showMessage(QStringLiteral("正文保存失败"), 3000);
        return;
    }
    contentFile.write(markdownEditor->toPlainText().toUtf8());
    if (!contentFile.commit()) {
        ui->statusbar->showMessage(QStringLiteral("正文保存失败"), 3000);
        return;
    }

    saveMetadata();
    rebuildNoteTree();
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
        const int noteIndex = findNoteIndex(currentIndex.data(NoteIdRole).toString());
        if (noteIndex < 0) {
            return;
        }

        // 同步更新当前打开笔记的标题输入框
        notes[noteIndex].title = newName;
        if (notes.at(noteIndex).id == currentNoteId) {
            loadingNote = true;
            titleEdit->setText(newName);
            loadingNote = false;
        }
    } else if (itemType == FolderItem) {
        // 固定的未分类节点不能重命名
        if (oldName == QStringLiteral("未分类")) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("未分类是系统文件夹，不能重命名"));
            return;
        }

        if (folders.contains(newName) || newName == QStringLiteral("未分类")) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("这个文件夹已经存在"));
            return;
        }

        // 更新文件夹列表以及其中所有笔记的归属信息
        const int folderIndex = folders.indexOf(oldName);
        if (folderIndex >= 0) {
            folders[folderIndex] = newName;
        }
        for (NoteRecord &note : notes) {
            if (note.folder == oldName) {
                note.folder = newName;
            }
        }
    }

    saveMetadata();
    rebuildNoteTree();
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
        const int noteIndex = findNoteIndex(noteId);
        if (noteIndex < 0) {
            return;
        }

        // 删除前保存其他正在编辑的笔记
        if (currentNoteId != noteId) {
            saveCurrentNote();
        }

        QFile::remove(noteFilePath(notes.at(noteIndex).fileName));
        notes.removeAt(noteIndex);

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

        // 删除文件夹时保留其中的笔记并转移到未分类
        for (NoteRecord &note : notes) {
            if (note.folder == itemName) {
                note.folder.clear();
            }
        }
        folders.removeAll(itemName);
    }

    saveMetadata();
    rebuildNoteTree();

    // 当前笔记被删除后自动打开剩余的第一篇
    if (currentNoteId.isEmpty()) {
        if (!notes.isEmpty()) {
            loadNote(notes.first().id);
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
    const int noteIndex = findNoteIndex(noteId);
    if (noteIndex < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择一篇笔记"));
        return;
    }

    // 移动目标包括未分类和所有用户文件夹
    QStringList choices = folders;
    choices.removeAll(QStringLiteral("未分类"));
    choices.prepend(QStringLiteral("未分类"));

    const QString currentFolder = notes.at(noteIndex).folder.isEmpty()
        ? QStringLiteral("未分类") : notes.at(noteIndex).folder;
    bool accepted = false;
    const QString targetFolder = QInputDialog::getItem(
        this, QStringLiteral("移动笔记"), QStringLiteral("目标文件夹"),
        choices, choices.indexOf(currentFolder), false, &accepted);

    if (!accepted || targetFolder.isEmpty() || targetFolder == currentFolder) {
        return;
    }

    notes[noteIndex].folder = targetFolder == QStringLiteral("未分类")
        ? QString() : targetFolder;
    saveMetadata();
    rebuildNoteTree();
    ui->statusbar->showMessage(QStringLiteral("笔记已移动"), 2000);
}

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTimer>
#include <QTreeView>

void MainWindow::loadNotes()
{
    // 存储对象负责读取 JSON 并在首次运行时建立欢迎笔记
    if (!storage.load()) {
        ui->statusbar->showMessage(QStringLiteral("笔记数据载入失败"), 3000);
    }

    // 首次载入需要建立笔记树和搜索列表两个模型
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

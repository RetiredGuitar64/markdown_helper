#include "notestorage.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

bool NoteStorage::load()
{
    // 重复载入时先清空旧数据，避免产生重复记录
    noteList.clear();
    folderList.clear();

    // 在读取前保证数据目录和正文目录存在
    QDir dataDirectory(dataDirectoryPath());
    if (!dataDirectory.mkpath(QStringLiteral("notes"))) {
        return false;
    }

    QFile metadataFile(dataDirectory.filePath(QStringLiteral("notes.json")));
    if (metadataFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // 解析保存文件中的根 JSON 对象
        const QJsonDocument document = QJsonDocument::fromJson(metadataFile.readAll());
        const QJsonObject rootObject = document.object();

        // 读取文件夹数组并忽略空名称和重复名称
        const QJsonArray folderArray = rootObject.value(QStringLiteral("folders")).toArray();
        for (const QJsonValue &value : folderArray) {
            const QString folderName = value.toString().trimmed();
            if (!folderName.isEmpty() && !folderList.contains(folderName)) {
                folderList.append(folderName);
            }
        }

        // 读取笔记元数据，正文在打开或搜索时再读取
        const QJsonArray noteArray = rootObject.value(QStringLiteral("notes")).toArray();
        for (const QJsonValue &value : noteArray) {
            const QJsonObject object = value.toObject();
            Note note;
            note.id = object.value(QStringLiteral("id")).toString();
            note.title = object.value(QStringLiteral("title")).toString();
            note.folder = object.value(QStringLiteral("folder")).toString();
            note.fileName = object.value(QStringLiteral("fileName")).toString();
            note.updatedAt = object.value(QStringLiteral("updatedAt")).toString();

            const QJsonArray tagArray = object.value(QStringLiteral("tags")).toArray();
            for (const QJsonValue &tagValue : tagArray) {
                note.tags.append(tagValue.toString());
            }

            // 缺少编号或正文文件名的记录不能正常使用
            if (!note.id.isEmpty() && !note.fileName.isEmpty()) {
                noteList.append(note);
            }
        }
    }

    // 没有可用笔记时创建首次运行示例
    if (noteList.isEmpty()) {
        return createWelcomeNote();
    }

    return true;
}

bool NoteStorage::saveMetadata() const
{
    // 把文件夹列表转换成 JSON 数组
    QJsonArray folderArray;
    for (const QString &folder : folderList) {
        folderArray.append(folder);
    }

    // 把每篇笔记的轻量信息写入 JSON 数组
    QJsonArray noteArray;
    for (const Note &note : noteList) {
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

    // QSaveFile 可以避免程序中断时留下只写了一半的 JSON
    QSaveFile metadataFile(
        QDir(dataDirectoryPath()).filePath(QStringLiteral("notes.json")));
    if (!metadataFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    metadataFile.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
    return metadataFile.commit();
}

const QVector<Note> &NoteStorage::notes() const
{
    return noteList;
}

const QStringList &NoteStorage::folders() const
{
    return folderList;
}

Note *NoteStorage::findNote(const QString &noteId)
{
    // 当前项目数据量不大，简单线性查找已经足够
    for (Note &note : noteList) {
        if (note.id == noteId) {
            return &note;
        }
    }

    return nullptr;
}

const Note *NoteStorage::findNote(const QString &noteId) const
{
    // const 重载用于不需要修改笔记的界面操作
    for (const Note &note : noteList) {
        if (note.id == noteId) {
            return &note;
        }
    }

    return nullptr;
}

Note *NoteStorage::addNote(const QString &title, const QString &folder)
{
    Note note;
    note.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    note.title = title;
    note.folder = folder;
    note.fileName = note.id + QStringLiteral(".md");
    note.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    noteList.append(note);

    // 新笔记先创建一个空正文文件
    Note &createdNote = noteList.last();
    if (!saveNoteContent(createdNote, QString()) || !saveMetadata()) {
        deleteNoteContent(createdNote);
        noteList.removeLast();
        return nullptr;
    }

    return &createdNote;
}

bool NoteStorage::removeNote(const QString &noteId)
{
    for (int index = 0; index < noteList.size(); ++index) {
        if (noteList.at(index).id == noteId) {
            // 正文不存在时也允许移除已经失效的元数据
            deleteNoteContent(noteList.at(index));
            noteList.removeAt(index);
            return saveMetadata();
        }
    }

    return false;
}

bool NoteStorage::addFolder(const QString &folderName)
{
    const QString trimmedName = folderName.trimmed();
    if (trimmedName.isEmpty() || trimmedName == QStringLiteral("未分类")
        || folderList.contains(trimmedName)) {
        return false;
    }

    folderList.append(trimmedName);
    return saveMetadata();
}

bool NoteStorage::renameFolder(const QString &oldName, const QString &newName)
{
    const int folderIndex = folderList.indexOf(oldName);
    if (folderIndex < 0 || newName.trimmed().isEmpty()
        || newName == QStringLiteral("未分类") || folderList.contains(newName)) {
        return false;
    }

    // 文件夹改名时同步更新所有所属笔记
    folderList[folderIndex] = newName;
    for (Note &note : noteList) {
        if (note.folder == oldName) {
            note.folder = newName;
        }
    }

    return saveMetadata();
}

bool NoteStorage::removeFolder(const QString &folderName)
{
    if (!folderList.contains(folderName)) {
        return false;
    }

    // 删除文件夹时保留正文并把笔记移到未分类
    for (Note &note : noteList) {
        if (note.folder == folderName) {
            note.folder.clear();
        }
    }
    folderList.removeAll(folderName);
    return saveMetadata();
}

QString NoteStorage::readNoteContent(const Note &note) const
{
    QFile contentFile(noteFilePath(note.fileName));
    if (!contentFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    return QString::fromUtf8(contentFile.readAll());
}

bool NoteStorage::saveNoteContent(const Note &note, const QString &content) const
{
    // 正文同样使用 QSaveFile 保证替换过程安全
    QSaveFile contentFile(noteFilePath(note.fileName));
    if (!contentFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    contentFile.write(content.toUtf8());
    return contentFile.commit();
}

bool NoteStorage::deleteNoteContent(const Note &note) const
{
    const QString filePath = noteFilePath(note.fileName);

    // 文件本来就不存在时也视为删除完成
    return !QFile::exists(filePath) || QFile::remove(filePath);
}

QString NoteStorage::dataDirectoryPath() const
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    // 极少数系统无法提供目录时使用用户主目录下的备用路径
    if (path.isEmpty()) {
        path = QDir::homePath() + QStringLiteral("/.markdown_helper");
    }

    return path;
}

QString NoteStorage::noteFilePath(const QString &fileName) const
{
    return QDir(dataDirectoryPath()).filePath(QStringLiteral("notes/") + fileName);
}

bool NoteStorage::createWelcomeNote()
{
    Note note;
    note.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    note.title = QStringLiteral("欢迎使用");
    note.folder = QStringLiteral("入门");
    note.tags = {QStringLiteral("示例"), QStringLiteral("Markdown")};
    note.fileName = note.id + QStringLiteral(".md");
    note.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    folderList.append(note.folder);
    noteList.append(note);

    // 示例正文覆盖标题、列表、表格和代码块
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

    if (!saveNoteContent(noteList.last(), welcomeText)) {
        noteList.clear();
        folderList.clear();
        return false;
    }

    return saveMetadata();
}

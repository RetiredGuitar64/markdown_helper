#ifndef NOTESTORAGE_H
#define NOTESTORAGE_H

#include "note.h"

#include <QStringList>
#include <QVector>

// 集中管理笔记数据和本地文件读写
class NoteStorage
{
public:
    // 从 notes.json 载入文件夹和笔记元数据
    bool load();

    // 将当前元数据保存到 notes.json
    bool saveMetadata() const;

    // 返回全部笔记供界面模型读取
    const QVector<Note> &notes() const;

    // 返回全部文件夹供界面模型读取
    const QStringList &folders() const;

    // 根据编号查找只读笔记
    const Note *findNote(const QString &noteId) const;

    // 创建笔记并同时生成空 Markdown 文件
    const Note *addNote(const QString &title, const QString &folder);

    // 更新一篇笔记的标题和正文
    bool updateNote(const QString &noteId, const QString &title,
                    const QString &content);

    // 修改一篇笔记的标题
    bool renameNote(const QString &noteId, const QString &newTitle);

    // 修改一篇笔记所属的文件夹
    bool moveNote(const QString &noteId, const QString &folder);

    // 删除笔记元数据和对应的 Markdown 文件
    bool removeNote(const QString &noteId);

    // 添加一个不重名的文件夹
    bool addFolder(const QString &folderName);

    // 重命名文件夹并更新其中笔记的归属
    bool renameFolder(const QString &oldName, const QString &newName);

    // 删除文件夹并将其中笔记移动到未分类
    bool removeFolder(const QString &folderName);

    // 读取指定笔记的 Markdown 正文
    QString readNoteContent(const Note &note) const;

    // 安全写入指定笔记的 Markdown 正文
    bool saveNoteContent(const Note &note, const QString &content) const;

    // 删除指定笔记对应的 Markdown 文件
    bool deleteNoteContent(const Note &note) const;

    // 返回系统推荐的应用数据目录
    QString dataDirectoryPath() const;

    // 返回指定 Markdown 文件的完整路径
    QString noteFilePath(const QString &fileName) const;

private:
    // 首次运行时创建一篇普通的欢迎笔记
    bool createWelcomeNote();

    // 内存中的全部笔记元数据
    QVector<Note> noteList;

    // 内存中的全部文件夹名称
    QStringList folderList;
};

#endif // NOTESTORAGE_H

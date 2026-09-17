#ifndef NOTE_H
#define NOTE_H

#include <QString>
#include <QStringList>

// 保存一篇笔记的轻量元数据
struct Note
{
    // 笔记的唯一编号
    QString id;

    // 显示在笔记树中的标题
    QString title;

    // 笔记所属的文件夹，空字符串表示未分类
    QString folder;

    // 一篇笔记可以拥有多个标签
    QStringList tags;

    // 正文对应的 Markdown 文件名
    QString fileName;

    // 最后一次保存时间
    QString updatedAt;
};

#endif // NOTE_H

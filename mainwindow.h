#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QVector>

class QCloseEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QStandardItem;
class QStandardItemModel;
class QTextBrowser;
class QTimer;
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

protected:
    // 关闭窗口前保存尚未写入磁盘的内容
    void closeEvent(QCloseEvent *event) override;

private slots:
    // 根据编辑区中的 Markdown 文本刷新右侧预览
    void updatePreview();

    // 创建一篇新笔记并放入当前选择的文件夹
    void createNote();

    // 创建一个新的笔记文件夹
    void createFolder();

    // 打开笔记树中双击或选中的笔记
    void openTreeItem(const QModelIndex &index);

    // 延迟保存编辑内容，避免每次按键都访问磁盘
    void scheduleSave();

    // 将当前编辑的笔记写入 Markdown 文件
    void saveCurrentNote();

    // 重命名当前选中的笔记或文件夹
    void renameSelectedItem();

    // 删除当前选中的笔记或文件夹
    void deleteSelectedItem();

    // 将当前选中的笔记移动到其他文件夹
    void moveSelectedNote();

private:
    // 一篇笔记在内存中的基本信息
    struct NoteRecord
    {
        QString id;
        QString title;
        QString folder;
        QStringList tags;
        QString fileName;
        QString updatedAt;
    };

    // 自定义数据角色用于区分文件夹和笔记
    enum ItemRole
    {
        ItemTypeRole = Qt::UserRole + 1,
        NoteIdRole,
        SearchTextRole
    };

    // 树节点的类型值
    enum ItemType
    {
        FolderItem = 1,
        NoteItem = 2
    };

    // 创建主窗口中需要的控件和布局
    void setupInterface();

    // 设置简单统一的界面样式
    void setupStyle();

    // 创建菜单和工具栏中的常用操作
    void setupActions();

    // 从本地 JSON 和 Markdown 文件读取全部笔记
    void loadNotes();

    // 将文件夹和笔记元数据保存为 JSON
    void saveMetadata();

    // 根据内存数据重新生成左侧树形列表
    void rebuildNoteTree();

    // 在编辑区载入指定编号的笔记
    void loadNote(const QString &noteId);

    // 创建首次运行时显示的欢迎笔记
    void createWelcomeNote();

    // 返回应用保存数据的文件夹路径
    QString dataDirectoryPath() const;

    // 返回指定 Markdown 文件的完整路径
    QString noteFilePath(const QString &fileName) const;

    // 根据编号查找笔记在数组中的位置
    int findNoteIndex(const QString &noteId) const;

    // 获取当前树节点代表的文件夹名称
    QString selectedFolderName() const;

    // 获取树中当前选中笔记的编号
    QString selectedNoteId() const;

    // 控制编辑控件在有无笔记时的可用状态
    void setEditorEnabled(bool enabled);

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

    // 管理左侧文件夹和笔记节点的数据模型
    QStandardItemModel *noteTreeModel;

    // 延迟自动保存使用的单次计时器
    QTimer *saveTimer;

    // 保存全部笔记的轻量数据数组
    QVector<NoteRecord> notes;

    // 保存用户创建的文件夹名称
    QStringList folders;

    // 当前正在编辑的笔记编号
    QString currentNoteId;

    // 载入笔记时阻止输入信号触发自动保存
    bool loadingNote;
};
#endif // MAINWINDOW_H

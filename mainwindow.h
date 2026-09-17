#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "notestorage.h"

#include <QMainWindow>

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QPlainTextEdit;
class QSortFilterProxyModel;
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

    // 根据搜索框内容过滤全文搜索结果
    void updateSearch(const QString &keyword);

    // 打开全文搜索结果中选中的笔记
    void openSearchResult(const QModelIndex &index);

    // 根据下拉框选择的标签过滤笔记树
    void filterTreeByTag();

    // 将当前笔记导出为可以在浏览器打开的 HTML 文件
    void exportCurrentNoteAsHtml();

    // 将当前笔记通过打印模块导出为 PDF 文件
    void exportCurrentNoteAsPdf();

private:
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

    // 根据内存数据重新生成左侧树形列表
    void rebuildNoteTree();

    // 建立包含标题、正文和标签的简单搜索数据源
    void rebuildSearchIndex();

    // 收集全部标签并刷新标签筛选下拉框
    void rebuildTagChoices();

    // 在编辑区载入指定编号的笔记
    void loadNote(const QString &noteId);

    // 获取当前树节点代表的文件夹名称
    QString selectedFolderName() const;

    // 获取树中当前选中笔记的编号
    QString selectedNoteId() const;

    // 在树刷新后重新选中当前打开的笔记
    void selectNoteInTree(const QString &noteId);

    // 控制编辑控件在有无笔记时的可用状态
    void setEditorEnabled(bool enabled);

    // 生成适合作为导出文件名的默认标题
    QString safeExportFileName() const;

    // Qt Designer 生成的界面对象
    Ui::MainWindow *ui;

    // 左侧用于查找笔记的输入框
    QLineEdit *searchEdit;

    // 标签下拉框用于按一个标签筛选笔记树
    QComboBox *tagFilterCombo;

    // 左侧用于显示文件夹和笔记的树
    QTreeView *noteTreeView;

    // 搜索框有内容时显示匹配的笔记列表
    QListView *searchResultView;

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

    // 全文搜索使用的原始扁平模型
    QStandardItemModel *searchSourceModel;

    // 代理模型负责根据搜索关键字过滤原始模型
    QSortFilterProxyModel *searchProxyModel;

    // 延迟自动保存使用的单次计时器
    QTimer *saveTimer;

    // 负责笔记元数据和 Markdown 文件的本地存储
    NoteStorage storage;

    // 当前正在编辑的笔记编号
    QString currentNoteId;

    // 载入笔记时阻止输入信号触发自动保存
    bool loadingNote;
};
#endif // MAINWINDOW_H

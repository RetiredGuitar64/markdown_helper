# Markdown 笔记管理器

这是一个使用 Qt 6 Widgets 和 C++ 编写的本地 Markdown 笔记管理器，适合作为 Qt 课程项目直接运行和演示

## 已实现功能

- Markdown 源码编辑和实时预览
- 新建、重命名、删除和移动笔记
- 新建、重命名和删除文件夹
- 为一篇笔记设置多个标签，并按标签筛选
- 对标题、正文和标签进行不区分大小写的全文搜索
- 自动保存 Markdown 正文和 JSON 元数据
- 导出当前笔记为 HTML 或 PDF

## 在 Qt Creator 中运行

1. 使用 Qt Creator 打开项目根目录的 `CMakeLists.txt`
2. 选择带有 Qt 6.5 或更高版本的 Desktop Kit
3. 等待 CMake 配置完成
4. 单击左下角运行按钮或按 `Ctrl+R`

项目依赖以下 Qt 模块

- Core
- Widgets
- PrintSupport

## 命令行构建

```bash
cmake -S . -B build
cmake --build build
./build/markdown_helper
```

## 数据保存位置

程序使用 `QStandardPaths::AppDataLocation` 获取当前系统推荐的应用数据目录，首次运行时会自动创建下面的内容

```text
应用数据目录/
├── notes.json
└── notes/
    └── 笔记编号.md
```

`notes.json` 保存标题、文件夹、标签和更新时间，`notes` 目录中的 Markdown 文件保存正文，所有数据均保存在本机

## 基本操作

- 单击左侧笔记即可打开，修改后约 600 毫秒自动保存
- 在文件夹或笔记上单击右键可以重命名、移动或删除
- 标签输入框使用英文逗号或中文逗号分隔多个标签
- 搜索框输入内容后会显示标题、正文或标签中匹配的笔记
- 文件菜单中可以将当前笔记导出为 HTML 或 PDF


我把你这次上传的项目完整看了一遍。现在这个项目已经很适合从“课程项目”的角度理解了：没有 Repository / Service / Controller 这些层，真正核心只有 **一个主窗口类 `MainWindow` + 一个存储类 `NoteStorage` + 一个纯数据结构 `Note`**。

最准确地说，现在它是一个“**两层半结构**”：

```text
                ┌──────────────────────┐
                │       main.cpp       │
                │     程序启动入口      │
                └──────────┬───────────┘
                           │ 创建
                           ▼
              ┌─────────────────────────┐
              │       MainWindow        │
              │                         │
              │ mainwindow.cpp          │
              │ UI / 预览 / 导出 / 菜单 │
              │                         │
              │ mainwindow_notes.cpp    │
              │ 笔记 / 文件夹 / 搜索     │
              └───────┬─────────┬───────┘
                      │         │
                 使用 │         │ 操作 Qt Model
                      ▼         ▼
             ┌─────────────┐   QStandardItemModel
             │ NoteStorage │   QSortFilterProxyModel
             └──────┬──────┘          │
                    │                 ▼
                    │            QTreeView / QListView
                    │
                    ▼
                 Note
                    │
          ┌─────────┴─────────┐
          ▼                   ▼
      notes.json         notes/*.md
      元数据                正文
```

其中 `mainwindow.cpp` 和 `mainwindow_notes.cpp` **不是两个类**，这一点很重要。它们只是把同一个 `MainWindow` 类的成员函数拆到了两个 `.cpp` 里，目的是避免一个 `mainwindow.cpp` 接近 1000 行。

---

# 一、现在整个项目有哪些文件

现在一共 9 个项目文件：

| 文件                     |  行数 | 作用                     |
| ---------------------- | --: | ---------------------- |
| `CMakeLists.txt`       |  29 | Qt/CMake 构建配置          |
| `main.cpp`             |  20 | 程序入口                   |
| `mainwindow.ui`        | 129 | Qt Designer 主界面布局      |
| `mainwindow.h`         | 140 | `MainWindow` 类的完整声明    |
| `mainwindow.cpp`       | 290 | 主窗口初始化、预览、菜单、导出        |
| `mainwindow_notes.cpp` | 506 | 笔记、文件夹、树、搜索等交互         |
| `note.h`               |  29 | `Note` 数据结构            |
| `notestorage.h`        |  73 | `NoteStorage` 接口       |
| `notestorage.cpp`      | 315 | JSON 和 Markdown 文件实际读写 |

总代码大约 **1531 行**，而原来的 `mainwindow.cpp` 本身就有 991 行。

现在最大的文件已经变成了 `mainwindow_notes.cpp`，506 行；但这是有意的，因为“笔记列表、文件夹、搜索”本身就是项目里逻辑最多的一块。

---

# 二、`CMakeLists.txt`：整个项目是怎么被编译起来的

现在它非常简单：

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets PrintSupport)
```

只有三个 Qt 模块：

```text
Qt::Core
    基础类型、文件、JSON、时间、UUID、模型等

Qt::Widgets
    QMainWindow、QTreeView、QLineEdit、QTextBrowser 等界面

Qt::PrintSupport
    QPrinter，用于导出 PDF
```

然后：

```cmake
qt_standard_project_setup()
```

这里实际上非常重要。Qt 官方说明，这个命令会默认启用 `CMAKE_AUTOMOC` 和 `CMAKE_AUTOUIC`。所以你项目中有 `Q_OBJECT` 时不用自己运行 `moc`，而 `mainwindow.ui` 也会自动经过 `uic` 转换。([Qt 文档][1])

因此项目中虽然没有：

```text
ui_mainwindow.h
```

这个文件，但你会看到：

```cpp
#include "ui_mainwindow.h"
```

并不会报错。

它实际上是在**编译过程中自动生成的**：

```text
mainwindow.ui
      │
      │ Qt uic
      ▼
ui_mainwindow.h
```

这也是为什么：

```cpp
ui->setupUi(this);
```

能够工作。

---

# 三、`main.cpp`：程序真正从这里开始

这个文件现在几乎已经不能再简化了。

核心流程就是：

```cpp
QApplication a(argc, argv);

QApplication::setApplicationName("markdown_helper");
QApplication::setApplicationDisplayName("Markdown 笔记管理器");

MainWindow w;
w.show();

return QApplication::exec();
```

运行关系是：

```text
main()
  │
  ├─ 创建 QApplication
  │
  ├─ 设置应用名称
  │
  ├─ 创建 MainWindow
  │       │
  │       └─ MainWindow 构造函数开始初始化整个程序
  │
  ├─ show()
  │
  └─ QApplication::exec()
          │
          └─ Qt 事件循环
```

其中：

```cpp
setApplicationName("markdown_helper");
```

不只是窗口名称，它也会影响 `QStandardPaths::AppDataLocation`。

你现在在 Arch/Linux 上运行时，数据目录通常会落到类似：

```text
~/.local/share/markdown_helper/
```

Qt 官方定义 `AppDataLocation` 就是“当前应用程序持久数据”的应用专属目录，在 Linux 下通常位于 `~/.local/share/<APPNAME>`。([Qt 文档][2])

---

# 四、`note.h`：整个项目最底层的数据结构

这个文件现在非常干净：

```cpp
struct Note
{
    QString id;
    QString title;
    QString folder;
    QStringList tags;
    QString fileName;
    QString updatedAt;
};
```

注意：

**这里没有正文。**

这是现在项目一个很关键的设计。

一篇笔记实际上被拆成：

```text
Note
│
├── id
├── title
├── folder
├── tags
├── fileName
└── updatedAt

正文
└── 单独存在 xxx.md 中
```

比如：

```cpp
Note {
    id        = "12ab34cd..."
    title     = "Qt 学习"
    folder    = "课程"
    tags      = {"Qt", "C++"}
    fileName  = "12ab34cd....md"
    updatedAt = "2026-09-21T..."
}
```

对应硬盘：

```text
notes.json

notes/
└── 12ab34cd....md
```

所以 `Note` 更准确的叫法其实是：

> 一篇笔记的元数据

而不是完整笔记对象。

这样做正好符合你最初作业要求里的：

> 元数据存 JSON，正文存 Markdown 文件

---

# 五、`NoteStorage`：项目的数据中心

下面这一层是：

```text
notestorage.h
notestorage.cpp
```

它不负责界面。

它根本不知道什么是：

```text
QTreeView
QTextBrowser
QLineEdit
QMainWindow
```

它只负责：

```text
内存中的笔记
        ↕
JSON / Markdown 文件
```

所以现在依赖关系非常干净：

```text
MainWindow
    │
    ▼
NoteStorage
    │
    ▼
Note
```

而不会倒过来：

```text
NoteStorage  X→ MainWindow
Note         X→ MainWindow
```

这叫**单向依赖**。

---

# 六、`notestorage.h`：存储层对外提供什么

外面的人基本只需要认识这些函数：

```cpp
bool load();

const QVector<Note> &notes() const;
const QStringList &folders() const;

const Note *findNote(const QString &noteId) const;

const Note *addNote(...);
bool updateNote(...);
bool renameNote(...);
bool moveNote(...);
bool removeNote(...);

bool addFolder(...);
bool removeFolder(...);

QString readNoteContent(...) const;
```

也就是：

```text
加载
查询
新建
修改
移动
删除
读正文
```

至于：

```cpp
saveMetadata()
saveNoteContent()
dataDirectoryPath()
noteFilePath()
createWelcomeNote()
```

全部是 `private`。

这意味着 `MainWindow` 不需要知道：

> `notes.json` 怎么构造
> Markdown 存在哪个目录
> UUID 怎么生成
> JSON 怎么解析
> 文件怎么安全写

这些全部由 `NoteStorage` 自己处理。

---

# 七、`NoteStorage` 在内存里保存了什么

只有两个变量：

```cpp
QVector<Note> noteList;
QStringList folderList;
```

所以可以直接理解成：

```text
NoteStorage
│
├── noteList
│     ├── Note
│     ├── Note
│     ├── Note
│     └── ...
│
└── folderList
      ├── "课程"
      ├── "生活"
      └── "学习"
```

没有数据库。

没有 Repository。

没有 CacheManager。

没有索引服务。

就是两个 Qt 容器。

这其实非常符合你这次要求的 Ponytail 风格。

---

# 八、磁盘上的实际数据结构

运行之后基本是：

```text
~/.local/share/markdown_helper/
│
├── notes.json
│
└── notes/
    ├── 27a53....md
    ├── 38cd1....md
    └── a812f....md
```

其中：

```text
notes.json
```

大概是：

```json
{
    "folders": [
        "入门",
        "课程"
    ],
    "notes": [
        {
            "id": "...",
            "title": "欢迎使用",
            "folder": "入门",
            "tags": [
                "示例",
                "Markdown"
            ],
            "fileName": "....md",
            "updatedAt": "..."
        }
    ]
}
```

正文不会塞进 JSON。

而是在：

```text
notes/<fileName>
```

里面。

---

# 九、`未分类`其实是一个“虚拟文件夹”

这里有一个设计细节很值得理解。

真实数据中：

```cpp
note.folder.isEmpty()
```

表示：

> 没有文件夹

但是 UI 不显示空字符串，而是显示成：

```text
未分类
```

所以：

```text
存储层：

folder = ""

        ↓ UI 转换

显示：

未分类
```

`未分类`实际上并不需要真的放进 `folderList`。

所以代码里才经常有：

```cpp
const QString actualFolder = note.folder.isEmpty()
    ? QStringLiteral("未分类")
    : note.folder;
```

而创建/移动时又反向转换：

```cpp
const QString folder =
    targetFolder == QStringLiteral("未分类")
    ? QString()
    : targetFolder;
```

这是一个很简单但合理的设计。

---

# 十、为什么使用 `QSaveFile`

现在：

```text
notes.json
Markdown 正文
导出的 HTML
```

都尽量使用：

```cpp
QSaveFile
```

它不是普通 `QFile`。

Qt 官方说明，`QSaveFile` 写入时先写临时文件，只有成功 `commit()` 后才替换目标文件，因此可以避免写到一半导致原文件损坏。([Qt 文档][3])

也就是说：

```text
普通写：

notes.json
    ↓
直接覆盖
    ↓
程序突然崩溃
    ↓
可能留下半个 JSON
```

而现在：

```text
临时文件
    ↓
完整写完
    ↓
commit()
    ↓
替换 notes.json
```

对于笔记软件这是一个很合理的 Qt 原生能力，没有自己发明复杂的事务系统。

---

# 十一、`mainwindow.h`：整个 UI 层的“总目录”

这是整个项目里非常值得先看的一个文件。

因为它告诉你：

> `MainWindow` 到底能做些什么。

这里声明了所有槽函数和成员。

现在 `MainWindow` 内部真正持有的核心状态只有：

```cpp
Ui::MainWindow *ui;

QLabel *wordCountLabel;

QStandardItemModel *noteTreeModel;
QStandardItemModel *searchSourceModel;
QSortFilterProxyModel *searchProxyModel;

QTimer *saveTimer;

NoteStorage storage;

QString currentNoteId;
```

可以把它分成四组：

```text
界面
ui
wordCountLabel

Qt Model
noteTreeModel
searchSourceModel
searchProxyModel

辅助功能
saveTimer

业务状态
storage
currentNoteId
```

非常少。

---

# 十二、`ui` 到底是什么

这里：

```cpp
Ui::MainWindow *ui;
```

很多初学 Qt 的人第一次会觉得奇怪。

它实际上对应：

```text
mainwindow.ui
       │
       │ uic 自动生成
       ▼
ui_mainwindow.h
       │
       ▼
class Ui_MainWindow
```

所以你写：

```cpp
ui->markdownEditor
ui->previewBrowser
ui->searchEdit
ui->noteTreeView
```

本质是在访问 `.ui` 里定义的控件。

例如 `.ui` 中：

```xml
<widget class="QPlainTextEdit" name="markdownEditor">
```

经过 `uic` 后，就变成可以在 C++ 使用的：

```cpp
ui->markdownEditor
```

---

# 十三、`mainwindow.ui`：现在界面其实只有三块

核心就是一个横向：

```cpp
QSplitter
```

结构非常清楚：

```text
┌─────────────┬─────────────────────┬─────────────────────┐
│             │                     │                     │
│   导航区    │   Markdown 编辑区   │      实时预览       │
│             │                     │                     │
│ 搜索框      │ 标题                │ QTextBrowser        │
│ 搜索结果    │                     │                     │
│ 笔记树      │ QPlainTextEdit      │                     │
│             │                     │                     │
└─────────────┴─────────────────────┴─────────────────────┘
```

左边：

```text
searchEdit
searchResultView
noteTreeView
```

中间：

```text
titleEdit
markdownEditor
```

右边：

```text
previewBrowser
```

菜单栏、状态栏也是 `.ui` 中创建的：

```text
menubar
statusbar
```

至于菜单里的 Action，是运行时在 `mainwindow.cpp` 中动态创建的。

---

# 十四、`mainwindow.cpp` 现在负责什么

这是现在拆分后的第一个重点。

它已经不再负责笔记增删改查。

它现在主要负责：

```text
MainWindow 生命周期
界面初始化
信号槽连接
菜单
工具栏
样式
Markdown 实时预览
HTML 导出
PDF 导出
```

也就是说它偏向：

> 窗口本身

---

# 十五、MainWindow 构造函数是整个应用初始化中心

现在启动过程基本就是：

```text
MainWindow()
│
├─ ui->setupUi(this)
│
├─ setupInterface()
│
├─ setupStyle()
│
├─ setupActions()
│
├─ 创建 QTimer
│
├─ connect 各种信号槽
│
└─ loadNotes()
```

详细一点：

```text
程序启动
   │
   ▼
MainWindow()
   │
   ├─ 根据 mainwindow.ui 创建控件
   │
   ├─ 设置 Splitter 大小
   │
   ├─ 设置字体、样式
   │
   ├─ 创建菜单和 toolbar
   │
   ├─ 创建 600ms 自动保存 Timer
   │
   ├─ markdownEditor.textChanged
   │       ├─ updatePreview()
   │       └─ 启动自动保存 Timer
   │
   ├─ titleEdit.textChanged
   │       └─ 启动 Timer
   │
   ├─ noteTreeView.clicked
   │       └─ openTreeItem()
   │
   ├─ searchEdit.textChanged
   │       └─ updateSearch()
   │
   └─ loadNotes()
```

这是理解整个项目最重要的一条主线。

---

# 十六、Markdown 实时预览非常简单

你现在没有引入第三方 Markdown 库。

核心只有：

```cpp
const QString content =
    ui->markdownEditor->toPlainText();

ui->previewBrowser
    ->document()
    ->setMarkdown(content);
```

原因就是 Qt 自带 `QTextDocument::setMarkdown()`。

Qt 官方说明，这个 API 默认支持其 Markdown 解析能力，并默认使用 GitHub 风格 Markdown 特性。([Qt 文档][4])

所以：

```text
QPlainTextEdit
   │
   │ QString
   ▼
QTextDocument::setMarkdown()
   │
   ▼
QTextBrowser
```

没有：

```text
MarkdownParser
MarkdownRenderer
PreviewManager
```

这正是现在代码大幅简化的原因之一。

---

# 十七、自动保存现在是怎么做的

这是这次重构中很重要的一部分。

当用户输入：

```text
markdownEditor
      │
      │ textChanged
      ▼
queueSave
      │
      ▼
QTimer 600ms
```

如果用户持续打字：

```text
输入
↓
Timer 从 600ms 重新开始

继续输入
↓
再次重新开始

继续输入
↓
再次重新开始
```

直到停下约 600ms：

```text
QTimer::timeout
       │
       ▼
saveCurrentNote()
```

所以不是：

> 每按一个键就写一次硬盘

而是一个非常简单的 debounce。

---

# 十八、现在保存一篇笔记时发生什么

`saveCurrentNote()` 是整个项目非常核心的函数。

数据流：

```text
titleEdit
markdownEditor
     │
     ▼
saveCurrentNote()
     │
     ├─ storage.findNote()
     │
     ├─ storage.updateNote()
     │       │
     │       ├─ 写 Markdown 文件
     │       └─ 写 notes.json
     │
     └─ updateSearchItem()
             │
             ├─ 更新树中的当前 Note
             └─ 更新搜索模型中的当前 Note
```

这里已经解决了原来一个比较大的问题。

以前可能是：

```text
保存一个字
   ↓
重新读取所有 md
   ↓
重建全部搜索数据
   ↓
重建全部树
```

现在是：

```text
保存当前笔记
   ↓
只写当前 Markdown
   ↓
更新 JSON
   ↓
只更新当前笔记对应的 UI 条目
```

这就合理很多。

---

# 十九、`mainwindow_notes.cpp` 到底是什么

这个文件虽然名字叫：

```text
mainwindow_notes.cpp
```

但它并没有：

```cpp
class MainWindowNotes
```

它还是：

```cpp
void MainWindow::createNote()
void MainWindow::saveCurrentNote()
...
```

也就是说：

```text
mainwindow.h
      │
      ├──────── mainwindow.cpp
      │          实现一部分 MainWindow 函数
      │
      └──────── mainwindow_notes.cpp
                 实现另一部分 MainWindow 函数
```

编译之后根本没有区别。

最后还是一个：

```cpp
MainWindow
```

这个做法非常适合你这个项目。

因为如果为了拆文件再创建：

```text
NoteController
SearchController
TreeManager
FolderManager
```

代码反而会越来越难看。

---

# 二十、`mainwindow_notes.cpp` 主要有四类工作

虽然都是一个文件，但内部逻辑大致是：

| 方向         | 相关函数                                                                                                               |
| ---------- | ------------------------------------------------------------------------------------------------------------------ |
| 初始化        | `loadNotes()`                                                                                                      |
| Model/View | `rebuildNoteTree()`、`rebuildSearchIndex()`、`updateSearchItem()`                                                    |
| 笔记操作       | `loadNote()`、`createNote()`、`saveCurrentNote()`、`renameSelectedItem()`、`deleteSelectedItem()`、`moveSelectedNote()` |
| 文件夹和搜索     | `createFolder()`、`updateSearch()`、`openSearchResult()`                                                             |

所以这个文件其实就是：

> 左边“笔记管理区”背后的逻辑

---

# 二十一、笔记树的数据结构

左边：

```text
课程
├── 作业1
├── Qt笔记
└── C++笔记

生活
├── 日记
└── 计划

未分类
└── 临时记录
```

不是直接拿 `QTreeView` 存数据。

而是：

```text
QStandardItemModel
        │
        ▼
QTreeView
```

也就是 Qt 的 Model/View 思路。

`rebuildNoteTree()` 创建的是：

```text
QStandardItemModel
│
├─ QStandardItem("课程")
│    ├─ QStandardItem("Qt笔记")
│    └─ QStandardItem("C++笔记")
│
├─ QStandardItem("生活")
│
└─ QStandardItem("未分类")
```

真正持久数据还是：

```text
NoteStorage
```

这个 `QStandardItemModel` 只是**给界面显示用的模型**。

---

# 二十二、三个自定义 Role 非常关键

你在 `mainwindow.h` 里定义：

```cpp
enum ItemRole
{
    ItemTypeRole = Qt::UserRole + 1,
    NoteIdRole,
    SearchTextRole
};
```

也就是说每个 UI 节点除了显示文字，还偷偷带一些附加数据。

比如树中：

```text
显示：
Qt学习

隐藏数据：
ItemTypeRole = NoteItem
NoteIdRole   = "a8c123..."
```

因此点击一项时：

```cpp
index.data(NoteIdRole).toString();
```

就能知道它对应哪一篇真正的 `Note`。

这样就不用：

```text
通过标题查笔记
```

因为标题可能重名。

而是始终：

```text
UI QModelIndex
      │
      │ NoteIdRole
      ▼
UUID
      │
      ▼
NoteStorage::findNote()
```

这一点设计得很好。

---

# 二十三、搜索是现在项目里最典型的 Qt Model/View 用法

搜索实际上用了两个模型：

```text
searchSourceModel
      │
      ▼
searchProxyModel
      │
      ▼
searchResultView
```

首先：

```cpp
QStandardItemModel *searchSourceModel;
```

里面每一行对应一篇笔记：

```text
Qt学习 [课程]
日记 [生活]
临时记录 [未分类]
```

但每一项还有隐藏的：

```cpp
SearchTextRole
```

内容是：

```text
标题
+
正文
+
tags
```

例如：

```text
Qt学习
今天学习 QTreeView 和 QStandardItemModel ...
Qt C++ 学习
```

---

# 二十四、真正搜索的是 `QSortFilterProxyModel`

然后：

```cpp
searchProxyModel->setFilterRole(SearchTextRole);
searchProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
```

用户输入：

```text
QTreeView
```

以后：

```cpp
searchProxyModel->setFilterFixedString("QTreeView");
```

于是：

```text
searchSourceModel
    所有笔记
       │
       │ proxy过滤
       ▼
searchProxyModel
    匹配的笔记
       │
       ▼
QListView
```

Qt 官方本来就支持用 `QSortFilterProxyModel` 对指定 role 的数据使用固定字符串过滤，而且常见用法正是把 `QLineEdit::textChanged` 接到过滤逻辑。([Qt 文档][5])

所以这里没有自己写：

```cpp
for (...)
    if (text.contains(...))
```

这是很典型的“能用 Qt 就不重新造”。

---

# 二十五、为什么现在使用 `match()`

比如更新一篇笔记标题。

现在不用：

```cpp
for 所有文件夹
    for 所有笔记
        if id == ...
```

而是：

```cpp
noteTreeModel->match(
    noteTreeModel->index(0, 0),
    NoteIdRole,
    note.id,
    1,
    Qt::MatchExactly | Qt::MatchRecursive);
```

意思就是：

```text
从模型开始
按 NoteIdRole
查这个 UUID
递归查子节点
```

`QAbstractItemModel::match()` 本身就是 Qt 提供的按某个 role 查找 QModelIndex 的 API。([Qt 文档][6])

所以：

```text
Note UUID
   ↓
QStandardItemModel::match()
   ↓
对应 QModelIndex
   ↓
只修改这一条
```

这是重构之后代码变短的一部分原因。

---

# 二十六、切换笔记的完整过程

用户点：

```text
课程
└── Qt学习
```

执行：

```cpp
openTreeItem(index)
```

先判断：

```cpp
ItemTypeRole == NoteItem
```

如果是文件夹：

```text
不做任何载入
```

如果是笔记：

```text
index
 │
 │ NoteIdRole
 ▼
noteId
 │
 ▼
loadNote(noteId)
```

`loadNote()`：

```text
找到 Note
   │
   ├─ 保存上一篇
   │
   ├─ currentNoteId = 新 note id
   │
   ├─ titleEdit = note.title
   │
   ├─ 从磁盘读取 Markdown
   │
   ├─ markdownEditor = 正文
   │
   ├─ updatePreview()
   │
   └─ 在树里选中它
```

这里还用了：

```cpp
QSignalBlocker
```

原因非常重要。

如果直接：

```cpp
ui->markdownEditor->setPlainText(...)
```

它会触发：

```text
textChanged
```

然后自动保存机制会以为：

> 用户修改内容了

所以载入时临时屏蔽：

```cpp
QSignalBlocker editorBlocker(ui->markdownEditor);
```

这样：

```text
程序载入文字
≠
用户修改文字
```

---

# 二十七、新建笔记的数据流

新建时：

```text
选择当前文件夹
      │
      ▼
输入标题
      │
      ▼
saveCurrentNote()
      │
      ▼
storage.addNote()
      │
      ├─ 创建 UUID
      ├─ 创建 Note
      ├─ 创建空 .md
      └─ 写 notes.json
      │
      ▼
rebuildNoteTree()
      │
      ▼
updateSearchItem()
      │
      ▼
loadNote()
```

这里：

```cpp
QUuid::createUuid()
```

保证每篇笔记有自己的 ID。

而文件名直接：

```text
UUID.md
```

避免：

```text
标题重名
非法文件名
用户重命名导致文件迁移
```

这些麻烦。

标题只存在 JSON。

文件真正名字永远保持 UUID。

这是非常省事的一种设计。

---

# 二十八、重命名为什么不需要改 Markdown 文件名

例如：

```text
Qt学习
```

改成：

```text
Qt课程笔记
```

硬盘文件：

```text
a8324fe....md
```

完全不用变。

只需要：

```text
note.title 改掉
      ↓
notes.json 更新
      ↓
树节点文字更新
      ↓
搜索项更新
```

这也是 UUID 文件名设计的优势。

---

# 二十九、移动笔记也是一样

从：

```text
课程
└── Qt学习
```

移动：

```text
学习
└── Qt学习
```

实际 `.md`：

```text
完全不移动
```

只是：

```cpp
note.folder = "学习";
```

然后保存 JSON。

所以所谓：

> 移动笔记

其实不是操作文件系统目录。

所有正文还是统一存在：

```text
notes/
```

里面。

“文件夹”只是逻辑分类。

这大幅降低了项目复杂度。

---

# 三十、删除文件夹也不会删笔记

当前设计：

```text
删除 “课程”
```

不会：

```text
删除课程里的全部笔记
```

而是：

```cpp
if (note.folder == folderName) {
    note.folder.clear();
}
```

于是：

```text
课程
├── A
└── B
```

变成：

```text
未分类
├── A
└── B
```

这也是一个相对安全的处理。

---

# 三十一、为什么删除文件夹时仍然重建整个搜索索引

通常情况下现在只更新一篇：

```text
保存一篇
→ updateSearchItem()

重命名一篇
→ updateSearchItem()

移动一篇
→ updateSearchItem()
```

但是删除一个文件夹可能导致：

```text
A [课程]
B [课程]
C [课程]
D [课程]
```

同时变成：

```text
A [未分类]
B [未分类]
C [未分类]
D [未分类]
```

一次会影响很多条。

所以这种低频操作：

```cpp
rebuildSearchIndex();
```

反而更简单。

这个选择比较符合你最初提出的：

> 不要过度优化和过度设计

---

# 三十二、导出 HTML 的结构

HTML 根本没有重新解析 Markdown。

因为右边已经有：

```text
QTextDocument
```

所以：

```cpp
ui->previewBrowser
    ->document()
    ->toHtml()
```

直接得到 HTML。

流程：

```text
Markdown
   ↓
QTextDocument
   ↓
toHtml()
   ↓
QSaveFile
   ↓
xxx.html
```

非常简单。

---

# 三十三、PDF 也是复用同一个 QTextDocument

流程：

```text
Markdown
   ↓
QTextDocument
   ↓
QPrinter(PdfFormat)
   ↓
xxx.pdf
```

核心：

```cpp
ui->previewBrowser->document()->print(&printer);
```

Qt 官方的 `QTextDocument::print()` 本身就可以把整个文档输出到 `QPagedPaintDevice`，所以这里也没有自己搞 PDF 库。([Qt 文档][4])

这也是一个很典型的 Ponytail 风格实现：

> Qt 都给你做完了，就直接调用它。

---

# 三十四、现在项目最重要的两个模型，不要搞混

这是整个项目里最容易混的地方。

```text
                    NoteStorage
                         │
              ┌──────────┴─────────┐
              ▼                    ▼
       noteTreeModel        searchSourceModel
              │                    │
              ▼                    ▼
         QTreeView         searchProxyModel
                                   │
                                   ▼
                              QListView
```

`noteTreeModel`：

```text
负责树形分类展示

文件夹
 └─ 笔记
```

`searchSourceModel`：

```text
负责全文搜索

每篇笔记一行
```

`searchProxyModel`：

```text
本身不拥有笔记数据

只是把 searchSourceModel
过滤成用户要看的结果
```

---

# 三十五、真正的“数据源”其实只有 NoteStorage

虽然名字里有很多 Model：

```text
noteTreeModel
searchSourceModel
searchProxyModel
```

但真正持久数据权威仍然只有：

```text
NoteStorage
```

正确理解应该是：

```text
NoteStorage
   │
   │ 真数据
   ▼
Note

QStandardItemModel
   │
   │ UI 显示数据
   ▼
View
```

所以用户改东西：

```text
UI
 ↓
NoteStorage
 ↓
磁盘

然后
 ↓
同步相关 UI Model
```

而不是反过来把：

```text
QTreeView
```

当数据库用。

---

# 三十六、三个地方分别保存了什么状态

可以把现在项目的状态分清楚：

| 位置                        | 保存内容     | 是否持久化  |
| ------------------------- | -------- | ------ |
| `NoteStorage::noteList`   | Note 元数据 | 内存     |
| `NoteStorage::folderList` | 文件夹      | 内存     |
| `notes.json`              | 元数据、文件夹  | 磁盘     |
| `notes/*.md`              | 正文       | 磁盘     |
| `noteTreeModel`           | 左侧树显示    | 临时 UI  |
| `searchSourceModel`       | 搜索文本     | 临时 UI  |
| `searchProxyModel`        | 当前过滤结果   | 临时 UI  |
| `currentNoteId`           | 当前编辑的是哪篇 | 临时状态   |
| `markdownEditor`          | 当前正文     | 当前界面状态 |

这个表理解以后，整个项目基本就通了。

---

# 三十七、所有文件之间的 `#include` 关系

现在非常简单：

```text
main.cpp
  │
  └── mainwindow.h
          │
          └── notestorage.h
                  │
                  └── note.h


mainwindow.cpp
  ├── mainwindow.h
  └── ui_mainwindow.h   ← mainwindow.ui 自动生成


mainwindow_notes.cpp
  ├── mainwindow.h
  └── ui_mainwindow.h


notestorage.cpp
  └── notestorage.h
          │
          └── note.h
```

注意最下面：

```text
note.h
```

不依赖任何项目内其他文件。

`NoteStorage` 只依赖 `Note`。

`MainWindow` 再依赖 `NoteStorage`。

这是一个非常清晰的箭头：

```text
MainWindow → NoteStorage → Note
```

没有循环依赖。

---

# 三十八、从运行角度看整个程序的完整数据流

把整个项目压缩成一张图就是：

```text
【程序启动】

main.cpp
   │
   ▼
MainWindow
   │
   ▼
NoteStorage::load()
   │
   ├──────────────┐
   ▼              ▼
notes.json     notes/*.md
   │
   ▼
QVector<Note>
   │
   ├──────────────┐
   ▼              ▼
笔记树模型      搜索模型
   │              │
   ▼              ▼
QTreeView      QListView


【用户打开笔记】

QTreeView
   │ NoteIdRole
   ▼
loadNote()
   │
   ▼
NoteStorage
   │
   ▼
xxx.md
   │
   ▼
QPlainTextEdit
   │
   ▼
QTextDocument::setMarkdown()
   │
   ▼
QTextBrowser


【用户编辑】

QPlainTextEdit
   │
   ├───────────────► 实时预览
   │
   └─ 600ms Timer
           │
           ▼
      saveCurrentNote()
           │
           ▼
       NoteStorage
        │       │
        ▼       ▼
       .md   notes.json
        │
        ▼
   updateSearchItem()
        │
        ├─ 更新树中这一项
        └─ 更新搜索中这一项
```

这张图基本就是你之后答辩时最应该理解的东西。

---

# 三十九、你们现在四个人的分工，和代码结构基本已经对上了

| 人           | 主要文件                                           | 本质职责                        |
| ----------- | ---------------------------------------------- | --------------------------- |
| 第 1 人：UI    | `mainwindow.ui`                                | 控件、布局、界面外观                  |
| 第 2 人：编辑/导出 | `mainwindow.cpp`、`main.cpp`                    | 主窗口初始化、Markdown 预览、HTML/PDF |
| 第 3 人：笔记交互  | `mainwindow_notes.cpp`、`mainwindow.h`          | 树、搜索、新建、删除、移动、重命名           |
| 第 4 人：存储    | `note.h`、`notestorage.h/.cpp`、`CMakeLists.txt` | JSON、Markdown、文件夹、磁盘数据      |

这里唯一会稍微产生交叉的是：

```text
mainwindow.h
```

因为第 2 和第 3 人实际上都在实现：

```cpp
MainWindow
```

但是这种程度对于四人课程项目完全可以接受。

如果为了彻底避免这个冲突再造：

```text
EditorController
NoteController
SearchService
```

我认为反而会让现在这个项目变差。

---

# 四十、现在项目应该怎样去读

如果你准备真正把这套代码吃透，我建议阅读顺序只有这一条：

1. **先看 `note.h`**，知道一篇笔记保存哪些字段
2. **再看 `notestorage.h`**，只看接口，知道数据层能干什么
3. **再看 `notestorage.cpp`**，搞懂 JSON 和 `.md` 怎么落盘
4. **看 `mainwindow.ui`**，知道屏幕上有哪些控件
5. **看 `mainwindow.h`**，知道 MainWindow 整体有哪些状态和功能
6. **看 `mainwindow.cpp`**，搞懂启动、信号槽、实时预览和导出
7. **最后看 `mainwindow_notes.cpp`**，把增删改查、树和搜索串起来
8. `main.cpp` 最后看都可以，因为它只有程序启动

尤其不要一上来从 506 行的 `mainwindow_notes.cpp` 第一行硬啃。

你现在只要牢牢记住一句：

> **`NoteStorage` 是真实数据；`MainWindow` 负责用户操作；`QStandardItemModel` 只是把数据展示给 View；正文单独存在 Markdown 文件中。**

理解这一句之后，现在这 1500 行代码实际上已经不算复杂了。

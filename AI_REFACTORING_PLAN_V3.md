# 替换 BinaryFileHelper 的重构方案 V3.0 (接口代理模式)

## 1. 核心目标与策略

* **目标**: 安全地引入新的 `RobustBinaryHelper` 数据持久化方案。
* **核心策略**: 我们采用**"接口代理"模式**。此方案**不再试图修改任何旧的 `VectorDataHandler` 代码**，而是创建一个新的**代理类 (`LegacyDataHandlerProxy`)**，用它来包装旧的逻辑并实现我们的新接口。这使得所有工作都变为AI擅长的"纯新增代码"，从根本上规避了修改复杂遗留代码的风险。

---

## 2. 通用工作准则 (AI必须遵守)

* **A. 计划是绝对指令**: 本文档是唯一指令源。**严格、精确地**按照字面意思执行每个步骤。**严禁**自行添加、修改或解读任何未明确说明的逻辑。
* **B. 严禁过早实现**: 除非任务明确要求，否则所有新方法的实现**必须**是简单的存根（stub）。
* **C. 你必须完成所有任务**: 本计划中的所有任务，包括跨文件的全局代码修改，都**必须由你来执行**。

---

## 3. 执行阶段：构建双轨并存的架构

### **任务 1：定义统一数据处理器接口 (不变)**

**目标**: 创建所有数据处理器都必须遵守的统一接口。

1. **创建文件**: 在 `vector/` 目录下创建新文件 `abstractvectordatahandler.h`。
2. **定义接口**: 将以下内容**原封不动地**写入该文件。**绝对禁止修改**。

    ```cpp
    #ifndef ABSTRACTVECTORDATAHANDLER_H
    #define ABSTRACTVECTORDATAHANDLER_H
    #include <QObject>
    #include <QString>
    #include <QList>
    #include <QVariant>
    #include <QStringList>

    class AbstractVectorDataHandler : public QObject
    {
        Q_OBJECT
    public:
        explicit AbstractVectorDataHandler(QObject *parent = nullptr) : QObject(parent) {}
        virtual ~AbstractVectorDataHandler() = default;
        virtual bool createNewProject(const QString &filePath, const QList<int> &pinList) = 0;
        virtual bool openProject(const QString &filePath) = 0;
        virtual void closeProject() = 0;
        virtual bool save() = 0;
        virtual QList<QList<QVariant>> getCurrentPage() const = 0;
        virtual int getRowCount() const = 0;
        virtual int getColumnCount() const = 0;
        virtual int getPageCount() const = 0;
        virtual int getCurrentPageIndex() const = 0;
        virtual void goToPage(int pageIndex) = 0;
        virtual QStringList getHeaderData() const = 0;
        virtual QString getVectorTableName() const = 0;
        virtual void setVectorTableName(const QString &name) = 0;
        virtual bool addRows(int row, int count) = 0;
        virtual bool deleteRows(const QList<int> &rows) = 0;
    };
    #endif // ABSTRACTVECTORDATAHANDLER_H
    ```

### **任务 2：创建"旧逻辑代理" (新核心)**

**目标**: 创建一个代理类，它实现新接口，但内部将所有工作**转发**给旧的 `VectorDataHandler` 单例。**我们不修改旧类，只包装它。**

1. **创建头文件**: 在 `vector/` 目录下创建 `legacydataproxy.h`。

    ```cpp
    #ifndef LEGACYDATAPROXY_H
    #define LEGACYDATAPROXY_H
    #include "abstractvectordatahandler.h"

    class LegacyDataProxy : public AbstractVectorDataHandler {
        Q_OBJECT
    public:
        explicit LegacyDataProxy(QObject* parent = nullptr);
        // 实现所有12个接口方法
        bool createNewProject(const QString &filePath, const QList<int> &pinList) override;
        bool openProject(const QString &filePath) override;
        void closeProject() override;
        // ... 其他所有接口方法的声明 ...
    };
    #endif // LEGACYDATAPROXY_H
    ```

2. **创建实现文件**: 在 `vector/` 目录下创建 `legacydataproxy.cpp`。
    * **强制要求**: 它的实现**必须**是简单的"函数转发"。**严禁添加任何新逻辑。**
    * **示例**:

        ```cpp
        #include "legacydataproxy.h"
        #include "vectordatahandler.h" // 包含旧的头文件以调用其单例
        #include <QDebug>

        LegacyDataProxy::LegacyDataProxy(QObject* parent) : AbstractVectorDataHandler(parent) {}
        
        bool LegacyDataProxy::createNewProject(const QString &filePath, const QList<int> &pinList) {
            // 这个接口在旧处理器中没有直接对应，所以返回一个安全值。
            qWarning() << "LegacyDataProxy: createNewProject is not supported by the old handler.";
            Q_UNUSED(filePath); Q_UNUSED(pinList);
            return false;
        }

        bool LegacyDataProxy::deleteRows(const QList<int> &rows) {
            qDebug() << "LegacyDataProxy: Forwarding deleteRows to old VectorDataHandler.";
            QString dummyError; // 旧API需要一个我们不在乎的参数
            int dummyTableId = 0; // 旧API需要一个tableId, 逻辑需要适配
            // 注意：这里的适配可能需要根据旧API的具体情况进行微调
            return VectorDataHandler::instance().deleteVectorRows(dummyTableId, rows, dummyError);
        }
        
        // ... 为所有其他接口方法实现类似的、简单的转发逻辑 ...
        ```

    * **注意**: 某些新接口方法在旧的`VectorDataHandler`中可能没有1对1的完美匹配，这时应返回一个安全的默认值（如`false`或空列表）并打印警告。

### **任务 3：创建新的 `RobustVectorDataHandler` (空壳)**

**目标**: 创建一个新的、符合接口的、但内部为空的 `RobustVectorDataHandler` 类。

1. **创建文件**: `vector/robustvectordatahandler.h` 和 `vector/robustvectordatahandler.cpp`。
2. **强制要求**: 内部所有接口方法**必须**实现为最简单的存根，与任务2.2中的范例类似（打印警告，返回默认值）。这是搭建框架的步骤，**不是功能实现**。

### **任务 4：改造 MainWindow 以使用策略模式**

**目标**: 将 `MainWindow` 对数据处理器的依赖改为 `AbstractVectorDataHandler` 接口，并根据用户选择实例化不同的处理器。

1. **修改 `app/mainwindow.h`**:
    * 包含头文件: `#include "vector/abstractvectordatahandler.h"` 和 `#include <memory>`。
    * 添加成员变量: `std::unique_ptr<AbstractVectorDataHandler> m_dataHandler;`
    * 添加新菜单项的 `QAction*` 和 `slot`。

2. **修改 `app/mainwindow_project_tables.cpp`** (或类似文件):
    * 在处理**旧项目**的函数 (`createNewProject`, `openExistingProject`) 开头，实例化**代理**:
        `m_dataHandler = std::make_unique<LegacyDataProxy>();`
    * 在处理**新项目**的函数 (`onNewRobustProjectTriggered`) 开头，实例化**新处理器**:
        `m_dataHandler = std::make_unique<RobustVectorDataHandler>();`
    * **执行全局替换**: 将 `MainWindow` 中所有对 `VectorDataHandler::instance()->...` 的调用，替换为对 `m_dataHandler->...` 的调用。

### **任务 5：实现 `RobustVectorDataHandler` 的"快乐路径"**

**目标**: 在任务3的空壳基础上，为新处理器添加最基本的功能。

1. **修改 `vector/robustvectordatahandler.cpp`**:
    * 为 `createNewProject(...)` 添加真实逻辑: 调用 `Persistence::RobustBinaryHelper::createFile(filePath);`。
    * 为 `getRowCount()` 添加真实逻辑: `return 0;`
    * 为 `getHeaderData()` 添加真实逻辑: `return {"Time", "VEC", "VEC_COMMENT"};`
    * 为 `getCurrentPage()` 添加真实逻辑: `return {};`

### **任务 6：更新构建系统**

**目标**: 确保所有新文件都被正确编译和链接。

1. **修改 `CMakeLists.txt`**:
    * 在 `PROJECT_SOURCES` 列表中，添加以下**所有**新文件：
        * `vector/abstractvectordatahandler.h`
        * `vector/legacydataproxy.h`
        * `vector/legacydataproxy.cpp`
        * `vector/robustvectordatahandler.h`
        * `vector/robustvectordatahandler.cpp`
    * **务必确认 `robust_binary_helper.h/cpp` 已在列表中。**
    * **务必确认没有添加 `vectordatahandler_interface.cpp` (该文件已废弃)。**

---

## 6. 注意事项 (给AI的特别提醒)

* **1.【新核心】代理模式是关键**: `LegacyDataProxy` 的唯一职责是"转发"调用给旧的 `VectorDataHandler::instance()`, **严禁在代理中添加任何新业务逻辑**。
* **2.【不变】不许碰旧代码**: `VectorDataHandler.h` 和 `VectorDataHandler` 所有相关的 `*.cpp` 实现文件，在此次重构中都**不应被打开或修改**。它们被视为一个黑盒。
* **3.【不变】全局替换是你的责任**: 任务4.2中对 `MainWindow` 的全局修改，**必须由你来完成**。
* **4.【不变】存根优先**: 搭建框架时，**必须**使用最简单的存根实现，直到任务明确授权进行功能开发。

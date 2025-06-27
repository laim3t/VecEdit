# 项目重构计划 V5.0 - "严谨的 IF-ELSE 包裹法"

## 1. 核心目标与策略

* **最终目标**: 安全地用新的 `RobustBinaryHelper` 替换旧的 `BinaryFileHelper` 数据持久化方案。
* **核心策略**: 我们将采用 **"严谨的 IF-ELSE 包裹法" (The Methodical IF-ELSE Wrapper)**。此方法继承了 V4.0 的核心思想，但将执行过程极度细化，以确保 AI 工具能够稳定、准确地完成任务。
* **工作原理**:
    1. 在 `MainWindow` 中引入一个简单的布尔标志 `m_useNewDataHandler` 来控制激活哪套逻辑。
    2. AI 的核心任务是，遵循一个严格的、**逐个文件、逐个函数**的流程，找到每一个调用旧 `VectorDataHandler::instance()` 的地方。
    3. 将这些调用点用 `if-else` 语句包裹起来。
    4. `else` 代码块中，保留 **一模一样、未经任何改动** 的旧代码调用。这是确保现有功能 100% 不被破坏的关键。
    5. `if` 代码块中，将调用新的 `RobustVectorDataHandler` 中的对应方法。

## 2. 通用工作准则 (AI 必须遵守)

* **A. 计划是绝对指令**: 严格、精确地按照本文档的字面意思执行每个步骤。**严禁**自行添加、修改或解读任何未明确说明的逻辑。
* **B. 严禁修改 `else` 块**: 在执行包裹操作时，`else` 块中的代码必须与原始代码**完全一致**。
* **C. 流程必须遵守**: 你必须严格按照任务分解的流程执行，特别是任务3中的循环和确认步骤。**禁止**一次性修改多个文件或多个函数。

---

## 3. 执行阶段：搭建并行的代码路径

### **任务 1：创建新的 `RobustVectorDataHandler` (作为存根)**

**目标**: 创建一个新的、独立的、但内部逻辑为空的 `RobustVectorDataHandler` 类。

1. **创建文件**: 在 `vector/` 目录下创建 `robustvectordatahandler.h` 和 `robustvectordatahandler.cpp`。
2. **定义类 (Header)**:
    * 这个类应该是一个独立的类，**不继承自任何接口**。为了与旧代码的访问模式保持一致，可以也设计成单例。
    * 在 `robustvectordatahandler.h` 中，为 `VectorDataHandler` 中的每一个公共方法，都创建一个同名的、空的"存根"方法。
    * **示例 `robustvectordatahandler.h`**:

        ```cpp
        #ifndef ROBUSTVECTORDATAHANDLER_H
        #define ROBUSTVECTORDATAHANDLER_H
        #include <QObject>
        // ... 其他必要的 Qt includes ...
        
        class RobustVectorDataHandler : public QObject {
            Q_OBJECT
        public:
            static RobustVectorDataHandler& instance(); // 单例访问
            bool createNewProject(const QString &filePath, const QList<int> &pinList);
            bool openProject(const QString &filePath);
            // ... VectorDataHandler 中其他所有公共方法的存根声明 ...

        private:
            RobustVectorDataHandler(QObject* parent = nullptr); // 私有构造
            ~RobustVectorDataHandler() = default;
            RobustVectorDataHandler(const RobustVectorDataHandler&) = delete;
            RobustVectorDataHandler& operator=(const RobustVectorDataHandler&) = delete;
        };
        #endif // ROBUSTVECTORDATAHANDLER_H
        ```

3. **实现类 (CPP)**:
    * 在 `robustvectordatahandler.cpp` 中，提供这些存根方法的极简实现。
    * **强制要求**: 方法体应该只包含一行 `qWarning()` 来提示此功能尚未实现，并返回一个安全的默认值（例如 `false`, `0`, `nullptr`, `""`, `{}`).
    * **示例 `robustvectordatahandler.cpp`**:

        ```cpp
        #include "robustvectordatahandler.h"
        #include <QDebug>

        RobustVectorDataHandler& RobustVectorDataHandler::instance() {
            static RobustVectorDataHandler inst;
            return inst;
        }

        RobustVectorDataHandler::RobustVectorDataHandler(QObject* parent) : QObject(parent) {}

        bool RobustVectorDataHandler::createNewProject(const QString&, const QList<int>&) {
            qWarning() << "RobustVectorDataHandler::createNewProject is not implemented yet.";
            return false;
        }

        bool RobustVectorDataHandler::openProject(const QString&) {
            qWarning() << "RobustVectorDataHandler::openProject is not implemented yet.";
            return false;
        }

        // ... 为所有其他存根方法提供类似的默认实现 ...
        ```

### **任务 2：改造 `MainWindow` 以引入控制开关**

**目标**: 在 `MainWindow` 中加入新旧逻辑的切换开关和对新处理器的引用。

1. **修改 `app/mainwindow.h`**:
    * **包含头文件**:

        ```cpp
        #include "vector/robustvectordatahandler.h" 
        ```

    * **添加成员变量**: 在 `private` 部分添加：

        ```cpp
        bool m_useNewDataHandler; // 控制开关
        RobustVectorDataHandler* m_robustDataHandler; // 指向新处理器的指针
        ```

2. **修改 `app/mainwindow_setup_1.cpp`** (或 `MainWindow` 的构造函数所在文件):
    * 在 `MainWindow` 的构造函数中，**初始化**这两个新成员变量：

        ```cpp
        m_useNewDataHandler = false; // 默认关闭新功能，确保应用启动时行为不变
        m_robustDataHandler = &RobustVectorDataHandler::instance(); // 获取新处理器的实例
        ```

3. **(可选，但强烈推荐)** 为了方便测试，可以在 UI 上添加一个临时的 `QAction` (例如，在"帮助"菜单下)，用来触发一个 slot，该 slot 的功能就是切换 `m_useNewDataHandler` 的布尔值。

### **任务 3：执行核心重构：逐个函数、逐个文件地包裹调用 (全新流程)**

**目标**: 这是本次重构最关键的部分。此前的失败正是因为此任务的执行粒度过大。现在，你必须严格遵循以下**微操流程**。

**3.1: 生成待办清单 (Checklist)**

* **行动**: 对 `app/` 目录执行一次精确搜索，找出所有包含 `VectorDataHandler::instance()` 字符串的 `mainwindow_*.cpp` 文件。
* **输出**: 将找到的文件路径列表展示给用户。这个列表就是你的待办清单。

**3.2: 开启文件处理循环**

* **行动**: 从待办清单的第一个文件开始，依次处理。在开始处理一个文件前，必须向用户报告："正在处理文件：[文件路径]"。

**3.3: 逐个函数进行修改 (核心步骤)**

* **行动**: 在当前处理的文件内，**找到第一个包含 `VectorDataHandler::instance()` 调用的函数**。
* **替换规则**:
  * **不要只修改那一行代码**。你必须复制**整个函数体**，然后在这个函数体内应用 `if-else` 包装逻辑。
  * 对于 **不返回任何值** 的调用，例如： `VectorDataHandler::instance()->closeProject();`
        应被替换为：

        ```cpp
        if (m_useNewDataHandler) {
            m_robustDataHandler->closeProject();
        } else {
            VectorDataHandler::instance()->closeProject();
        }
        ```

  * 对于 **返回一个值** 的调用，例如： `int count = VectorDataHandler::instance()->getRowCount();`
        应被替换为：

        ```cpp
        int count;
        if (m_useNewDataHandler) {
            count = m_robustDataHandler->getRowCount();
        } else {
            count = VectorDataHandler::instance()->getRowCount();
        }
        ```

        **注意**: 必须将变量声明提到 `if-else` 块的外部。
* **行动**: 使用 `edit_file` 工具，用修改后的**整个函数体**替换掉原始的函数体。
* **行动**: **重复此过程**，直到当前文件内**所有**包含 `VectorDataHandler::instance()` 的函数都被修改完毕。

**3.4: 文件级别确认**

* **行动**: 当一个文件中的所有相关函数都已修改完毕后，向用户报告："文件 [文件路径] 处理完毕。请确认修改。"
* **等待**: 等待用户确认后，再从待办清单中选择下一个文件，返回执行 **步骤 3.2**。

**3.5: 循环结束**

* **行动**: 当待办清单中的所有文件都处理完毕后，向用户报告："所有文件均已处理完毕。"

### **任务 4：更新构建系统**

**目标**: 确保新创建的文件能够被正确编译和链接。

1. **修改 `CMakeLists.txt`**:
    * 在 `PROJECT_SOURCES` 列表中，添加以下**所有**新文件：
        * `vector/robustvectordatahandler.h`
        * `vector/robustvectordatahandler.cpp`
    * **务必确认 `robust_binary_helper.h/cpp` (如果已创建) 也已在列表中。**
    * **务-   **务必不要添加** 任何与 `AbstractVectorDataHandler` 或 `LegacyDataProxy` 相关的文件，它们在此方案中已被废弃。

### **任务 5：最终验证**

**目标**: 确保所有旧的调用都已被正确包裹。

1. **最终检查**:
    * **行动**: 再次对 `app/` 目录执行一次精确搜索，查找 `VectorDataHandler::instance()`。
    * **预期结果**: 所有的匹配项都应该位于 `else` 块内部。不应该有任何裸露的（未被包裹的）调用。
    * **报告**: 向用户报告检查结果。

---

## 4. 后续工作 (在 AI 完成上述任务后)

1. **编译和验证**: 在 `m_useNewDataHandler = false` 的情况下，编译并运行程序，验证所有功能是否与重构前完全一致。
2. **实现新逻辑**: 在 `RobustVectorDataHandler` 的各个方法中，逐步用 `RobustBinaryHelper` 实现真正的业务逻辑。
3. **测试新路径**: 在代码中将 `m_useNewDataHandler` 设置为 `true`，然后编译运行，测试新数据处理路径的功能。
4. **最终清理**: 当 `RobustVectorDataHandler` 被完全实现和测试后，便可以进行最后的清理工作。

这个 V5.0 计划通过将宏大任务分解为一系列定义明确、可验证的微小步骤，最大限度地提高了 AI 执行的成功率。

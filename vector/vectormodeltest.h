#ifndef VECTORMODELTEST_H
#define VECTORMODELTEST_H

#include <QObject>
#include <QString>
#include <QList>

namespace Vector
{

    /**
     * @class VectorModelTest
     * @brief 用于测试VectorTableModel的辅助类
     *
     * 提供一些辅助方法来初始化测试数据和环境，
     * 使测试程序能更容易地验证VectorTableModel的功能。
     */
    class VectorModelTest : public QObject
    {
        Q_OBJECT

    public:
        /**
         * @brief 构造函数
         * @param parent 父对象
         */
        explicit VectorModelTest(QObject *parent = nullptr);

        /**
         * @brief 初始化测试数据
         *
         * 打开指定的数据库文件，并确保其中包含必要的表结构
         *
         * @param dbPath 数据库文件路径
         * @return 初始化成功返回true，失败返回false
         */
        bool initializeTestData(const QString &dbPath);

        /**
         * @brief 获取可用的向量表ID列表
         * @return 向量表ID列表
         */
        QList<int> getAvailableTables();

        /**
         * @brief 获取表名称
         * @param tableId 表ID
         * @return 表名称
         */
        QString getTableName(int tableId);
    };

} // namespace Vector

#endif // VECTORMODELTEST_H
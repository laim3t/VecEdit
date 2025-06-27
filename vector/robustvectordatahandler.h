#ifndef ROBUSTVECTORDATAHANDLER_H
#define ROBUSTVECTORDATAHANDLER_H

#include <QObject>
#include <QString>
#include <QList>

class RobustVectorDataHandler : public QObject
{
    Q_OBJECT
public:
    static RobustVectorDataHandler &instance();

    // 核心项目操作
    bool createNewProject(const QString &filePath, const QList<int> &pinList);
    bool openProject(const QString &filePath);
    void closeProject();
    bool saveProject();
    bool saveProjectAs(const QString &filePath);

    // 向量表操作
    bool deleteVectorTable(int tableId, QString &errorMessage);

    // 基本数据访问
    int getRowCount() const;
    int getColumnCount() const;
    QString getData(int row, int column) const;
    bool setData(int row, int column, const QString &value);

    // 行操作
    bool insertRow(int row);
    bool removeRow(int row);
    bool insertRows(int row, int count);
    bool removeRows(int row, int count);

    // 状态查询
    bool isProjectOpen() const;
    QString getCurrentProjectPath() const;

private:
    RobustVectorDataHandler(QObject *parent = nullptr);
    ~RobustVectorDataHandler() = default;
    RobustVectorDataHandler(const RobustVectorDataHandler &) = delete;
    RobustVectorDataHandler &operator=(const RobustVectorDataHandler &) = delete;
};

#endif // ROBUSTVECTORDATAHANDLER_H
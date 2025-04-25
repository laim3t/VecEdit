#ifndef TIMESETDATAACCESS_H
#define TIMESETDATAACCESS_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMap>
#include <QString>
#include <QList>
#include <QTreeWidgetItem>
#include <QTableWidget>
#include <QDebug>

// TimeSet边沿参数条目数据结构
struct TimeSetEdgeData
{
    int timesetId; // 关联的TimeSet ID
    int pinId;     // 关联的管脚ID
    double t1r;    // T1R值，默认250
    double t1f;    // T1F值，默认750
    double stbr;   // STBR值，默认500
    int waveId;    // 波形ID，关联wave_options表
};

// TimeSet数据结构
struct TimeSetData
{
    int dbId;                     // 数据库中的ID，用于更新时的精确匹配
    QString name;                 // TimeSet名称
    double period;                // 周期，单位ns
    QList<int> pinIds;            // 关联的管脚ID列表
    QList<TimeSetEdgeData> edges; // 边沿参数列表
};

class TimeSetDataAccess
{
public:
    TimeSetDataAccess(QSqlDatabase &db);

    // 返回数据库连接
    QSqlDatabase &database() { return m_db; }

    // 查询方法
    bool loadWaveOptions(QMap<int, QString> &waveOptions);
    bool loadPins(QMap<int, QString> &pinList);
    bool loadExistingTimeSets(QList<TimeSetData> &timeSetDataList);
    QList<TimeSetData> loadExistingTimeSets();
    QList<TimeSetEdgeData> loadTimeSetEdges(int timeSetId);

    // 验证方法
    bool isTimeSetNameExists(const QString &name);
    bool isTimeSetInUse(int timeSetId);

    // 保存方法
    bool saveTimeSetToDatabase(const TimeSetData &timeSet, int &outTimeSetId);
    bool saveTimeSetEdgesToDatabase(int timeSetId, const QList<TimeSetEdgeData> &edges);
    bool updateTimeSetName(int timeSetId, const QString &newName);
    bool updateTimeSetPeriod(int timeSetId, double period);

    // 删除方法
    bool deleteTimeSet(int timeSetId);
    bool deleteTimeSetEdge(int timeSetId, int pinId);

    // 向量数据方法
    bool loadVectorData(int tableId, QTableWidget *vectorTable);
    bool saveVectorData(int tableId, QTableWidget *vectorTable, int insertPosition, bool appendToEnd);

    // 管脚相关方法
    QList<int> getPinIdsForTimeSet(int timeSetId);
    bool savePinSelection(int timeSetId, const QList<int> &selectedPinIds);

private:
    QSqlDatabase &m_db;
};

#endif // TIMESETDATAACCESS_H
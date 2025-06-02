#ifndef TIMESETMODEL_H
#define TIMESETMODEL_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QList>
#include <QMap>
#include <QString>

// TimeSet数据结构
struct TimeSetData
{
    int dbId;          // 数据库中的ID，用于更新时的精确匹配
    QString name;      // TimeSet名称
    double period;     // 周期，单位ns
    QList<int> pinIds; // 关联的管脚ID列表
};

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

class TimeSetModel
{
public:
    TimeSetModel();
    ~TimeSetModel();

    // 数据加载方法
    bool loadWaveOptions(QMap<int, QString> &waveOptions);
    bool loadPins(QMap<int, QString> &pinList);
    bool loadExistingTimeSets(QList<TimeSetData> &timeSetDataList);
    bool loadTimeSetEdges(int timeSetId, QList<TimeSetEdgeData> &edgeDataList);

    // 数据保存方法
    bool saveTimeSetToDatabase(const TimeSetData &timeSet, int &outTimeSetId);
    bool saveTimeSetEdgesToDatabase(int timeSetId, const QList<TimeSetEdgeData> &edges);
    bool removeTimeSetFromDatabase(int timeSetId);
    bool updateTimeSetName(int timeSetId, const QString &newName);
    bool updateTimeSetPeriod(int timeSetId, double period);

    // 向量数据处理
    bool getVectorData(int tableId, QList<QStringList> &vectorData);
    bool saveVectorData(int tableId, const QList<QStringList> &vectorData);
    bool getPinOptions(QStringList &pinOptions);
    int getVectorRowCount(int tableId);

private:
    QSqlDatabase db;
    void initDatabase();
    void ensureDatabaseConsistency(); // 添加数据库一致性检查方法
};

#endif // TIMESETMODEL_H
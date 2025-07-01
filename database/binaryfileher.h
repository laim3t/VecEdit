// 读取并验证二进制文件头部
static bool readAndValidateHeader(QFile &file, Persistence::HeaderData &headerData, int expectedSchemaVersion);

// 读取二进制文件中所有行的物理偏移量和大小
static bool readAllRowOffsetsAndSizes(QFile &file, QList<QPair<qint64, qint64>> &rowPositions);

private:
// 私有辅助函数 
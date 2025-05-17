-- 升级数据库脚本：从版本2到版本3
-- 添加 IsVisible 字段到 VectorTableColumnConfiguration 表，用于逻辑删除列

BEGIN TRANSACTION;

-- 添加 IsVisible 字段到 VectorTableColumnConfiguration 表
ALTER TABLE VectorTableColumnConfiguration ADD COLUMN IsVisible BOOLEAN NOT NULL DEFAULT 1;

-- 更新所有现有记录，将 IsVisible 设置为 true (1)
UPDATE VectorTableColumnConfiguration SET IsVisible = 1;

-- 注意：更新数据库版本号到 3 将由 C++ 代码中的 DatabaseManager::performSchemaUpgradeToV3 处理
-- 在此脚本成功执行后。

COMMIT; 
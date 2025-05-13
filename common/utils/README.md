# 路径工具类 (PathUtils)

## 功能概述

PathUtils类提供了一系列工具函数，用于处理VecEdit应用程序中的文件路径。主要功能包括生成二进制数据目录的路径、标准化路径格式以及处理各种路径相关的操作。

## 使用说明

### 生成二进制数据目录路径

```cpp
// 根据数据库文件路径生成对应的二进制数据目录路径
QString dbPath = "C:/Users/username/Documents/project.db";
QString binaryDataDir = Utils::PathUtils::getProjectBinaryDataDirectory(dbPath);
// 返回: "C:\Users\username\Documents\project_vbindata"
```

### 路径标准化

PathUtils中的函数会自动处理路径格式，确保返回的路径使用系统本地分隔符。

## 输入/输出规范

### getProjectBinaryDataDirectory

- **输入**: 数据库文件的绝对路径（QString）
- **输出**: 二进制数据目录的绝对路径（QString）
- **处理逻辑**:
  1. 从数据库路径中提取基本名称和目录
  2. 在同级目录下创建以"_vbindata"为后缀的目录名称
  3. 标准化路径格式后返回

## 向后兼容性

PathUtils是完全向后兼容的。最近的更新增强了路径格式处理，确保在所有平台上保持一致的路径格式，解决了由于混合使用路径分隔符（'/'和'\'）导致的问题。

## 版本和变更历史

### 当前版本: 1.1

#### 变更日志

- **1.1** (2025-05-13)
  - 增强了路径格式处理，确保使用QDir::toNativeSeparators标准化所有路径
  - 修复了在Windows平台上混合使用正斜杠和反斜杠导致的二进制文件加载问题

- **1.0** (2025-05-12)
  - 首次发布
  - 基本路径处理功能

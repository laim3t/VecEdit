#include "utils/fileutils.h"
#include <QSqlError>
#include <QDebug>
#include <QSqlQuery>
#include <QVariant>
#include <QDir>
#include <QStandardPaths>

Vector VectorDAO::addVector(const QString &name, const QString &metadata, const QByteArray &binaryData, VectorCollection *collection)
{
    qDebug() << "VectorDAO::addVector - Entry. Name:" << name << "Metadata:" << metadata << "Collection ID:" << (collection ? QString::number(collection->id) : "None");

    if (name.isEmpty())
    {
        qDebug() << "VectorDAO::addVector - Error: Name cannot be empty.";
        // Consider throwing an exception or returning a special error Vector object
        return Vector();
    }

    QString filename = QString::number(QDateTime::currentMSecsSinceEpoch()) + ".bin";
    // Ensure the path is constructed correctly, especially in different environments or OS
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString filePath = appDataPath + QDir::separator() + filename;
    qDebug() << "VectorDAO::addVector - Generated filename:" << filename << "Full path:" << filePath;

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);
    query.prepare("INSERT INTO vectors (name, metadata, filename, collection_id, created_at, updated_at) "
                  "VALUES (:name, :metadata, :filename, :collection_id, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)");
    query.bindValue(":name", name);
    query.bindValue(":metadata", metadata);
    query.bindValue(":filename", filename); // Store only the filename, not the full path
    if (collection)
    {
        query.bindValue(":collection_id", collection->id);
    }
    else
    {
        query.bindValue(":collection_id", QVariant(QVariant::Int)); // Bind NULL if no collection
    }

    Vector newVector;
    newVector.name = name;
    newVector.metadata = metadata;
    newVector.filename = filename;                             // Store only filename in the object as well
    newVector.collectionId = collection ? collection->id : -1; // Use -1 or some indicator for no collection

    if (!query.exec())
    {
        qDebug() << "VectorDAO::addVector - SQL Error after preparing insert:" << query.lastError().text();
        // Return an invalid Vector or throw an exception
        return Vector();
    }

    int lastId = query.lastInsertId().toInt();
    newVector.id = lastId;
    qDebug() << "VectorDAO::addVector - Successfully inserted vector record with ID:" << lastId;

    // Save the binary data to the file
    // Ensure the directory exists before trying to save the file
    QDir dir(appDataPath);
    if (!dir.exists())
    {
        qDebug() << "VectorDAO::addVector - AppDataLocation does not exist, attempting to create:" << appDataPath;
        if (!dir.mkpath("."))
        { // mkpath will create all necessary parent directories
            qDebug() << "VectorDAO::addVector - Failed to create directory:" << appDataPath;
            // Rollback: Delete the database record if directory creation fails
            QSqlQuery deleteQuery(db);
            deleteQuery.prepare("DELETE FROM vectors WHERE id = :id");
            deleteQuery.bindValue(":id", lastId);
            if (!deleteQuery.exec())
            {
                qDebug() << "VectorDAO::addVector - Rollback SQL Error (directory creation failed):" << deleteQuery.lastError().text();
            }
            else
            {
                qDebug() << "VectorDAO::addVector - Rollback successful (directory creation failed): Deleted vector record with ID:" << lastId;
            }
            return Vector(); // Return an invalid vector
        }
        qDebug() << "VectorDAO::addVector - Successfully created directory:" << appDataPath;
    }

    if (!FileUtils::saveBinaryFile(filePath, binaryData))
    {
        qDebug() << "VectorDAO::addVector - Failed to save binary file:" << filePath;
        // Rollback: Delete the database record
        QSqlQuery deleteQuery(db);
        deleteQuery.prepare("DELETE FROM vectors WHERE id = :id");
        deleteQuery.bindValue(":id", lastId);
        if (!deleteQuery.exec())
        {
            qDebug() << "VectorDAO::addVector - Rollback SQL Error:" << deleteQuery.lastError().text();
        }
        else
        {
            qDebug() << "VectorDAO::addVector - Rollback successful: Deleted vector record with ID:" << lastId;
        }
        // Return an invalid Vector or throw an exception
        return Vector();
    }

    qDebug() << "VectorDAO::addVector - Successfully saved binary file:" << filePath;
    qDebug() << "VectorDAO::addVector - Exit. Returning new vector with ID:" << newVector.id;
    return newVector;
}

Vector VectorDAO::getVectorById(int id)
{
    qDebug() << "VectorDAO::getVectorById - Entry. ID:" << id;
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);
    query.prepare("SELECT id, name, metadata, filename, collection_id FROM vectors WHERE id = :id");
    query.bindValue(":id", id);

    Vector vector;
    if (!query.exec())
    {
        qDebug() << "VectorDAO::getVectorById - SQL Error:" << query.lastError().text();
        return vector; // Return an empty/invalid vector
    }

    if (query.next())
    {
        vector.id = query.value("id").toInt();
        vector.name = query.value("name").toString();
        vector.metadata = query.value("metadata").toString();
        vector.filename = query.value("filename").toString(); // Filename only
        vector.collectionId = query.value("collection_id").toInt();
        qDebug() << "VectorDAO::getVectorById - Found vector. ID:" << vector.id << "Name:" << vector.name << "Filename:" << vector.filename;

        // Construct the full path to the binary file
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString filePath = appDataPath + QDir::separator() + vector.filename;
        qDebug() << "VectorDAO::getVectorById - Attempting to load binary data from:" << filePath;

        QByteArray binaryData;
        if (FileUtils::loadBinaryFile(filePath, binaryData))
        {
            vector.setBinaryData(binaryData); // Assuming Vector class has a method to set binary data
            qDebug() << "VectorDAO::getVectorById - Successfully loaded binary data for vector ID:" << vector.id << "Size:" << binaryData.size();
        }
        else
        {
            qDebug() << "VectorDAO::getVectorById - Failed to load binary data from file:" << filePath << "for vector ID:" << vector.id;
            // Decide on handling: return vector without data, or mark as invalid, or clear other fields?
            // For now, returning vector with data missing, filename still present.
        }
    }
    else
    {
        qDebug() << "VectorDAO::getVectorById - No vector found with ID:" << id;
    }
    qDebug() << "VectorDAO::getVectorById - Exit.";
    return vector;
}

bool VectorDAO::updateVector(const Vector &vector)
{
    qDebug() << "VectorDAO::updateVector - Entry. Vector ID:" << vector.id << "Name:" << vector.name;
    if (vector.id <= 0)
    {
        qDebug() << "VectorDAO::updateVector - Error: Invalid vector ID.";
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);
    query.prepare("UPDATE vectors SET name = :name, metadata = :metadata, filename = :filename, "
                  "collection_id = :collection_id, updated_at = CURRENT_TIMESTAMP WHERE id = :id");
    query.bindValue(":name", vector.name);
    query.bindValue(":metadata", vector.metadata);
    query.bindValue(":filename", vector.filename); // Store only filename
    query.bindValue(":collection_id", vector.collectionId > 0 ? QVariant(vector.collectionId) : QVariant(QVariant::Int));
    query.bindValue(":id", vector.id);

    if (!query.exec())
    {
        qDebug() << "VectorDAO::updateVector - SQL Error:" << query.lastError().text();
        return false;
    }

    bool success = query.numRowsAffected() > 0;
    if (success)
    {
        qDebug() << "VectorDAO::updateVector - Successfully updated vector record for ID:" << vector.id;
        // If binary data is part of the Vector object and might have changed, resave it.
        // This example assumes binary data is handled separately or filename change implies new data.
        if (!vector.getBinaryData().isEmpty())
        { // Assuming getBinaryData() exists
            QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QString filePath = appDataPath + QDir::separator() + vector.filename;
            if (!FileUtils::saveBinaryFile(filePath, vector.getBinaryData()))
            {
                qDebug() << "VectorDAO::updateVector - Failed to save updated binary file:" << filePath << "for vector ID:" << vector.id;
                // CRITICAL: Consider rollback strategy here too. If DB is updated but file save fails, data is inconsistent.
                // For simplicity, current example does not implement complex rollback for update.
                // A robust solution might involve transactions if DB supports, or a two-phase commit like pattern.
                // Or, save to a temp file, then rename, then update DB. If DB fails, delete temp file.
                // Or, update DB first, if file save fails, revert DB change (harder if original values not kept).
                // Current: logs error, returns true as DB part succeeded. This might not be ideal.
            }
            else
            {
                qDebug() << "VectorDAO::updateVector - Successfully saved updated binary file:" << filePath << "for vector ID:" << vector.id;
            }
        }
    }
    else
    {
        qDebug() << "VectorDAO::updateVector - No vector record found or no change for ID:" << vector.id;
    }

    qDebug() << "VectorDAO::updateVector - Exit. Success:" << success;
    return success;
}

bool VectorDAO::deleteVector(int id)
{
    qDebug() << "VectorDAO::deleteVector - Entry. ID:" << id;
    if (id <= 0)
    {
        qDebug() << "VectorDAO::deleteVector - Error: Invalid vector ID.";
        return false;
    }

    QSqlDatabase db = DatabaseManager::instance().database();

    // First, get the filename to delete the associated binary file
    QSqlQuery selectQuery(db);
    selectQuery.prepare("SELECT filename FROM vectors WHERE id = :id");
    selectQuery.bindValue(":id", id);
    QString filenameToDelete;

    if (!selectQuery.exec())
    {
        qDebug() << "VectorDAO::deleteVector - SQL Error (select filename):" << selectQuery.lastError().text();
        return false; // Cannot proceed without filename
    }

    if (selectQuery.next())
    {
        filenameToDelete = selectQuery.value("filename").toString();
        qDebug() << "VectorDAO::deleteVector - Found filename to delete:" << filenameToDelete << "for vector ID:" << id;
    }
    else
    {
        qDebug() << "VectorDAO::deleteVector - No vector found with ID:" << id << "Cannot determine filename to delete.";
        // Record doesn't exist, consider it a "successful" deletion in terms of state.
        // Or return false if strict "something was deleted" is required.
        // For now, let's say if no record, nothing to delete, so operation is "complete" in a sense.
        return true;
    }

    // Now, delete the database record
    QSqlQuery deleteQuery(db);
    deleteQuery.prepare("DELETE FROM vectors WHERE id = :id");
    deleteQuery.bindValue(":id", id);

    if (!deleteQuery.exec())
    {
        qDebug() << "VectorDAO::deleteVector - SQL Error (delete record):" << deleteQuery.lastError().text();
        return false; // Database record deletion failed
    }

    bool dbRecordDeleted = deleteQuery.numRowsAffected() > 0;
    if (!dbRecordDeleted)
    {
        qDebug() << "VectorDAO::deleteVector - No database record was deleted for ID (though it was found earlier):" << id;
        // This case should ideally not happen if selectQuery found it.
        // Still, if it occurs, the file won't be deleted yet.
        // Consider what to do: if DB record not deleted, should we proceed to delete file?
        // For safety, if DB deletion fails or reports 0 rows affected unexpectedly, perhaps don't delete file.
        return false;
    }

    qDebug() << "VectorDAO::deleteVector - Successfully deleted database record for ID:" << id;

    // Delete the binary file from the filesystem
    if (!filenameToDelete.isEmpty())
    {
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString filePathToDelete = appDataPath + QDir::separator() + filenameToDelete;
        if (FileUtils::deleteBinaryFile(filePathToDelete))
        {
            qDebug() << "VectorDAO::deleteVector - Successfully deleted binary file:" << filePathToDelete;
        }
        else
        {
            qDebug() << "VectorDAO::deleteVector - Failed to delete binary file:" << filePathToDelete << ". Database record was deleted.";
            // This is a state of inconsistency. Logged, but function might return true as DB part succeeded.
            // Or return false to indicate overall operation failure.
            // For now, let's prioritize DB success.
            // Consider a more robust cleanup mechanism for orphaned files if this is critical.
        }
    }
    else
    {
        qDebug() << "VectorDAO::deleteVector - Filename was empty, no binary file to delete for vector ID:" << id;
    }

    qDebug() << "VectorDAO::deleteVector - Exit. DB record deleted:" << dbRecordDeleted;
    return dbRecordDeleted; // Returns true if the database record was deleted.
}

QList<Vector> VectorDAO::getAllVectorsByCollectionId(int collectionId)
{
    qDebug() << "VectorDAO::getAllVectorsByCollectionId - Entry. Collection ID:" << collectionId;
    QList<Vector> vectors;
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);
    query.prepare("SELECT id, name, metadata, filename, collection_id FROM vectors WHERE collection_id = :collection_id ORDER BY created_at DESC");
    query.bindValue(":collection_id", collectionId);

    if (!query.exec())
    {
        qDebug() << "VectorDAO::getAllVectorsByCollectionId - SQL Error:" << query.lastError().text();
        return vectors; // Return empty list on error
    }

    while (query.next())
    {
        Vector vector;
        vector.id = query.value("id").toInt();
        vector.name = query.value("name").toString();
        vector.metadata = query.value("metadata").toString();
        vector.filename = query.value("filename").toString(); // Filename only
        vector.collectionId = query.value("collection_id").toInt();
        // Binary data is not loaded here for performance; load on demand when a specific vector is accessed.
        vectors.append(vector);
        qDebug() << "VectorDAO::getAllVectorsByCollectionId - Fetched vector. ID:" << vector.id << "Name:" << vector.name << "Filename:" << vector.filename;
    }
    qDebug() << "VectorDAO::getAllVectorsByCollectionId - Exit. Found" << vectors.size() << "vectors for collection ID:" << collectionId;
    return vectors;
}

QList<Vector> VectorDAO::getAllVectors()
{
    qDebug() << "VectorDAO::getAllVectors - Entry.";
    QList<Vector> vectors;
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);
    query.prepare("SELECT id, name, metadata, filename, collection_id FROM vectors ORDER BY created_at DESC");

    if (!query.exec())
    {
        qDebug() << "VectorDAO::getAllVectors - SQL Error:" << query.lastError().text();
        return vectors; // Return empty list on error
    }

    while (query.next())
    {
        Vector vector;
        vector.id = query.value("id").toInt();
        vector.name = query.value("name").toString();
        vector.metadata = query.value("metadata").toString();
        vector.filename = query.value("filename").toString(); // Filename only
        vector.collectionId = query.value("collection_id").toInt();
        // Binary data is not loaded here
        vectors.append(vector);
        qDebug() << "VectorDAO::getAllVectors - Fetched vector. ID:" << vector.id << "Name:" << vector.name << "Filename:" << vector.filename;
    }
    qDebug() << "VectorDAO::getAllVectors - Exit. Found" << vectors.size() << "vectors in total.";
    return vectors;
}
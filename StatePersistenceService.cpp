#include "StatePersistenceService.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <sqlite3.h>

StatePersistenceService::StatePersistenceService(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("Не удалось открыть БД: " + std::string(sqlite3_errmsg(db)));
    }
}

StatePersistenceService::~StatePersistenceService() {
    if (db) sqlite3_close(db);
}

void StatePersistenceService::initializeSchema() {
    const std::string drop = R"SQL(
        drop table file_changes;
        drop table tracking_files;
    )SQL";
    execute(drop); //DEBUG DROP ONLY, SHOULD BE DELETED

    const std::string sql = R"SQL(
        CREATE TABLE IF NOT EXISTS tracking_files (
            file_id integer PRIMARY KEY AUTOINCREMENT, 
            file_path TEXT NOT NULL,
            last_checksum TEXT,
            is_missing INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS file_changes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_id integer NOT NULL,
            timestamp TEXT NOT NULL,
            change_type TEXT NOT NULL,
            checksum TEXT,
            saved_version_id TEXT,
            user TEXT,
            additional_info TEXT,
            FOREIGN KEY (file_id) REFERENCES tracking_files(file_id)
        );
    )SQL";
    execute(sql);
}

void StatePersistenceService::execute(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        throw std::runtime_error("SQL error: " + msg);
    }
}

void StatePersistenceService::createTrackingFile(TrackingFile& file) {
    const std::string sql = "INSERT INTO tracking_files (file_path, last_checksum, is_missing) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, file.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, file.lastChecksum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, file.isMissing ? 1 : 0);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Получаем автоинкрементный ID
    sqlite3_int64 rowId = sqlite3_last_insert_rowid(db);
    file.fileId = std::to_string(rowId);  // сохраняем в структуру для дальнейшего использования

    for (const auto& change : file.history.changes) {
        saveFileChange(file.fileId, change);
    }
}

void StatePersistenceService::saveTrackingFile(const TrackingFile& file) {
    const std::string sql = "INSERT OR REPLACE INTO tracking_files (file_id, file_path, last_checksum, is_missing) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, file.fileId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, file.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, file.lastChecksum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, file.isMissing ? 1 : 0);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    for (const auto& change : file.history.changes) {
        saveFileChange(file.fileId, change);
    }
}


void StatePersistenceService::saveFileChange(const std::string& fileId, const FileChange& change) {
    const std::string sql = "INSERT INTO file_changes (file_id, timestamp, change_type, checksum, saved_version_id, user, additional_info) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, fileId.c_str(), -1, SQLITE_TRANSIENT);
    std::string ts = toIsoString(change.timestamp);
    sqlite3_bind_text(stmt, 2, ts.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, change.changeType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, change.checksum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, change.savedVersionId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, change.user.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, change.additionalInfo.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::string StatePersistenceService::toIsoString(const std::chrono::system_clock::time_point& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&t), "%FT%TZ");
    return ss.str();
}

std::chrono::system_clock::time_point StatePersistenceService::fromIsoString(const std::string& str) {
    std::tm tm{};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// Метод loadTrackedFiles будет добавлен по запросу
std::vector<TrackingFile> StatePersistenceService::loadTrackedFiles() {
    std::vector<TrackingFile> files;

    const std::string sql = "SELECT file_id, file_path, last_checksum, is_missing FROM tracking_files;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Ошибка подготовки запроса к tracking_files");
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TrackingFile file;
        file.fileId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        file.filePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        file.lastChecksum = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        file.isMissing = sqlite3_column_int(stmt, 3) != 0;

        // Загружаем изменения для файла
        const std::string changesSql = "SELECT timestamp, change_type, checksum, saved_version_id, user, additional_info FROM file_changes WHERE file_id = ? ORDER BY timestamp ASC;";
        sqlite3_stmt* changesStmt;
        sqlite3_prepare_v2(db, changesSql.c_str(), -1, &changesStmt, nullptr);
        sqlite3_bind_text(changesStmt, 1, file.fileId.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(changesStmt) == SQLITE_ROW) {
            FileChange change;
            change.timestamp = fromIsoString(reinterpret_cast<const char*>(sqlite3_column_text(changesStmt, 0)));
            change.changeType = reinterpret_cast<const char*>(sqlite3_column_text(changesStmt, 1));
            change.checksum = reinterpret_cast<const char*>(sqlite3_column_text(changesStmt, 2));
            change.savedVersionId = reinterpret_cast<const char*>(sqlite3_column_text(changesStmt, 3));
            change.user = reinterpret_cast<const char*>(sqlite3_column_text(changesStmt, 4));
            change.additionalInfo = reinterpret_cast<const char*>(sqlite3_column_text(changesStmt, 5));
            file.history.changes.push_back(change);
        }

        sqlite3_finalize(changesStmt);
        files.push_back(file);
    }

    sqlite3_finalize(stmt);
    return files;
}


void StatePersistenceService::updateTrackingFileChecksum(const std::string& fileId, const std::string& newChecksum) {
    const std::string sql = "UPDATE tracking_files SET last_checksum = ? WHERE file_id = ?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, newChecksum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, fileId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void StatePersistenceService::updateTrackingFileMissing(const std::string& fileId, bool isMissing) {
    const std::string sql = "UPDATE tracking_files SET is_missing = ? WHERE file_id = ?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, isMissing ? 1 : 0);
    sqlite3_bind_text(stmt, 2, fileId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}
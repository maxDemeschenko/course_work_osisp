#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>
#include "TrackingFile.hpp"


class StatePersistenceService {
public:
    StatePersistenceService(const std::string& dbPath);
    
    void initializeSchema(); // Создание таблиц при первом запуске
    void createTrackingFile(TrackingFile& file);
    void saveTrackingFile(const TrackingFile& file);
    void saveFileChange(const std::string& fileId, const FileChange& change);

    std::vector<TrackingFile> loadTrackedFiles(); // Восстановление состояния

    ~StatePersistenceService();

private:
    std::string toIsoString(const std::chrono::system_clock::time_point& tp);
    std::chrono::system_clock::time_point fromIsoString(const std::string& str);
    void execute(const std::string& sql);


    sqlite3* db;
};

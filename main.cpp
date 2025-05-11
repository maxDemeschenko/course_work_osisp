#include <filesystem>
#include <iostream>
#include <vector>
#include <memory>
#include "ConfigLoader.hpp"
#include "VaultService.hpp"
#include "ChecksumService.hpp"
#include "InitializationService.hpp"
#include "TrackingFile.hpp"
#include "StatePersistenceService.hpp"
#include "InotifyWatcher.hpp"

void processFile(const std::filesystem::path& filePath,
                 InitializationService& initializer,
                 ChecksumService& checksum,
                 VaultService& vault,
                 StatePersistenceService& dbService,
                 std::vector<TrackingFile>& trackedFilesFromDb,
                 std::vector<TrackingFile>& trackedFilesOut) {
    try {
        auto it = std::find_if(trackedFilesFromDb.begin(), trackedFilesFromDb.end(),
                               [&](const TrackingFile& file) {
                                   return file.filePath == filePath;
                               });

        if (it == trackedFilesFromDb.end()) {
            // Новый файл — инициализируем и сохраняем
            TrackingFile tf = initializer.initialize(filePath.string());
            trackedFilesOut.push_back(tf);
            dbService.createTrackingFile(tf);
            std::cout << "  → Инициализирован новый файл: " << filePath << std::endl;
        } else {
            // Файл уже есть — проверим хеш
            TrackingFile file = *it;  // Копия, чтобы можно было модифицировать
            std::string currentChecksum = checksum.compute(file.filePath);

            // Проверка: существует ли резерв с совпадающим хешем
            bool hasBackup = false;
            for (const auto& change : file.history.changes) {
                if (currentChecksum == file.lastChecksum && vault.exists(change.savedVersionId)) {
                    hasBackup = true;
                    break;
                }
            }

            if (!hasBackup && !file.lastChecksum.empty()) {
                std::cout << "  ⚠ Резервная копия отсутствует, создаём заново..." << std::endl;
                
                FileChange change = FileChange();
                std::string restoredId = vault.save(file.filePath); // Сохраняем с тем же ID
                change.savedVersionId = restoredId;
                change.changeType = "Restore of reserve copy";
                change.checksum = checksum.compute(file.filePath);
                change.timestamp = std::chrono::system_clock::now();
                dbService.saveFileChange(file.fileId, change);

                std::cout << "  ✔ Резервная копия восстановлена: " << restoredId << std::endl;
            }

            trackedFilesOut.push_back(file);
        }
    } catch (const std::exception& ex) {
        std::cerr << "  ⚠ Ошибка обработки файла " << filePath << ": " << ex.what() << std::endl;
    }
}


void processDirectory(const std::filesystem::path& dirPath,
                      InitializationService& initializer,
                      ChecksumService& checksum,
                      VaultService& vault,
                      StatePersistenceService& dbService,
                      std::vector<TrackingFile>& trackedFilesFromDb,
                      std::vector<TrackingFile>& trackedFilesOut,
                      bool recursive) {
    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
        std::cerr << "Путь не является директорией: " << dirPath << std::endl;
        return;
    }

    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
                if (std::filesystem::is_regular_file(entry)) {
                    processFile(entry.path(), initializer, checksum, vault, dbService, trackedFilesFromDb, trackedFilesOut);
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
                if (std::filesystem::is_regular_file(entry)) {
                    processFile(entry.path(), initializer, checksum, vault, dbService, trackedFilesFromDb, trackedFilesOut);
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& fe) {
        std::cerr << "  ⚠ Ошибка обхода директории " << dirPath << ": " << fe.what() << std::endl;
    }
}

std::vector<TrackingFile> loadAndProcessConfiguration(const std::string& configPath,
                                                      InitializationService& initializer,
                                                      ChecksumService& checksum,
                                                      VaultService& vault,
                                                      StatePersistenceService& dbService) {
    ConfigLoader loader(configPath);
    if (!loader.load()) {
        throw std::runtime_error("Ошибка загрузки конфигурации!");
    }

    const auto& groups = loader.getMonitoringGroups();
    std::cout << "Загружено групп: " << groups.size() << std::endl;

    std::vector<TrackingFile> trackedFilesFromDb = dbService.loadTrackedFiles();
    std::vector<TrackingFile> trackedFiles;

    for (const auto& group : groups) {
        std::cout << "Группа ID: " << group.id << "\nОписание: " << group.description << std::endl;

        for (const auto& path : group.paths) {
            if (std::filesystem::is_regular_file(path.path)) {
                processFile(path.path, initializer, checksum, vault, dbService, trackedFilesFromDb, trackedFiles);
            }
            else if (std::filesystem::is_directory(path.path)) {
                std::cout << "  → Инициализация директории: " << path.path << std::endl;
                processDirectory(path.path, initializer, checksum, vault, dbService, trackedFilesFromDb, trackedFiles, path.recursive);
            }
        }

        std::cout << "-------------------------------" << std::endl;
    }

    return trackedFiles;
}


int main() {
    const std::string configPath = "config.json";

    VaultService vault(".filevault");
    ChecksumService checksum;
    InitializationService initializer(vault, checksum);
    StatePersistenceService dbService("tracking.db");
    dbService.initializeSchema();

    InotifyWatcher watcher;
    std::vector<TrackingFile> trackedFiles;

    auto setupFileWatchers = [&](std::vector<TrackingFile>& files) {
        for (auto& file : files) {
            watcher.addWatch(file.filePath, [&](uint32_t mask) {
                std::cout << "📝 Изменение файла: " << file.filePath << std::endl;

                if (mask & IN_MODIFY) {
                    std::cout << "  → Файл модифицирован. Пересчитываем хеш..." << std::endl;
                    try {
                        std::string newChecksum = checksum.compute(file.filePath);
                        if (newChecksum != file.lastChecksum) {
                            FileChange change;
                            change.timestamp = std::chrono::system_clock::now();
                            change.checksum = newChecksum;
                            change.savedVersionId = vault.save(file.filePath);
                            dbService.saveFileChange(file.fileId, change);
                            file.lastChecksum = newChecksum;
                            dbService.updateTrackingFileChecksum(file.fileId, newChecksum);
                            std::cout << "  ✔ Резервная копия сохранена и хеш обновлён." << std::endl;
                        } else {
                            std::cout << "  ↪ Хеш не изменился" << std::endl;
                        }
                    } catch (const std::exception& ex) {
                        std::cerr << "  ⚠ Ошибка обновления контрольной суммы: " << ex.what() << std::endl;
                    }
                }

                if (mask & IN_DELETE) {
                    std::cout << "  ⚠ Файл был удалён" << std::endl;
                    file.isMissing = true;
                    dbService.updateTrackingFileMissing(file.fileId, true);
                }
            });
        }
    };

    std::function<void()> reloadConfiguration = [&]() {
        std::cout << "\n🔄 Перезагрузка конфигурации..." << std::endl;
        try {
            // Удаляем все текущие наблюдения
            watcher.clearWatches();

            // Перезагружаем конфигурацию и отслеживаемые файлы
            trackedFiles = loadAndProcessConfiguration(configPath, initializer, checksum, vault, dbService);
            setupFileWatchers(trackedFiles);

            // Добавляем наблюдение за изменением конфигурации
            watcher.addWatch(configPath, [&](uint32_t mask) {
                if (mask & IN_MODIFY) {
                    reloadConfiguration();
                }
            });

            std::cout << "✔ Конфигурация обновлена. Отслеживаемых файлов: " << trackedFiles.size() << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "  ⚠ Ошибка при обновлении конфигурации: " << ex.what() << std::endl;
        }
    };

    // Инициализация
    reloadConfiguration();
    watcher.start();

    std::cout << "Нажмите Enter для выхода..." << std::endl;
    std::cin.get();

    watcher.stop();
    return 0;
}

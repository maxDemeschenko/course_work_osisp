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

void processDirectory(const std::filesystem::path& dirPath, InitializationService& initializer, std::vector<TrackingFile>& trackedFiles, bool recursive) {
    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
        std::cerr << "Путь не является директорией: " << dirPath << std::endl;
        return;
    }

    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
                if (std::filesystem::is_regular_file(entry)) {
                    try {
                        TrackingFile tf = initializer.initialize(entry.path().string());
                        trackedFiles.push_back(tf);
                        std::cout << "  → Инициализирован файл: " << entry.path() << std::endl;
                    } catch (const std::exception& ex) {
                        std::cerr << "  ⚠ Ошибка инициализации файла " << entry.path()
                                  << ": " << ex.what() << std::endl;
                    }
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
                if (std::filesystem::is_regular_file(entry)) {
                    try {
                        TrackingFile tf = initializer.initialize(entry.path().string());
                        trackedFiles.push_back(tf);
                        std::cout << "  → Инициализирован файл: " << entry.path() << std::endl;
                    } catch (const std::exception& ex) {
                        std::cerr << "  ⚠ Ошибка инициализации файла " << entry.path()
                                  << ": " << ex.what() << std::endl;
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& fe) {
        std::cerr << "  ⚠ Ошибка обхода директории " << dirPath << ": " << fe.what() << std::endl;
    }
}


int main() {
    const std::string configPath = "config.json";

    ConfigLoader loader(configPath);
    if (!loader.load()) {
        std::cerr << "Ошибка загрузки конфигурации!" << std::endl;
        return 1;
    }

    const auto& groups = loader.getMonitoringGroups();
    std::cout << "Загружено групп: " << groups.size() << std::endl;

    VaultService vault(".filevault");
    ChecksumService checksum;
    InitializationService initializer(vault, checksum);

    std::vector<TrackingFile> trackedFiles;

    for (const auto& group : groups) {
        std::cout << "Группа ID: " << group.id << std::endl;
        std::cout << "Описание: " << group.description << std::endl;

        for (const auto& path : group.paths) {
            if (std::filesystem::is_regular_file(path.path)) {
                // Если путь — файл
                try {
                    TrackingFile tf = initializer.initialize(path.path);
                    trackedFiles.push_back(tf);
                    std::cout << "  → Инициализирован файл: " << path.path << std::endl;
                } catch (const std::exception& ex) {
                    std::cerr << "  ⚠ Ошибка инициализации файла " << path.path
                              << ": " << ex.what() << std::endl;
                }
            }
            else if (std::filesystem::is_directory(path.path)) {
                // Если путь — директория и нужно обрабатывать рекурсивно
                std::cout << "  → Инициализация директории: " << path.path << std::endl;
                processDirectory(path.path, initializer, trackedFiles, path.recursive);
            }
        }

        std::cout << "-------------------------------" << std::endl;
    }

    std::cout << "Отслеживаемых файлов: " << trackedFiles.size() << std::endl;

    StatePersistenceService dbService("tracking.db");
    dbService.initializeSchema();

    for (auto& file : trackedFiles)
    {
        dbService.createTrackingFile(file);
    }

    InotifyWatcher watcher;

    for (auto& file : trackedFiles) {
        watcher.addWatch(file.filePath, [&](uint32_t mask) {
            std::cout << "📝 Изменение файла: " << file.filePath << std::endl;
            std::cout << mask;

            if (mask & IN_MODIFY) {
                std::cout << "  → Файл модифицирован. Пересчитываем хеш..." << std::endl;
                try {
                    std::string newChecksum = checksum.compute(file.filePath);
                    std::string oldChecksum = file.lastChecksum;

                    if (newChecksum != oldChecksum) {
                        FileChange change;
                        change.timestamp = std::chrono::system_clock::now();
                        change.checksum = newChecksum;
                        change.savedVersionId = vault.save(file.filePath);

                        file.lastChecksum = newChecksum;
                        file.history.changes.push_back(change);

                        vault.save(file.filePath);  // сохранить старую версию
                        dbService.updateTrackingFileChecksum(file.fileId, newChecksum);
                        dbService.saveFileChange(file.fileId, change);

                        std::cout << "  ✔ Хеш обновлён и сохранён в БД" << std::endl;
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

    watcher.start();

    // Например, подождать на enter чтобы завершить
    std::cout << "Нажмите Enter для выхода..." << std::endl;
    std::cin.get();

    watcher.stop();


    return 0;
}

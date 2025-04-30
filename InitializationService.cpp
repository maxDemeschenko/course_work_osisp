// InitializationService.cpp
#include "InitializationService.hpp"
#include <stdexcept>

InitializationService::InitializationService(VaultService& vault, ChecksumService& checksum)
    : vault(vault), checksum(checksum) {}

TrackingFile InitializationService::initialize(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        throw std::runtime_error("Файл не найден: " + filePath);
    }

    // Создание версии и вычисление контрольной суммы
    std::string checksumValue = checksum.compute(filePath);
    std::string versionId = vault.save(filePath);

    // Создание и возвращение TrackingFile
    TrackingFile file;
    file.filePath = filePath;
    file.lastChecksum = checksumValue;

    // Создаем FileChange для первого сохранения
    FileChange initialChange;
    initialChange.timestamp = std::chrono::system_clock::now();
    initialChange.changeType = "INITIAL";
    initialChange.checksum = checksumValue;
    initialChange.savedVersionId = versionId;

    file.history.changes.push_back(initialChange);

    return file;
}

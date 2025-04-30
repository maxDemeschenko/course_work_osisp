// VaultService.cpp
#include "VaultService.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>

VaultService::VaultService(std::filesystem::path vaultRoot)
    : vaultDir(vaultRoot) {
    if (!std::filesystem::exists(vaultDir)) {
        std::filesystem::create_directory(vaultDir);
    }
}

std::string VaultService::save(const std::filesystem::path& filePath) {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 15);
    std::ostringstream versionId;

    // Генерация случайного versionId (UUID-like)
    for (int i = 0; i < 8; ++i) {
        versionId << std::hex << dist(rd);
    }

    std::filesystem::path destination = vaultDir / versionId.str();
    std::filesystem::copy(filePath, destination, std::filesystem::copy_options::overwrite_existing);

    return versionId.str();
}

bool VaultService::restore(const std::string& versionId, const std::filesystem::path& destination) {
    std::filesystem::path source = vaultDir / versionId;
    if (std::filesystem::exists(source)) {
        std::filesystem::copy(source, destination, std::filesystem::copy_options::overwrite_existing);
        return true;
    }
    return false;
}

bool VaultService::exists(const std::string& versionId) const {
    std::filesystem::path versionPath = vaultDir / versionId;
    return std::filesystem::exists(versionPath);
}

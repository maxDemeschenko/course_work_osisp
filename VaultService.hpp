#pragma once

#include <string>
#include <filesystem>

class VaultService {
public:
    VaultService(std::filesystem::path vaultRoot);

    std::string save(const std::filesystem::path& filePath); // returns versionId
    bool restore(const std::string& versionId, const std::filesystem::path& destination);
    bool exists(const std::string& versionId) const;

private:
    std::filesystem::path vaultDir;
};
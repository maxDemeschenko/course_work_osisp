#pragma once

#include <string>
#include <filesystem>

class ChecksumService {
public:
    static std::string compute(const std::filesystem::path& filePath);
};

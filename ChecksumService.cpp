// ChecksumService.cpp
#include "ChecksumService.hpp"
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iomanip>

std::string ChecksumService::compute(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Не удалось открыть файл для вычисления контрольной суммы");
    }

    SHA256_CTX sha256Context;
    SHA256_Init(&sha256Context);

    char buffer[1024];
    while (file.read(buffer, sizeof(buffer))) {
        SHA256_Update(&sha256Context, buffer, file.gcount());
    }
    SHA256_Update(&sha256Context, buffer, file.gcount());  // Для последней части

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256Context);

    std::ostringstream result;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        result << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return result.str();
}

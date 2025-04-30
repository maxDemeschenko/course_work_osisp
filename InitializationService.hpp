#pragma once

#include "VaultService.hpp"
#include "ChecksumService.hpp"
#include "TrackingFile.hpp"
#include <string>

class InitializationService {
public:
    InitializationService(VaultService& vault, ChecksumService& checksum);

    TrackingFile initialize(const std::string& filePath);

private:
    VaultService& vault;
    ChecksumService& checksum;
};

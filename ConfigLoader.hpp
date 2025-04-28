// ConfigLoader.h
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

struct PathConfig {
    std::string path;
    bool recursive;
};

struct ChecksumConfig {
    bool enabled;
    std::string algorithm;
};

struct MonitoringGroup {
    std::string id;
    std::string description;
    std::vector<PathConfig> paths;
    std::vector<std::string> events;
    ChecksumConfig checksum;
};

class ConfigLoader {
public:
    explicit ConfigLoader(const std::string& configPath);

    bool load(); // Загрузка конфига
    const std::vector<MonitoringGroup>& getMonitoringGroups() const;

private:
    std::string m_configPath;
    std::vector<MonitoringGroup> m_monitoringGroups;

    void parse(const nlohmann::json& root); // Разбор JSON
};

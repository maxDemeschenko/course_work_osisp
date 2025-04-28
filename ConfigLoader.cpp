#include "ConfigLoader.hpp"
#include <fstream>
#include "nlohmann/json.hpp"

ConfigLoader::ConfigLoader(const std::string& configPath)
    : m_configPath(configPath) {}

bool ConfigLoader::load() {
    std::ifstream file(m_configPath);
    if (!file.is_open()) {
        return false;
    }

    nlohmann::json root;
    try {
        file >> root;
        parse(root);
    } catch (const nlohmann::json::exception& ex) {
        // Можно логировать ошибку
        return false;
    }

    return true;
}

void ConfigLoader::parse(const nlohmann::json& root) {
    m_monitoringGroups.clear();
    auto groups = root.at("monitoring").at("groups");

    for (const auto& group : groups) {
        MonitoringGroup mg;
        mg.id = group.at("id").get<std::string>();
        mg.description = group.at("description").get<std::string>();

        for (const auto& pathObj : group.at("paths")) {
            PathConfig pc;
            pc.path = pathObj.at("path").get<std::string>();
            pc.recursive = pathObj.at("recursive").get<bool>();
            mg.paths.push_back(pc);
        }

        mg.events = group.at("events").get<std::vector<std::string>>();

        auto checksumObj = group.at("checksum");
        mg.checksum.enabled = checksumObj.at("enabled").get<bool>();
        if (mg.checksum.enabled) {
            mg.checksum.algorithm = checksumObj.at("algorithm").get<std::string>();
        }

        m_monitoringGroups.push_back(mg);
    }
}

const std::vector<MonitoringGroup>& ConfigLoader::getMonitoringGroups() const {
    return m_monitoringGroups;
}

// main.cpp
#include <iostream>
#include "ConfigLoader.hpp"

int main() {
    const std::string configPath = "config.json"; // Путь до файла конфигурации

    ConfigLoader loader(configPath);

    if (!loader.load()) {
        std::cerr << "Ошибка загрузки конфигурации!" << std::endl;
        return 1;
    }

    const auto& groups = loader.getMonitoringGroups();

    std::cout << "Загружено групп: " << groups.size() << std::endl;

    for (const auto& group : groups) {
        std::cout << "Группа ID: " << group.id << std::endl;
        std::cout << "Описание: " << group.description << std::endl;

        std::cout << "Пути:" << std::endl;
        for (const auto& path : group.paths) {
            std::cout << "  - " << path.path 
                      << " (рекурсивно: " << (path.recursive ? "да" : "нет") << ")" 
                      << std::endl;
        }

        std::cout << "Отслеживаемые события:" << std::endl;
        for (const auto& event : group.events) {
            std::cout << "  - " << event << std::endl;
        }

        std::cout << "Проверка контрольной суммы: " 
                  << (group.checksum.enabled ? "включена" : "отключена") << std::endl;
        if (group.checksum.enabled) {
            std::cout << "Алгоритм контрольной суммы: " << group.checksum.algorithm << std::endl;
        }

        std::cout << "-------------------------------" << std::endl;
    }

    return 0;
}

#pragma once
#include "../core/app_settings.hpp"
#include <string>

class SettingsIO {
public:
    static void Load(const std::string& filepath);
    static void Save(const std::string& filepath);
};

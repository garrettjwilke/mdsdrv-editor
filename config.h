#ifndef CONFIG_H
#define CONFIG_H

#include <filesystem>

struct UserConfig {
    int theme = 0;           // 0=Dark,1=Light,2=Classic
    int windowWidth = 1280;
    int windowHeight = 720;
};

std::filesystem::path GetUserConfigPath();
UserConfig LoadUserConfig();
void SaveUserConfig(const UserConfig& config);

#endif // CONFIG_H


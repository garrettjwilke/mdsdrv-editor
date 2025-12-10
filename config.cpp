#include "config.h"

#include <cstdlib>
#include <fstream>
#include <string>

namespace {
std::filesystem::path GetConfigDir() {
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".config" / "mdsdrv-editor";
    }
    return std::filesystem::temp_directory_path() / "mdsdrv-editor";
}

int ClampDimension(int value) {
    // Keep reasonable defaults to avoid tiny or zero-sized windows
    const int minSize = 320;
    const int maxSize = 10000;
    if (value < minSize) return minSize;
    if (value > maxSize) return maxSize;
    return value;
}

float ClampUiScale(float value) {
    const float minScale = 0.5f;
    const float maxScale = 3.0f;
    if (value < minScale) return minScale;
    if (value > maxScale) return maxScale;
    return value;
}
} // namespace

std::filesystem::path GetUserConfigPath() {
    try {
        auto dir = GetConfigDir();
        std::filesystem::create_directories(dir);
        return dir / "config.ini";
    } catch (...) {
        // Fall back to a temp directory if anything goes wrong
        return std::filesystem::temp_directory_path() / "mdsdrv-editor" / "config.ini";
    }
}

UserConfig LoadUserConfig() {
    UserConfig config;
    try {
        auto path = GetUserConfigPath();
        std::ifstream in(path);
        if (!in.is_open()) {
            return config;
        }

        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("theme=", 0) == 0) {
                int value = std::stoi(line.substr(6));
                if (value >= 0 && value <= 2) {
                    config.theme = value;
                }
            } else if (line.rfind("window_width=", 0) == 0) {
                int value = std::stoi(line.substr(13));
                if (value > 0) {
                    config.windowWidth = ClampDimension(value);
                }
            } else if (line.rfind("window_height=", 0) == 0) {
                int value = std::stoi(line.substr(14));
                if (value > 0) {
                    config.windowHeight = ClampDimension(value);
                }
            } else if (line.rfind("ui_scale=", 0) == 0) {
                float value = std::stof(line.substr(9));
                config.uiScale = ClampUiScale(value);
            }
        }
    } catch (...) {
        // Ignore config load errors and stick with defaults
    }
    return config;
}

void SaveUserConfig(const UserConfig& config) {
    try {
        auto path = GetUserConfigPath();
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open()) {
            return;
        }

        // Write all known keys so we don't discard data when another
        // component updates a single field.
        out << "theme=" << config.theme << "\n";
        out << "window_width=" << ClampDimension(config.windowWidth) << "\n";
        out << "window_height=" << ClampDimension(config.windowHeight) << "\n";
        out << "ui_scale=" << ClampUiScale(config.uiScale) << "\n";
    } catch (...) {
        // Ignore save errors to avoid crashing the UI over config persistence
    }
}


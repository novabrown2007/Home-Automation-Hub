#include "configloader.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace {
using Json = nlohmann::json;

std::filesystem::path resolveConfigPath(const std::filesystem::path& requestedPath) {
    if (requestedPath.is_absolute()) {
        return requestedPath;
    }

    const std::filesystem::path current = std::filesystem::current_path();
    const std::filesystem::path candidates[] = {
        requestedPath,
        current / requestedPath,
        current / ".." / requestedPath,
        current.parent_path() / requestedPath
    };

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate;
        }
    }

    return current / requestedPath;
}
}

std::filesystem::path configloader::defaultPath() {
    return "config/config.json";
}

configloader::Config configloader::load() {
    return load(defaultPath());
}

configloader::Config configloader::load(const std::filesystem::path& path) {
    const std::filesystem::path resolvedPath = resolveConfigPath(path);
    Config config{};

    std::ifstream input(resolvedPath);
    if (!input.is_open()) {
        return config;
    }

    Json document;
    input >> document;

    if (document.contains("bridgePort")) {
        config.bridgePort = document.at("bridgePort").get<int>();
        if (config.bridgePort <= 0 || config.bridgePort > 65535) {
            throw std::runtime_error("Config bridgePort must be between 1 and 65535.");
        }
    }

    if (document.contains("hubPort")) {
        config.hubPort = document.at("hubPort").get<int>();
        if (config.hubPort <= 0 || config.hubPort > 65535) {
            throw std::runtime_error("Config hubPort must be between 1 and 65535.");
        }
    }

    return config;
}

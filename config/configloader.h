#ifndef HOME_AUTOMATION_HUB_CONFIGLOADER_H
#define HOME_AUTOMATION_HUB_CONFIGLOADER_H

#include <filesystem>

class configloader {
public:
    struct Config {
        int bridgePort{8083};
        int hubPort{8080};
    };

    static Config load();
    static Config load(const std::filesystem::path& path);

private:
    static std::filesystem::path defaultPath();
};


#endif //HOME_AUTOMATION_HUB_CONFIGLOADER_H

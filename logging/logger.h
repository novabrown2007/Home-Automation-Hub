#pragma once

#include <fstream>
#include <mutex>
#include <string>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    static Logger& instance();

    void initialize(const std::string& filePath);

    void debug(const std::string& component, const std::string& message);
    void info(const std::string& component, const std::string& message);
    void warning(const std::string& component, const std::string& message);
    void error(const std::string& component, const std::string& message);

private:
    Logger() = default;

    void log(LogLevel level, const std::string& component, const std::string& message);
    static std::string levelToString(LogLevel level);
    static std::string timestamp();

    std::mutex mutex;
    std::ofstream fileStream;
    bool initialized{false};
};

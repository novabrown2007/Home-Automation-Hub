#include "logger.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::initialize(const std::string& filePath) {
    {
        std::lock_guard lock(mutex);
        if (initialized) {
            return;
        }

        try {
            const std::filesystem::path path(filePath);
            if (path.has_parent_path()) {
                std::filesystem::create_directories(path.parent_path());
            }
            fileStream.open(filePath, std::ios::out | std::ios::app);
        } catch (...) {
            // Fall back to console-only logging.
        }

        initialized = true;
    }

    log(LogLevel::Info, "Logger", "Logging initialized at " + filePath);
}

void Logger::debug(const std::string& component, const std::string& message) {
    log(LogLevel::Debug, component, message);
}

void Logger::info(const std::string& component, const std::string& message) {
    log(LogLevel::Info, component, message);
}

void Logger::warning(const std::string& component, const std::string& message) {
    log(LogLevel::Warning, component, message);
}

void Logger::error(const std::string& component, const std::string& message) {
    log(LogLevel::Error, component, message);
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
    std::lock_guard lock(mutex);

    std::ostringstream line;
    line << "[" << timestamp() << "]"
         << " [" << levelToString(level) << "]"
         << " [" << component << "] "
         << message;

    std::cout << line.str() << "\n";
    if (fileStream.is_open()) {
        fileStream << line.str() << "\n";
        fileStream.flush();
    }
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
        default:
            return "ERROR";
    }
}

std::string Logger::timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;

    std::ostringstream buffer;
    buffer << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
           << "." << std::setw(3) << std::setfill('0') << milliseconds.count();
    return buffer.str();
}

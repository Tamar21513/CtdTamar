#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace ctd::logging {

enum class LogLevel { Info, Warn, Error };

std::string toString(LogLevel level);

// Appends timestamped lines to a log file. If the file cannot be
// opened (e.g. the directory could not be created), the logger
// disables itself after printing one warning to stderr; callers do
// not need to check isEnabled() before logging.
class FileLogger {
public:
    explicit FileLogger(const std::filesystem::path& path);

    void log(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

    bool isEnabled() const;

private:
    std::mutex mutex_;
    std::ofstream stream_;
    bool enabled_ = false;
};

// Lazily-initialized logger shared by the whole client process,
// writing to logs/client.log relative to the current working
// directory (the executable directory; see main.cpp).
FileLogger& defaultLogger();

}  // namespace ctd::logging

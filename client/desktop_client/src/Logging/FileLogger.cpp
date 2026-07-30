#include "Logging/FileLogger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ctd::logging {
namespace {

std::string isoTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto milliseconds = std::chrono::duration_cast<
        std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm utcTime{};
#if defined(_WIN32)
    gmtime_s(&utcTime, &time);
#else
    gmtime_r(&time, &utcTime);
#endif

    std::ostringstream stream;
    stream << std::put_time(&utcTime, "%Y-%m-%dT%H:%M:%S");
    stream << '.' << std::setfill('0') << std::setw(3)
           << milliseconds.count();
    stream << 'Z';
    return stream.str();
}

}  // namespace

std::string toString(LogLevel level) {
    switch (level) {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
    }
    return "INFO";
}

FileLogger::FileLogger(const std::filesystem::path& path) {
    std::error_code directoryError;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(
            path.parent_path(), directoryError);
    }
    stream_.open(path, std::ios::out | std::ios::app);
    enabled_ = stream_.is_open();
    if (!enabled_) {
        std::cerr
            << "[FileLogger] Warning: could not open log file '"
            << path.string()
            << "'; file logging is disabled for this session."
            << std::endl;
    }
}

void FileLogger::log(LogLevel level, const std::string& message) {
    if (!enabled_) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    stream_ << isoTimestamp() << ' ' << toString(level) << ' '
            << message << '\n';
    stream_.flush();
}

void FileLogger::info(const std::string& message) {
    log(LogLevel::Info, message);
}

void FileLogger::warn(const std::string& message) {
    log(LogLevel::Warn, message);
}

void FileLogger::error(const std::string& message) {
    log(LogLevel::Error, message);
}

bool FileLogger::isEnabled() const {
    return enabled_;
}

FileLogger& defaultLogger() {
    static FileLogger instance{
        std::filesystem::path("logs") / "client.log"};
    return instance;
}

}  // namespace ctd::logging

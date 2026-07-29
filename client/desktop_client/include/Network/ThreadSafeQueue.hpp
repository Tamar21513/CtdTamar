#pragma once

#include <deque>
#include <mutex>
#include <optional>
#include <utility>

namespace ctd::network {

template <typename T>
class ThreadSafeQueue {
public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.push_back(std::move(value));
    }

    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (values_.empty()) {
            return std::nullopt;
        }
        T value = std::move(values_.front());
        values_.pop_front();
        return value;
    }

private:
    std::mutex mutex_;
    std::deque<T> values_;
};

}  // namespace ctd::network

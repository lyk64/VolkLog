#pragma once

#include <atomic>
#include <format>
#include <functional>
#include <iostream>
#include <mutex>
#include <string_view>

namespace Volk::Log {

enum class Level {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Off,
};

using Sink = std::function<void(Level level, std::string_view component, std::string_view message)>;

namespace detail {

    inline std::atomic<Level> current_level{ Level::Warning };
    inline std::mutex sink_mutex;
    inline Sink current_sink;

    constexpr std::string_view level_name(Level level) noexcept {
        switch (level) {
            case Level::Trace:   return "TRACE";
            case Level::Debug:   return "DEBUG";
            case Level::Info:    return "INFO";
            case Level::Warning: return "WARN";
            case Level::Error:   return "ERROR";
            case Level::Off:     return "OFF";
        }
        return "UNKNOWN";
    }

} // namespace detail

inline void set_level(Level level) noexcept {
    detail::current_level.store(level, std::memory_order_relaxed);
}

inline void set_sink(Sink sink) {
    std::lock_guard lock{ detail::sink_mutex };
    detail::current_sink = std::move(sink);
}

inline void emit(Level level, std::string_view component, std::string_view message) {
    std::lock_guard lock{ detail::sink_mutex };
    if (detail::current_sink) {
        detail::current_sink(level, component, message);
    } else {
        std::cerr << std::format("[{}] [{}] {}\n", detail::level_name(level), component, message);
    }
}

struct Logger {
    const char* component;

    [[nodiscard]] bool should_log(Level level) const noexcept {
        return static_cast<int>(level) >= static_cast<int>(detail::current_level.load(std::memory_order_relaxed));
    }

    template<typename... Args>
    void log(Level level, std::format_string<Args...> fmt, Args&&... args) const {
        if (!should_log(level))
            return;
        emit(level, component, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void trace(std::format_string<Args...> fmt, Args&&... args) const { log(Level::Trace, fmt, std::forward<Args>(args)...); }

    template<typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args) const { log(Level::Debug, fmt, std::forward<Args>(args)...); }

    template<typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) const { log(Level::Info, fmt, std::forward<Args>(args)...); }

    template<typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args) const { log(Level::Warning, fmt, std::forward<Args>(args)...); }

    template<typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) const { log(Level::Error, fmt, std::forward<Args>(args)...); }
};

} // namespace Volk::Log

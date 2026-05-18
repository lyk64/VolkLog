#pragma once

#include <atomic>
#include <chrono>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string_view>
#include <thread>

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

inline std::string format_entry(Level level, std::string_view component, std::string_view message) {
    auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    std::ostringstream tid;
    tid << std::this_thread::get_id();
    return std::format("[{:%H:%M:%S}] [{}] [{:<5}] [{}] {}",
        now, tid.str(), detail::level_name(level), component, message);
}

inline void set_level(Level level) noexcept {
    detail::current_level.store(level, std::memory_order_relaxed);
}

inline void set_sink(Sink sink) {
    std::lock_guard lock{ detail::sink_mutex };
    detail::current_sink = std::move(sink);
}

inline void emit(Level level, std::string_view component, std::string_view message) {
    if (static_cast<int>(level) < static_cast<int>(detail::current_level.load(std::memory_order_relaxed)))
        return;

    Sink sink_copy;
    {
        std::lock_guard lock{ detail::sink_mutex };
        sink_copy = detail::current_sink;
    }

    if (sink_copy) {
        sink_copy(level, component, message);
    } else {
        std::cerr << format_entry(level, component, message) << '\n';
    }
}

template<typename F>
auto make_logged_callable(const char* component, F&& f) {
    return [component, f = std::forward<F>(f)](auto&&... args) {
#ifdef _WIN32
        ULONG stack_guarantee = 64 * 1024;
        SetThreadStackGuarantee(&stack_guarantee);
#endif
        try {
            std::invoke(f, std::forward<decltype(args)>(args)...);
        } catch (const std::exception& e) {
            emit(Level::Error, component, std::format("unhandled exception: {}", e.what()));
        } catch (...) {
            emit(Level::Error, component, "unhandled unknown exception");
        }
    };
}

#ifdef _WIN32
namespace detail {
    constexpr std::string_view exception_name(DWORD code) noexcept {
        switch (code) {
            case EXCEPTION_ACCESS_VIOLATION:      return "access violation";
            case EXCEPTION_STACK_OVERFLOW:        return "stack overflow";
            case EXCEPTION_ILLEGAL_INSTRUCTION:   return "illegal instruction";
            case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "integer divide by zero";
            case EXCEPTION_INT_OVERFLOW:          return "integer overflow";
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
            default:                              return "unknown exception";
        }
    }
}

inline void install_crash_handler() {
    SetUnhandledExceptionFilter([](PEXCEPTION_POINTERS ep) noexcept -> LONG {
        const auto code = ep->ExceptionRecord->ExceptionCode;
        const auto addr = reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress);

        if (code == EXCEPTION_ACCESS_VIOLATION) {
            const auto access_addr = ep->ExceptionRecord->ExceptionInformation[1];
            emit(Level::Error, "crash", std::format("access violation at {:#x} accessing {:#x}", addr, access_addr));
        } else {
            emit(Level::Error, "crash", std::format("{} at {:#x}", detail::exception_name(code), addr));
        }

        return EXCEPTION_CONTINUE_SEARCH;
    });
}
#endif

inline Sink make_file_sink(const std::filesystem::path& path) {
    auto file = std::make_shared<std::ofstream>(path, std::ios::trunc);
    if (!file->is_open())
        throw std::runtime_error(std::format("VolkLog: failed to open log file: {}", path.string()));
    auto mtx = std::make_shared<std::mutex>();
    return [file, mtx](Level level, std::string_view component, std::string_view message) {
        auto entry = format_entry(level, component, message);
        std::lock_guard lock{ *mtx };
        *file << entry << '\n';
        file->flush();
    };
}

inline void set_sink(const std::filesystem::path& path) {
    set_sink(make_file_sink(path));
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

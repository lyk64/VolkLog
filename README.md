# VolkLog
A header-only, thread-safe logging library for C++20.

### Currently supports:
- **Logging**
  - Component-tagged `volk::log::Logger` with `std::format` style calls
  - Trace, debug, info, warning, and error levels with a global level filter
  - Entries stamped with time, thread ID, level, and component

- **Sinks**
  - Logs to `stderr` by default
  - File sink by path
  - Custom sinks through a callback

- **Crash reporting**
  - Unhandled SEH exception logging, including access violation addresses (Windows)
  - Callable wrapper that logs uncaught exceptions on worker threads

## Usage

VolkLog is header-only. Add it as a submodule and put `external\VolkLog\include` in your project's **Additional Include Directories**:

```cpp
#include <VolkLog/log.hh>

static constexpr volk::log::Logger logger{ "OVERLAY" };

int main() {
    volk::log::set_level(volk::log::Level::Info);
    volk::log::set_sink("app.log");
    volk::log::install_crash_handler();

    logger.info("Window created ({}x{})", 2560, 1440);
}
```

Output:

```
[14:02:31.512] [1234] [INFO ] [OVERLAY] Window created (2560x1440)
```

## Contributors
- **Creator:** [lyk64](https://github.com/lyk64)

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

#pragma once

#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wredundant-move"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#pragma GCC diagnostic pop

namespace nouveaux {
    class Logger {
        static std::shared_ptr<spdlog::logger> __logger;

      public:
        static auto init(uint64_t id) -> void;
        [[nodiscard]]
        static auto get() -> std::shared_ptr<spdlog::logger>;
    };
}

#if defined(NOUVEAUX_DISABLE_LOGS)
    #define trace(fmt, ...)
    #define debug(fmt, ...)
    #define info(fmt, ...)
    #define warn(fmt, ...)
    #define error(fmt, ...)
    #define critical(fmt, ...)
#else
    #define trace(fmt, ...) nouveaux::Logger::get()->trace(fmt __VA_OPT__(, ) __VA_ARGS__);
    #define debug(fmt, ...) nouveaux::Logger::get()->debug(fmt __VA_OPT__(, ) __VA_ARGS__);
    #define info(fmt, ...) nouveaux::Logger::get()->info(fmt __VA_OPT__(, ) __VA_ARGS__);
    #define warn(fmt, ...) nouveaux::Logger::get()->warn(fmt __VA_OPT__(, ) __VA_ARGS__);
    #define error(fmt, ...) nouveaux::Logger::get()->error(fmt __VA_OPT__(, ) __VA_ARGS__);
    #define critical(fmt, ...) nouveaux::Logger::get()->critical(fmt __VA_OPT__(, ) __VA_ARGS__);
#endif
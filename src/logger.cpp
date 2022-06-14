#include "logger.hpp"

#include <fmt/format.h>


namespace nouveaux {

    std::shared_ptr<spdlog::logger> Logger::__logger;

    auto Logger::init(uint64_t id) -> void {
        // Single initialization guard.
        static bool initialized = false;
        if (initialized) {
            return;
        }
        initialized = true;

        __logger = spdlog::basic_logger_mt("out", fmt::format("logs/process_{}.log", id), true);
        spdlog::set_pattern("[%L][%H:%M:%S] %v");
        spdlog::flush_every(std::chrono::seconds(1));
#if defined(NOUVEAUX_DEBUG)
        spdlog::set_level(spdlog::level::trace);
#else
        spdlog::set_level(spdlog::level::info);
#endif
    }

    auto Logger::get() -> std::shared_ptr<spdlog::logger> {
        return __logger;
    }

}
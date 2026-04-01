#include "Logger.h"

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

void initLogger(spdlog::level::level_enum level, const std::string& log_file)
{
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;

    // Sink 1: stderr with ANSI colour codes.
    auto stderr_sink = std::make_shared<spdlog::sinks::ansicolor_stderr_sink_mt>();
    stderr_sink->set_level(level);
    stderr_sink->set_pattern("[%T.%e] [%^%l%$] %v");

    // Sink 2: plain-text file (truncated at startup, not appended).
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, /*truncate=*/true);
    file_sink->set_level(level);
    file_sink->set_pattern("[%Y-%m-%d %T.%e] [%l] %v");

    // Combine both sinks into one named logger.
    auto logger = std::make_shared<spdlog::logger>(
        "GA_3DBPP",
        spdlog::sinks_init_list{stderr_sink, file_sink}
    );
    logger->set_level(level);

    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
}

std::shared_ptr<spdlog::logger> getLogger()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        initLogger();
    }
    return spdlog::get("GA_3DBPP");
}

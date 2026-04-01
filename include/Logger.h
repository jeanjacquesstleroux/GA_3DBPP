#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>

// Initializes the application-wide logger with two sinks:
//   1. stderr — coloured output (warnings appear yellow, errors red)
//   2. log_file — plain-text append log on disk for post-run review
//
// Must be called once before any other logging.  Subsequent calls are no-ops.
// Default level : info   (debug < info < warn < error)
// Default file  : "ga_3dbpp.log" in the working directory
void initLogger(spdlog::level::level_enum level    = spdlog::level::info,
                const std::string&         log_file = "ga_3dbpp.log");

// Returns the shared application logger.
// Usage:  getLogger()->info("placed {} items", count);
//         getLogger()->warn("EP list empty for container {}", ci);
// If initLogger was never called, auto-initializes at info level.
std::shared_ptr<spdlog::logger> getLogger();

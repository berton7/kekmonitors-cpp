#pragma once
#include <iostream>
#include <kekmonitors/config.hpp>
#include <kekmonitors/core.hpp>
#include <pwd.h>
#include <spdlog/spdlog.h>
#include <string>

#ifdef KEKMONITORS_DEBUG
#define KDBGD(x)                                                               \
  do {                                                                         \
    spdlog::get("KDBG")->debug("[{}] {}", __FUNCTION__, x);                    \
  } while (0)
#define KINFOD(x)                                                              \
  do {                                                                         \
    spdlog::get("KDBG")->info("[{}] {}", __FUNCTION__, x);                     \
  } while (0)
#define KWARND(x)                                                              \
  do {                                                                         \
    spdlog::get("KDBG")->warn("[{}] {}", __FUNCTION__, x);                     \
  } while (0)
#define KERRD(x)                                                               \
  do {                                                                         \
    spdlog::get("KDBG")->error("[{}] {}", __FUNCTION__, x);                    \
  } while (0)
#else
#define KDBGD(x)
#define KINFOD(x)
#define KWARND(x)
#define KERRD(x)
#endif

namespace kekmonitors::utils {

std::string getUserHomeDir();

std::string getLocalKekDir();

std::string getContentIfFileExistsElseCreate(const std::string &filepath,
                                             const std::string &content = "");

void initDebugLogger();

spdlog::logger getLogger(const Config &config, const std::string &name);
} // namespace kekmonitors::utils

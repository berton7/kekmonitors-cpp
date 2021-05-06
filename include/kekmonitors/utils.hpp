#pragma once
#include <kekmonitors/core.hpp>
#include <kekmonitors/config.hpp>
#include <pwd.h>
#include <string>
#include <iostream>
#include <spdlog/spdlog.h>

#ifdef KEKMONITORS_DEBUG
#define KDBGD(...) do{spdlog::debug(__VA_ARGS__);} while(0)
#define KINFOD(...) do{spdlog::info(__VA_ARGS__);} while(0)
#define KWARND(...) do{spdlog::warn(__VA_ARGS__);} while(0)
#define KERRD(...) do{spdlog::error(__VA_ARGS__);} while(0)
#else
#define KDBGD(x)
#define KINFOD(x)
#define KWARND(x)
#define KERRD(x)
#endif

namespace kekmonitors::utils {

std::string getUserHomeDir();

std::string getLocalKekDir();

std::string getContentIfFileExistsElseCreate(const std::string &filepath, const std::string &content = "");

spdlog::logger getLogger(const Config &config, const std::string &name);
} // namespace kekmonitors::utils
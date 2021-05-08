#pragma once
#include <iostream>
#include <kekmonitors/config.hpp>
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>
#include <pwd.h>
#include <spdlog/spdlog.h>
#include <string>

#ifdef KEKMONITORS_DEBUG
#define KDBGD(x)                                                               \
    do {                                                                       \
        spdlog::get("KDBG")->debug("[{}] {}", __FUNCTION__, x);                \
    } while (0)
#define KINFOD(x)                                                              \
    do {                                                                       \
        spdlog::get("KDBG")->info("[{}] {}", __FUNCTION__, x);                 \
    } while (0)
#define KWARND(x)                                                              \
    do {                                                                       \
        spdlog::get("KDBG")->warn("[{}] {}", __FUNCTION__, x);                 \
    } while (0)
#define KERRD(x)                                                               \
    do {                                                                       \
        spdlog::get("KDBG")->error("[{}] {}", __FUNCTION__, x);                \
    } while (0)
#else
#define KDBGD(x)
#define KINFOD(x)
#define KWARND(x)
#define KERRD(x)
#endif

namespace kekmonitors::utils {

static std::map<kekmonitors::COMMANDS, std::string> commandToString{
    {kekmonitors::COMMANDS::PING, "PING"},
    {kekmonitors::COMMANDS::STOP, "STOP"},
    {kekmonitors::COMMANDS::SET_SHOES, "SET_SHOES"},
    {kekmonitors::COMMANDS::ADD_SHOES, "ADD_SHOES"},
    {kekmonitors::COMMANDS::GET_SHOES, "GET_SHOES"},
    {kekmonitors::COMMANDS::GET_CONFIG, "GET_CONFIG"},
    {kekmonitors::COMMANDS::GET_WHITELIST, "GET_WHITELIST"},
    {kekmonitors::COMMANDS::GET_BLACKLIST, "GET_BLACKLIST"},
    {kekmonitors::COMMANDS::GET_WEBHOOKS, "GET_WEBHOOKS"},
    {kekmonitors::COMMANDS::SET_SPECIFIC_CONFIG, "SET_SPECIFIC_CONFIG"},
    {kekmonitors::COMMANDS::SET_SPECIFIC_WEBHOOKS, "SET_SPECIFIC_WEBHOOKS"},
    {kekmonitors::COMMANDS::SET_SPECIFIC_BLACKLIST, "SET_SPECIFIC_BLACKLIST"},
    {kekmonitors::COMMANDS::SET_SPECIFIC_WHITELIST, "SET_SPECIFIC_WHITELIST"},
    {kekmonitors::COMMANDS::SET_COMMON_CONFIG, "SET_COMMON_CONFIG"},
    {kekmonitors::COMMANDS::SET_COMMON_WEBHOOKS, "SET_COMMON_WEBHOOKS"},
    {kekmonitors::COMMANDS::SET_COMMON_BLACKLIST, "SET_COMMON_BLACKLIST"},
    {kekmonitors::COMMANDS::SET_COMMON_WHITELIST, "SET_COMMON_WHITELIST"},
    {kekmonitors::COMMANDS::MM_ADD_MONITOR, "MM_ADD_MONITOR"},
    {kekmonitors::COMMANDS::MM_ADD_SCRAPER, "MM_ADD_SCRAPER"},
    {kekmonitors::COMMANDS::MM_ADD_MONITOR_SCRAPER, "MM_ADD_MONITOR_SCRAPER"},
    {kekmonitors::COMMANDS::MM_STOP_MONITOR, "MM_STOP_MONITOR"},
    {kekmonitors::COMMANDS::MM_STOP_SCRAPER, "MM_STOP_SCRAPER"},
    {kekmonitors::COMMANDS::MM_STOP_MONITOR_SCRAPER, "MM_STOP_MONITOR_SCRAPER"},
    {kekmonitors::COMMANDS::MM_STOP_MONITOR_MANAGER, "MM_STOP_MONITOR_MANAGER"},
    {kekmonitors::COMMANDS::MM_GET_MONITOR_STATUS, "MM_GET_MONITOR_STATUS"},
    {kekmonitors::COMMANDS::MM_GET_SCRAPER_STATUS, "MM_GET_SCRAPER_STATUS"},
    {kekmonitors::COMMANDS::MM_GET_MONITOR_SCRAPER_STATUS, "MM_GET_MONITOR_SCRAPER_STATUS"},
    {kekmonitors::COMMANDS::MM_SET_MONITOR_CONFIG, "MM_SET_MONITOR_CONFIG"},
    {kekmonitors::COMMANDS::MM_GET_MONITOR_CONFIG, "MM_GET_MONITOR_CONFIG"},
    {kekmonitors::COMMANDS::MM_SET_MONITOR_WHITELIST, "MM_SET_MONITOR_WHITELIST"},
    {kekmonitors::COMMANDS::MM_GET_MONITOR_WHITELIST, "MM_GET_MONITOR_WHITELIST"},
    {kekmonitors::COMMANDS::MM_SET_MONITOR_BLACKLIST, "MM_SET_MONITOR_BLACKLIST"},
    {kekmonitors::COMMANDS::MM_GET_MONITOR_BLACKLIST, "MM_GET_MONITOR_BLACKLIST"},
    {kekmonitors::COMMANDS::MM_SET_MONITOR_WEBHOOKS, "MM_SET_MONITOR_WEBHOOKS"},
    {kekmonitors::COMMANDS::MM_GET_MONITOR_WEBHOOKS, "MM_GET_MONITOR_WEBHOOKS"},
    {kekmonitors::COMMANDS::MM_SET_SCRAPER_CONFIG, "MM_SET_SCRAPER_CONFIG"},
    {kekmonitors::COMMANDS::MM_GET_SCRAPER_CONFIG, "MM_GET_SCRAPER_CONFIG"},
    {kekmonitors::COMMANDS::MM_SET_SCRAPER_WHITELIST, "MM_SET_SCRAPER_WHITELIST"},
    {kekmonitors::COMMANDS::MM_GET_SCRAPER_WHITELIST, "MM_GET_SCRAPER_WHITELIST"},
    {kekmonitors::COMMANDS::MM_SET_SCRAPER_BLACKLIST, "MM_SET_SCRAPER_BLACKLIST"},
    {kekmonitors::COMMANDS::MM_GET_SCRAPER_BLACKLIST, "MM_GET_SCRAPER_BLACKLIST"},
    {kekmonitors::COMMANDS::MM_SET_SCRAPER_WEBHOOKS, "MM_SET_SCRAPER_WEBHOOKS"},
    {kekmonitors::COMMANDS::MM_GET_SCRAPER_WEBHOOKS, "MM_GET_SCRAPER_WEBHOOKS"},
    {kekmonitors::COMMANDS::MM_SET_MONITOR_SCRAPER_BLACKLIST,
     "MM_SET_MONITOR_SCRAPER_BLACKLIST"},
    {kekmonitors::COMMANDS::MM_SET_MONITOR_SCRAPER_WHITELIST,
     "MM_SET_MONITOR_SCRAPER_WHITELIST"},
    {kekmonitors::COMMANDS::MM_SET_MONITOR_SCRAPER_WEBHOOKS,
     "MM_SET_MONITOR_SCRAPER_WEBHOOKS"},
    {kekmonitors::COMMANDS::MM_SET_MONITOR_SCRAPER_CONFIG, "MM_SET_MONITOR_SCRAPER_CONFIG"},
    {kekmonitors::COMMANDS::MM_GET_MONITOR_SHOES, "MM_GET_MONITOR_SHOES"},
    {kekmonitors::COMMANDS::MM_GET_SCRAPER_SHOES, "MM_GET_SCRAPER_SHOES"},
};

static std::map<kekmonitors::ERRORS, std::string> errorsToString = {
    {kekmonitors::ERRORS::OK, "OK"},
    {kekmonitors::ERRORS::SOCKET_DOESNT_EXIST, "SOCKET_DOESNT_EXIST"},
    {kekmonitors::ERRORS::SOCKET_COULDNT_CONNECT, "SOCKET_COULDNT_CONNECT"},
    {kekmonitors::ERRORS::SOCKET_TIMEOUT, "SOCKET_TIMEOUT"},
    {kekmonitors::ERRORS::MONITOR_DOESNT_EXIST, "MONITOR_DOESNT_EXIST"},
    {kekmonitors::ERRORS::SCRAPER_DOESNT_EXIST, "SCRAPER_DOESNT_EXIST"},
    {kekmonitors::ERRORS::MONITOR_NOT_REGISTERED, "MONITOR_NOT_REGISTERED"},
    {kekmonitors::ERRORS::SCRAPER_NOT_REGISTERED, "SCRAPER_NOT_REGISTERED"},
    {kekmonitors::ERRORS::UNRECOGNIZED_COMMAND, "UNRECOGNIZED_COMMAND"},
    {kekmonitors::ERRORS::BAD_PAYLOAD, "BAD_PAYLOAD"},
    {kekmonitors::ERRORS::MISSING_PAYLOAD, "MISSING_PAYLOAD"},
    {kekmonitors::ERRORS::MISSING_PAYLOAD_ARGS, "MISSING_PAYLOAD_ARGS"},
    {kekmonitors::ERRORS::MM_COULDNT_ADD_MONITOR, "MM_COULDNT_ADD_MONITOR"},
    {kekmonitors::ERRORS::MM_COULDNT_ADD_SCRAPER, "MM_COULDNT_ADD_SCRAPER"},
    {kekmonitors::ERRORS::MM_COULDNT_ADD_MONITOR_SCRAPER, "MM_COULDNT_ADD_MONITOR_SCRAPER"},
    {kekmonitors::ERRORS::MM_COULDNT_STOP_MONITOR, "MM_COULDNT_STOP_MONITOR"},
    {kekmonitors::ERRORS::MM_COULDNT_STOP_SCRAPER, "MM_COULDNT_STOP_SCRAPER"},
    {kekmonitors::ERRORS::MM_COULDNT_STOP_MONITOR_SCRAPER, "MM_COULDNT_STOP_MONITOR_SCRAPER"},
    {kekmonitors::ERRORS::OTHER_ERROR, "OTHER_ERROR"},
    {kekmonitors::ERRORS::UNKNOWN_ERROR, "UNKNOWN_ERROR"},
};

std::string getUserHomeDir();

std::string getLocalKekDir();

std::string getContentIfFileExistsElseCreate(const std::string &filepath,
                                             const std::string &content = "");

void initDebugLogger();

spdlog::logger getLogger(const std::shared_ptr<Config> &config,
                         const std::string &name);
} // namespace kekmonitors::utils

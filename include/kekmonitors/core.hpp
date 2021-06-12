#pragma once
#include <boost/bimap.hpp>
#include <inotify-cxx.h>
#include <kekmonitors/typedefs.hpp>
#include <mongocxx/instance.hpp>
#include <spdlog/spdlog.h>

#ifndef NDEBUG
#define KEKMONITORS_DEBUG
#endif

#ifdef KEKMONITORS_DEBUG
#define KDBG(x)                                                                \
    do {                                                                       \
        spdlog::get("KDBG")->debug("[{}] {}", __FUNCTION__, x);                \
    } while (0)
#else
#define KDBG(x)
#endif

#define REGISTER_COMMAND(cmd)                                                  \
    kekmonitors::commandStringMap().insert(kekmonitors::CommandStringValue(    \
        cmd, kekmonitors::utils::getStringWithoutNamespaces(#cmd)))
#define REGISTER_ERROR(err)                                                    \
    kekmonitors::errorStringMap().insert(kekmonitors::ErrorStringValue(        \
        err, kekmonitors::utils::getStringWithoutNamespaces(#err)))

#define KEKMONITORS_FIRST_CUSTOM_COMMAND                                       \
    (kekmonitors::COMMANDS::MM_GET_SCRAPER_SHOES + 1)
#define KEKMONITORS_FIRST_CUSTOM_ERROR (kekmonitors::ERRORS::UNKNOWN_ERROR + 1)

namespace kekmonitors {

enum COMMANDS : CommandType {
    PING = 1,
    STOP,
    SET_SHOES,
    ADD_SHOES,
    GET_SHOES,
    GET_CONFIG,
    GET_WHITELIST,
    GET_BLACKLIST,
    GET_WEBHOOKS,
    SET_SPECIFIC_CONFIG,
    SET_SPECIFIC_WEBHOOKS,
    SET_SPECIFIC_BLACKLIST,
    SET_SPECIFIC_WHITELIST,
    SET_COMMON_CONFIG,
    SET_COMMON_WEBHOOKS,
    SET_COMMON_BLACKLIST,
    SET_COMMON_WHITELIST,

    MM_ADD_MONITOR,
    MM_ADD_SCRAPER,
    MM_ADD_MONITOR_SCRAPER,
    MM_STOP_MONITOR,
    MM_STOP_SCRAPER,
    MM_STOP_MONITOR_SCRAPER,
    MM_STOP_MONITOR_MANAGER,
    MM_GET_MONITOR_STATUS,
    MM_GET_SCRAPER_STATUS,
    MM_GET_MONITOR_SCRAPER_STATUS,
    MM_SET_MONITOR_CONFIG,
    MM_GET_MONITOR_CONFIG,
    MM_SET_MONITOR_WHITELIST,
    MM_GET_MONITOR_WHITELIST,
    MM_SET_MONITOR_BLACKLIST,
    MM_GET_MONITOR_BLACKLIST,
    MM_SET_MONITOR_WEBHOOKS,
    MM_GET_MONITOR_WEBHOOKS,
    MM_SET_SCRAPER_CONFIG,
    MM_GET_SCRAPER_CONFIG,
    MM_SET_SCRAPER_WHITELIST,
    MM_GET_SCRAPER_WHITELIST,
    MM_SET_SCRAPER_BLACKLIST,
    MM_GET_SCRAPER_BLACKLIST,
    MM_SET_SCRAPER_WEBHOOKS,
    MM_GET_SCRAPER_WEBHOOKS,
    MM_SET_MONITOR_SCRAPER_BLACKLIST,
    MM_SET_MONITOR_SCRAPER_WHITELIST,
    MM_SET_MONITOR_SCRAPER_WEBHOOKS,
    MM_SET_MONITOR_SCRAPER_CONFIG,
    MM_GET_MONITOR_SHOES,
    MM_GET_SCRAPER_SHOES,
};

enum ERRORS : ErrorType {
    OK = 0,

    SOCKET_DOESNT_EXIST,
    SOCKET_COULDNT_CONNECT,
    SOCKET_TIMEOUT,

    MONITOR_DOESNT_EXIST,
    SCRAPER_DOESNT_EXIST,
    MONITOR_NOT_REGISTERED,
    SCRAPER_NOT_REGISTERED,

    UNRECOGNIZED_COMMAND,
    BAD_PAYLOAD,
    MISSING_PAYLOAD,
    MISSING_PAYLOAD_ARGS,

    MM_COULDNT_ADD_MONITOR,
    MM_COULDNT_ADD_SCRAPER,
    MM_COULDNT_ADD_MONITOR_SCRAPER,
    MM_COULDNT_STOP_MONITOR,
    MM_COULDNT_STOP_SCRAPER,
    MM_COULDNT_STOP_MONITOR_SCRAPER,

    OTHER_ERROR,
    UNKNOWN_ERROR
};

enum class MonitorOrScraper { Monitor = 0, Scraper };

typedef boost::bimap<CommandType, std::string> CommandStringMap;
typedef CommandStringMap::value_type CommandStringValue;
typedef boost::bimap<ErrorType, std::string> ErrorStringMap;
typedef ErrorStringMap::value_type ErrorStringValue;

CommandStringMap &commandStringMap();
ErrorStringMap &errorStringMap();

void init();
mongocxx::instance &initDbInstance();
} // namespace kekmonitors
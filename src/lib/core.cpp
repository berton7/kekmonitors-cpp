//
// Created by berton on 5/14/21.
//
#include <kekmonitors/core.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>

#define CORE_REGISTER_COMMAND(cmd)                                             \
    kekmonitors::commandStringMap().insert(kekmonitors::CommandStringValue(    \
        cmd, getStringWithoutNamespaces(#cmd)))
#define CORE_REGISTER_ERROR(err)                                               \
    kekmonitors::errorStringMap().insert(                                      \
        kekmonitors::ErrorStringValue(err, getStringWithoutNamespaces(#err)))

std::string getStringWithoutNamespaces(const std::string &command) {
    auto lastNamespaceSeparator = command.rfind("::");
    return lastNamespaceSeparator != std::string::npos
               ? command.substr(lastNamespaceSeparator + 2)
               : command;
}

namespace kekmonitors {
namespace {
std::shared_ptr<spdlog::logger> initDebugLogger() {
#ifdef KEKMONITORS_DEBUG
    auto dbgLog = spdlog::stdout_color_st("KDBG");
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    dbgLog->set_pattern("%v");
    dbgLog->set_level(spdlog::level::debug);
    return dbgLog;
#endif
}
void initMaps() {
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::PING);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::STOP);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::SET_SHOES);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::ADD_SHOES);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::GET_SHOES);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::GET_CONFIG);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::GET_WHITELIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::GET_BLACKLIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::GET_WEBHOOKS);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::SET_SPECIFIC_CONFIG);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::SET_SPECIFIC_WEBHOOKS);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::SET_SPECIFIC_BLACKLIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::SET_SPECIFIC_WHITELIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::SET_COMMON_CONFIG);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::SET_COMMON_WEBHOOKS);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::SET_COMMON_BLACKLIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::SET_COMMON_WHITELIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_ADD_MONITOR);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_ADD_SCRAPER);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_ADD_MONITOR_SCRAPER);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_STOP_MONITOR);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_STOP_SCRAPER);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_STOP_MONITOR_SCRAPER);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_STOP_MONITOR_MANAGER);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_STATUS);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_SCRAPER_STATUS);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_SCRAPER_STATUS);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_CONFIG);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_CONFIG);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_WHITELIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_WHITELIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_BLACKLIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_BLACKLIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_WEBHOOKS);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_WEBHOOKS);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_SCRAPER_CONFIG);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_SCRAPER_CONFIG);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_SCRAPER_WHITELIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_SCRAPER_WHITELIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_SCRAPER_BLACKLIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_SCRAPER_BLACKLIST);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_SCRAPER_WEBHOOKS);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_SCRAPER_WEBHOOKS);
    CORE_REGISTER_COMMAND(
        kekmonitors::COMMANDS::MM_SET_MONITOR_SCRAPER_BLACKLIST);
    CORE_REGISTER_COMMAND(
        kekmonitors::COMMANDS::MM_SET_MONITOR_SCRAPER_WHITELIST);
    CORE_REGISTER_COMMAND(
        kekmonitors::COMMANDS::MM_SET_MONITOR_SCRAPER_WEBHOOKS);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_SCRAPER_CONFIG);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_SHOES);
    CORE_REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_SCRAPER_SHOES);

    CORE_REGISTER_ERROR(kekmonitors::ERRORS::OK);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::SOCKET_DOESNT_EXIST);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::SOCKET_COULDNT_CONNECT);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::SOCKET_TIMEOUT);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::MONITOR_DOESNT_EXIST);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::SCRAPER_DOESNT_EXIST);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::MONITOR_NOT_REGISTERED);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::SCRAPER_NOT_REGISTERED);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::UNRECOGNIZED_COMMAND);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::BAD_PAYLOAD);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::MISSING_PAYLOAD);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::MISSING_PAYLOAD_ARGS);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::MM_COULDNT_ADD_MONITOR);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::MM_COULDNT_ADD_SCRAPER);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::MM_COULDNT_ADD_MONITOR_SCRAPER);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::MM_COULDNT_STOP_MONITOR);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::MM_COULDNT_STOP_SCRAPER);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::MM_COULDNT_STOP_MONITOR_SCRAPER);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::OTHER_ERROR);
    CORE_REGISTER_ERROR(kekmonitors::ERRORS::UNKNOWN_ERROR);
}

int init() {
    initMaps();
    initDebugLogger();
    return 0;
}
auto _ = init();
} // namespace

CommandStringMap &commandStringMap()
{
    static CommandStringMap map;
    return map;
}

ErrorStringMap &errorStringMap()
{
    static ErrorStringMap map;
    return map;
}
} // namespace kekmonitors
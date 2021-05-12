//
// Created by berton on 4/28/21.
//
#include <boost/filesystem.hpp>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/utils.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <unistd.h>

using namespace boost;

namespace kekmonitors::utils {

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
void initMaps()
{
    REGISTER_COMMAND(kekmonitors::COMMANDS::PING);
    REGISTER_COMMAND(kekmonitors::COMMANDS::STOP);
    REGISTER_COMMAND(kekmonitors::COMMANDS::SET_SHOES);
    REGISTER_COMMAND(kekmonitors::COMMANDS::ADD_SHOES);
    REGISTER_COMMAND(kekmonitors::COMMANDS::GET_SHOES);
    REGISTER_COMMAND(kekmonitors::COMMANDS::GET_CONFIG);
    REGISTER_COMMAND(kekmonitors::COMMANDS::GET_WHITELIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::GET_BLACKLIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::GET_WEBHOOKS);
    REGISTER_COMMAND(kekmonitors::COMMANDS::SET_SPECIFIC_CONFIG);
    REGISTER_COMMAND(kekmonitors::COMMANDS::SET_SPECIFIC_WEBHOOKS);
    REGISTER_COMMAND(kekmonitors::COMMANDS::SET_SPECIFIC_BLACKLIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::SET_SPECIFIC_WHITELIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::SET_COMMON_CONFIG);
    REGISTER_COMMAND(kekmonitors::COMMANDS::SET_COMMON_WEBHOOKS);
    REGISTER_COMMAND(kekmonitors::COMMANDS::SET_COMMON_BLACKLIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::SET_COMMON_WHITELIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_ADD_MONITOR);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_ADD_SCRAPER);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_ADD_MONITOR_SCRAPER);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_STOP_MONITOR);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_STOP_SCRAPER);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_STOP_MONITOR_SCRAPER);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_STOP_MONITOR_MANAGER);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_STATUS);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_SCRAPER_STATUS);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_SCRAPER_STATUS);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_CONFIG);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_CONFIG);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_WHITELIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_WHITELIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_BLACKLIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_BLACKLIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_WEBHOOKS);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_WEBHOOKS);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_SCRAPER_CONFIG);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_SCRAPER_CONFIG);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_SCRAPER_WHITELIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_SCRAPER_WHITELIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_SCRAPER_BLACKLIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_SCRAPER_BLACKLIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_SCRAPER_WEBHOOKS);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_SCRAPER_WEBHOOKS);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_SCRAPER_BLACKLIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_SCRAPER_WHITELIST);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_SCRAPER_WEBHOOKS);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_SET_MONITOR_SCRAPER_CONFIG);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_MONITOR_SHOES);
    REGISTER_COMMAND(kekmonitors::COMMANDS::MM_GET_SCRAPER_SHOES);

    REGISTER_ERROR(kekmonitors::ERRORS::OK);
    REGISTER_ERROR(kekmonitors::ERRORS::SOCKET_DOESNT_EXIST);
    REGISTER_ERROR(kekmonitors::ERRORS::SOCKET_COULDNT_CONNECT);
    REGISTER_ERROR(kekmonitors::ERRORS::SOCKET_TIMEOUT);
    REGISTER_ERROR(kekmonitors::ERRORS::MONITOR_DOESNT_EXIST);
    REGISTER_ERROR(kekmonitors::ERRORS::SCRAPER_DOESNT_EXIST);
    REGISTER_ERROR(kekmonitors::ERRORS::MONITOR_NOT_REGISTERED);
    REGISTER_ERROR(kekmonitors::ERRORS::SCRAPER_NOT_REGISTERED);
    REGISTER_ERROR(kekmonitors::ERRORS::UNRECOGNIZED_COMMAND);
    REGISTER_ERROR(kekmonitors::ERRORS::BAD_PAYLOAD);
    REGISTER_ERROR(kekmonitors::ERRORS::MISSING_PAYLOAD);
    REGISTER_ERROR(kekmonitors::ERRORS::MISSING_PAYLOAD_ARGS);
    REGISTER_ERROR(kekmonitors::ERRORS::MM_COULDNT_ADD_MONITOR);
    REGISTER_ERROR(kekmonitors::ERRORS::MM_COULDNT_ADD_SCRAPER);
    REGISTER_ERROR(kekmonitors::ERRORS::MM_COULDNT_ADD_MONITOR_SCRAPER);
    REGISTER_ERROR(kekmonitors::ERRORS::MM_COULDNT_STOP_MONITOR);
    REGISTER_ERROR(kekmonitors::ERRORS::MM_COULDNT_STOP_SCRAPER);
    REGISTER_ERROR(kekmonitors::ERRORS::MM_COULDNT_STOP_MONITOR_SCRAPER);
    REGISTER_ERROR(kekmonitors::ERRORS::OTHER_ERROR);
    REGISTER_ERROR(kekmonitors::ERRORS::UNKNOWN_ERROR);
}

int init()
{
    initMaps();
    initDebugLogger();
    return 0;
}

auto _ = init();
}

std::string getUserHomeDir() {
    return getenv("HOME") ?: getpwuid(getuid())->pw_dir;
}

std::string getLocalKekDir() { return getUserHomeDir() + "/.kekmonitors"; }

std::string getContentIfFileExistsElseCreate(const std::string &filepath,
                                             const std::string &content) {
    const std::string filepathUntilFile = filepath.substr(
        0, filepath.rfind(filesystem::path::preferred_separator));
    filesystem::create_directories(filepathUntilFile);
    std::fstream file;
    if (filesystem::is_regular_file(filepath)) {
        file.open(filepath, std::ios::in);
        if (!file.is_open()) {
            KDBG("Failed to open config file.");
            return "";
        }
        KDBG("Config file exists, returning content");
        std::stringstream ss;
        while (!file.eof()) {
            std::string line;
            std::getline(file, line);
            ss << line;
        }
        return ss.str();
    } else {
        file.open(filepath, std::ios::out);
        if (!file.is_open()) {
            KDBG("Failed to open config file.");
            return "";
        }
        KDBG("Config file doesn't exist, creating and returning content");
        file << content;
        file.close();
        return content;
    }
}

std::unique_ptr<spdlog::logger> getLogger(const std::string &name) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);
    console_sink->set_pattern("[%^%n%$] %v");

    // auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
    //     "logs/multisink.txt", true);
    // file_sink->set_level(spdlog::level::trace);

    std::unique_ptr<spdlog::logger> logger = std::make_unique<spdlog::logger>(
        name,
        std::initializer_list<spdlog::sink_ptr>{console_sink /*, file_sink*/});
    logger->set_level(spdlog::level::debug);
    return logger;
}

kekmonitors::COMMANDS stringToCommand(const std::string &str) {
    return commandStringMap.right.at(str);
}
std::string commandToString(const kekmonitors::COMMANDS cmd) {
    return commandStringMap.left.at(cmd);
}
kekmonitors::ERRORS stringToError(const std::string &str) {
    return errorStringMap.right.at(str);
}
std::string errorToString(const kekmonitors::ERRORS err) {
    return errorStringMap.left.at(err);
}
} // namespace kekmonitors::utils

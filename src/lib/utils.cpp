//
// Created by berton on 4/28/21.
//
#include <boost/filesystem.hpp>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/utils.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <unistd.h>

using namespace boost;

namespace kekmonitors::utils {

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
            KERRD("Failed to open config file.");
            return "";
        }
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
            KERRD("Failed to open config file.");
            return "";
        }
        file << content;
        file.close();
        return content;
    }
}

void initDebugLogger() {
#ifdef KEKMONITORS_DEBUG
    auto dbgLog = spdlog::stdout_color_st("KDBG");
    dbgLog->set_pattern("[DBG] [%^%l%$] %v");
    dbgLog->set_level(spdlog::level::debug);
#endif
}

std::unique_ptr<spdlog::logger> getLogger(const std::string &name) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    //auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
    //    "logs/multisink.txt", true);
    //file_sink->set_level(spdlog::level::trace);

    std::unique_ptr<spdlog::logger> logger = std::make_unique<spdlog::logger>(name, std::initializer_list<spdlog::sink_ptr>{console_sink/*, file_sink*/});
    logger->set_level(spdlog::level::debug);
    return logger;
}
} // namespace kekmonitors::utils
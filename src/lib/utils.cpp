//
// Created by berton on 4/28/21.
//
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/utils.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <sstream>
#include <unistd.h>

namespace kekmonitors::utils{
std::string getUserHomeDir() {
    return getenv("HOME") ?: getpwuid(getuid())->pw_dir;
}

std::string getLocalKekDir() { return getUserHomeDir() + "/.kekmonitors"; }

std::string getContentIfFileExistsElseCreate(const std::string &filepath,
                                             const std::string &content) {
    const std::string filepathUntilFile = filepath.substr(
        0, filepath.rfind(fs::path::preferred_separator));
    fs::create_directories(filepathUntilFile);
    std::fstream file;
    if (fs::is_regular_file(filepath)) {
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

CommandType stringToCommand(const std::string &str) {
    return commandStringMap().right.at(str);
}
std::string commandToString(CommandType cmd) {
    return commandStringMap().left.at(cmd);
}
ErrorType stringToError(const std::string &str) {
    return errorStringMap().right.at(str);
}
std::string errorToString(ErrorType err) {
    return errorStringMap().left.at(err);
}

std::string getStringWithoutNamespaces(const std::string &command) {
    auto lastNamespaceSeparator = command.rfind("::");
    return lastNamespaceSeparator != std::string::npos
               ? command.substr(lastNamespaceSeparator + 2)
               : command;
}

std::string getStringWithoutNamespaces(CommandType command) {
    return getStringWithoutNamespaces(
        kekmonitors::utils::commandToString(command));
}

fs::path getPythonExecutable() {
    namespace bp = boost::process;
    static fs::path pythonExecutable{};
    if (pythonExecutable.empty()) {
        for (const auto &possiblePythonPath : {"python", "python3"}) {
            auto pythonPath = bp::search_path(possiblePythonPath);
            if (!pythonPath.empty()) {
                bp::ipstream in;
                bp::system(pythonPath, "--version", bp::std_out > in,
                           bp::std_err > bp::null);
                // "Python 2.7.18"
                // "Python 3.6.12"
                std::string version;
                std::getline(in, version);
                if (version[7] == '3') {
                    pythonExecutable = pythonPath;
                    return pythonExecutable;
                }
            }
        }
    }
    return pythonExecutable;
}

Response makeCommonResponse(const Response &firstResponse,
                            const Response &secondResponse,
                            const ERRORS commonError) {
    Response finalResponse = Response::okResponse();
    if (firstResponse.getError() || secondResponse.getError()) {
        finalResponse.setError(commonError);
        std::string info{};
        for (const Response &response : {firstResponse, secondResponse}) {
            if (response.getError()) {
                info += errorToString(response.getError());
                if (!response.getInfo().empty())
                    info += ": " + response.getInfo() + " ";
                else
                    info += "; ";
            }
        }
        finalResponse.setInfo(info);
    }
    return finalResponse;
}
} // namespace kekmonitors::utils

//
// Created by berton on 4/28/21.
//
#include <boost/filesystem.hpp>
#include <fstream>
#include <kekmonitors/utils.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <sstream>
#include <unistd.h>

using namespace boost;

namespace kekmonitors::utils {
std::string getUserHomeDir() {
    return getenv("HOME") ?: getpwuid(getuid())->pw_dir;
}

std::string getLocalKekDir() { return getUserHomeDir() + "/.kekmonitors"; }

void KDevLog(const LogLevel &lvl, const std::string &log) {
    std::string lvlstr;
    std::basic_ostream<char> *out = &std::cout;
    switch (lvl) {
    case LogLevel::DEBUG:
        lvlstr = "[Dbg]";
        break;
    case LogLevel::INFO:
        lvlstr = "[Info]";
        break;
    case LogLevel::WARN:
        lvlstr = "[Warn]";
        out = &std::cerr;
        break;
    case LogLevel::ERROR:
        lvlstr = "[Err]";
        out = &std::cerr;
        break;
    default:
        lvlstr = "[Unkwn]";
        out = &std::cerr;
        break;
    };
    *out << lvlstr << ": " << log << std::endl;
}
void KDevDbg(const std::string &log) { KDevLog(LogLevel::DEBUG, log); }
void KDevInfo(const std::string &log) { KDevLog(LogLevel::INFO, log); }
void KDevWarn(const std::string &log) { KDevLog(LogLevel::WARN, log); }
void KDevErr(const std::string &log) { KDevLog(LogLevel::ERROR, log); }
void KDevDbg(const std::string &functionName, const std::string &log) {
    KDevLog(LogLevel::DEBUG, functionName + ": " + log);
}
void KDevInfo(const std::string &functionName, const std::string &log) {
    KDevLog(LogLevel::INFO, functionName + ": " + log);
}
void KDevWarn(const std::string &functionName, const std::string &log) {
    KDevLog(LogLevel::WARN, functionName + ": " + log);
}
void KDevErr(const std::string &functionName, const std::string &log) {
    KDevLog(LogLevel::ERROR, functionName + ": " + log);
}

std::string getContentIfFileExistsElseCreate(const std::string &filepath,
                                      const std::string &content) {
    const std::string filepathUntilFile = filepath.substr(0, filepath.rfind(filesystem::path::preferred_separator));
    filesystem::create_directories(filepathUntilFile);
    std::fstream file;
    if (filesystem::is_regular_file(filepath))
    {
        file.open(filepath, std::ios::in);
        if (!file.is_open())
        {
            KERR("Failed to open config file.");
            return "";
        }
        std::stringstream ss;
        while (!file.eof())
        {
            std::string line;
            std::getline(file, line);
            ss << line;
        }
        return ss.str();
    }
    else{
        file.open(filepath, std::ios::out);
        if (!file.is_open())
        {
            KERR("Failed to open config file.");
            return "";
        }
        file << content;
        file.close();
        return content;
    }
}

spdlog::logger getLogger(const Config &config, const std::string &name)
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::warn);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/multisink.txt", true);
    file_sink->set_level(spdlog::level::trace);

    spdlog::logger logger(name, {console_sink, file_sink});
    logger.set_level(spdlog::level::debug);
    logger.warn("this should appear in both console and file");
    logger.info("this message should not appear in the console, only in the file");
    return logger;
}
} // namespace kekmonitors::utils
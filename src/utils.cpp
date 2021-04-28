//
// Created by berton on 4/28/21.
//
#include <kekmonitors/utils.hpp>
#include <unistd.h>

namespace kekmonitors::utils {
std::string getUserHomeDir() {
    return getenv("HOME") ?: getpwuid(getuid())->pw_dir;
}

std::string getLocalKekDir() { return getUserHomeDir() + "/.kekmonitors"; }

void KLog(const LogLevel &lvl, const std::string &log) {
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
void KDebug(const std::string &log) { KLog(LogLevel::DEBUG, log); }
void KInfo(const std::string &log) { KLog(LogLevel::INFO, log); }
void KWarn(const std::string &log) { KLog(LogLevel::WARN, log); }
void KErr(const std::string &log) { KLog(LogLevel::ERROR, log); }
void KDebug(const std::string &functionName, const std::string &log) {
    KLog(LogLevel::DEBUG, functionName + ": " + log);
}
void KInfo(const std::string &functionName, const std::string &log) {
    KLog(LogLevel::INFO, functionName + ": " + log);
}
void KWarn(const std::string &functionName, const std::string &log) {
    KLog(LogLevel::WARN, functionName + ": " + log);
}
void KErr(const std::string &functionName, const std::string &log) {
    KLog(LogLevel::ERROR, functionName + ": " + log);
}

} // namespace kekmonitors::utils
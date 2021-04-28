#pragma once
#include <kekmonitors/core.hpp>
#include <pwd.h>
#include <string>
#include <iostream>

#ifdef KEKMONITORS_DEBUG
#define KDBG(x) do{kekmonitors::utils::KDebug(__PRETTY_FUNCTION__, x);} while(0)
#define KINFO(x) do{kekmonitors::utils::KInfo(__PRETTY_FUNCTION__, x);} while(0)
#define KWARN(x) do{kekmonitors::utils::KWarn(__PRETTY_FUNCTION__, x);} while(0)
#define KERR(x) do{kekmonitors::utils::KErr(__PRETTY_FUNCTION__, x);} while(0)
#else
#define KDBG(x)
#define KINFO(x)
#define KWARN(x)
#define KERR(x)
#endif

namespace kekmonitors::utils {

enum LogLevel{
    DEBUG,
    INFO,
    WARN,
    ERROR
};

std::string getUserHomeDir();

std::string getLocalKekDir();

void KLog(const LogLevel &lvl, const std::string &log);

void KDebug(const std::string &log);

void KInfo(const std::string &log);

void KWarn(const std::string &log);

void KErr(const std::string &log);

void KDebug(const std::string &functionName, const std::string &log);

void KInfo(const std::string &functionName, const std::string &log);

void KWarn(const std::string &functionName, const std::string &log);

void KErr(const std::string &functionName, const std::string &log);
} // namespace kekmonitors::utils
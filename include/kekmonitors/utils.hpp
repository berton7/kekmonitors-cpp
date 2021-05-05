#pragma once
#include <kekmonitors/core.hpp>
#include <pwd.h>
#include <string>
#include <iostream>

#ifdef KEKMONITORS_DEBUG
#define KDBG(x) do{kekmonitors::utils::KDevDbg(__PRETTY_FUNCTION__, x);} while(0)
#define KINFO(x) do{kekmonitors::utils::KDevInfo(__PRETTY_FUNCTION__, x);} while(0)
#define KWARN(x) do{kekmonitors::utils::KDevWarn(__PRETTY_FUNCTION__, x);} while(0)
#define KERR(x) do{kekmonitors::utils::KDevErr(__PRETTY_FUNCTION__, x);} while(0)
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

void KDevLog(const LogLevel &lvl, const std::string &log);

void KDevDbg(const std::string &log);

void KDevInfo(const std::string &log);

void KDevWarn(const std::string &log);

void KDevErr(const std::string &log);

void KDevDbg(const std::string &functionName, const std::string &log);

void KDevInfo(const std::string &functionName, const std::string &log);

void KDevWarn(const std::string &functionName, const std::string &log);

void KDevErr(const std::string &functionName, const std::string &log);

std::string getContentIfFileExistsElseCreate(const std::string &filepath, const std::string &content = "");
} // namespace kekmonitors::utils
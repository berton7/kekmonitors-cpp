#pragma once
#include <boost/filesystem.hpp>
#include <kekmonitors/config.hpp>
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>
#include <pwd.h>
#include <spdlog/spdlog.h>
#include <string>

#ifdef KEKMONITORS_DEBUG
#define KDBG(x)                                                                \
    do {                                                                       \
        spdlog::get("KDBG")->debug("[{}] {}", __FUNCTION__, x);                \
    } while (0)
#else
#define KDBG(x)
#endif

namespace kekmonitors::utils {

kekmonitors::CommandType stringToCommand(const std::string &);
std::string commandToString(CommandType cmd);
ErrorType stringToError(const std::string &);
std::string errorToString(ErrorType err);

std::string getUserHomeDir();

std::string getLocalKekDir();

std::string getContentIfFileExistsElseCreate(const std::string &filepath,
                                             const std::string &content = "");

std::unique_ptr<spdlog::logger> getLogger(const std::string &name);

std::string getStringWithoutNamespaces(const std::string &command);

std::string getStringWithoutNamespaces(CommandType command);

boost::filesystem::path getPythonExecutable();
} // namespace kekmonitors::utils

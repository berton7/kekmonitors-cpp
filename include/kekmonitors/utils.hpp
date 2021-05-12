#pragma once
#include <boost/bimap.hpp>
#include <kekmonitors/config.hpp>
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>
#include <pwd.h>
#include <spdlog/spdlog.h>
#include <string>

#ifdef KEKMONITORS_DEBUG
#define KDBG(x)                                                               \
    do {                                                                       \
        spdlog::get("KDBG")->debug("[{}] {}", __FUNCTION__, x);                \
    } while (0)
#else
#define KDBG(x)
#endif

#define REGISTER_COMMAND(cmd) commandStringMap.insert(CommandStringValue(cmd, #cmd))
#define REGISTER_ERROR(err) errorStringMap.insert(ErrorStringValue(err, #err))

namespace kekmonitors::utils {

typedef boost::bimap<kekmonitors::COMMANDS, std::string> CommandStringMap;
typedef CommandStringMap::value_type CommandStringValue;
typedef boost::bimap<kekmonitors::ERRORS, std::string> ErrorStringMap;
typedef ErrorStringMap::value_type ErrorStringValue;

static CommandStringMap commandStringMap;
static ErrorStringMap errorStringMap;

kekmonitors::COMMANDS stringToCommand(const std::string &);
std::string commandToString(const kekmonitors::COMMANDS);
kekmonitors::ERRORS stringToError(const std::string &);
std::string errorToString(const kekmonitors::ERRORS);

std::string getUserHomeDir();

std::string getLocalKekDir();

std::string getContentIfFileExistsElseCreate(const std::string &filepath,
                                             const std::string &content = "");

std::unique_ptr<spdlog::logger> getLogger(const std::string &name);
} // namespace kekmonitors::utils

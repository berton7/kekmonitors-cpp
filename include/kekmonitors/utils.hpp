#pragma once
#include <kekmonitors/config.hpp>
#include <kekmonitors/core.hpp>
#include <kekmonitors/msg.hpp>
#include <pwd.h>
#include <spdlog/spdlog.h>
#include <string>

namespace kekmonitors::utils {

kekmonitors::CommandType stringToCommand(const std::string &);
std::string commandToString(CommandType cmd);
ErrorType stringToError(const std::string &);
std::string errorToString(ErrorType err);

std::string getUserHomeDir();

std::string getLocalKekDir();

std::string getContentIfFileExistsElseCreate(const std::string &filepath,
                                             const std::string &content = "");

std::shared_ptr<spdlog::logger> getLogger(const std::string &name);

std::string getStringWithoutNamespaces(const std::string &command);

std::string getStringWithoutNamespaces(CommandType command);

fs::path getPythonExecutable();

Response makeCommonResponse(const Response &firstResponse,
                            const Response &secondResponse,
                            const ERRORS commonError = ERRORS::UNKNOWN_ERROR);
} // namespace kekmonitors::utils

//
// Created by berton on 5/15/21.
//

#pragma once
#include <boost/process.hpp>
#include <ctime>
#include <kekmonitors/core.hpp>
#include <kekmonitors/utils.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace kekmonitors {

class Process {
  private:
    const std::string _className{};
    boost::process::child _process;
    const std::time_t _creation = 0;

  public:
    Process() = default;
    template <typename... Args>
    Process(std::string className,
            const boost::filesystem::path &pythonExecutablePath,
            const boost::filesystem::path &monitorExecutablePath, Args... args)
        : _className(std::move(className)),
          _process(pythonExecutablePath, monitorExecutablePath, args...),
          _creation(std::time(nullptr)) {
        KDBG("Constructed process");
    };

    ~Process() { KDBG("Destroyed process"); }

    std::time_t getCreation() const { return _creation; };
    boost::process::child &getProcess() { return _process; };
    json toJson() const {
        return {
            {_className, {{"Started at", _creation}, {"PID", _process.id()}}}};
    };
    const std::string &getClassName() const { return _className; }
};
} // namespace kekmonitors
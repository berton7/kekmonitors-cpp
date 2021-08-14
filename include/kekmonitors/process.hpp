//
// Created by berton on 5/15/21.
//

#pragma once
#include <boost/process.hpp>
#include <ctime>
#include <kekmonitors/core.hpp>
#include <kekmonitors/utils.hpp>
#include <nlohmann/json.hpp>

namespace kekmonitors {

class Process {
  private:
    const std::string _className{};
    boost::process::child _process;
    const std::time_t _creation = 0;

  public:
    Process() = default;
    template <typename... Args>
    Process(std::string className, Args &&... processArgs)
        : _className(std::move(className)),
          _process(std::forward<Args>(processArgs)...),
          _creation(std::time(nullptr)) {
        KDBG("Constructed process");
    };

    ~Process() { KDBG("Destroyed process"); }

    std::time_t getCreation() const { return _creation; };
    boost::process::child &getProcess() { return _process; };
    nlohmann::json toJson() const {
        return {{"Started at", _creation}, {"PID", _process.id()}};
    };
    const std::string &getClassName() const { return _className; }
};
} // namespace kekmonitors
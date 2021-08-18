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
    const std::string m_className{};
    boost::process::child m_process;
    const std::time_t m_creation = 0;

  public:
    Process() = default;
    template <typename... Args>
    Process(std::string className, Args &&... processArgs)
        : m_className(std::move(className)),
          m_process(std::forward<Args>(processArgs)...),
          m_creation(std::time(nullptr)) {
        KDBG("Constructed process");
    };

    ~Process() { KDBG("Destroyed process"); }

    std::time_t creation() const { return m_creation; };
    boost::process::child &process() { return m_process; };
    nlohmann::json toJson() const {
        return {{"Started at", m_creation}, {"PID", m_process.id()}};
    };
    const std::string &classname() const { return m_className; }
};
} // namespace kekmonitors
//
// Created by berton on 4/28/21.
//
#pragma once
#include <boost/property_tree/ini_parser.hpp>
#include <kekmonitors/core.hpp>

namespace kekmonitors {
class Config {
  public:
    const static std::string s_defaultConfig;

    boost::property_tree::ptree p_parser;

    Config();
    ~Config();
};

Config &getConfig();
} // namespace kekmonitors

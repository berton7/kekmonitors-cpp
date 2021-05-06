//
// Created by berton on 4/28/21.
//
#pragma once
#include <kekmonitors/core.hpp>
#include <boost/property_tree/ini_parser.hpp>

namespace kekmonitors {
class Config {
  public:
    const static std::string defaultConfig;

    boost::property_tree::ptree parser;

    Config();
};
} // namespace kekmonitors

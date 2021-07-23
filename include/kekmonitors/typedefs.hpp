//
// Created by berton on 5/24/21.
//

#pragma once
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

namespace kekmonitors {

typedef uint32_t CommandType;
typedef uint32_t ErrorType;
typedef boost::system::error_code error_code;
namespace fs = boost::filesystem;
namespace ph = std::placeholders;

} // namespace kekmonitors
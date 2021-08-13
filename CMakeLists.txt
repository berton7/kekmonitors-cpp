cmake_minimum_required(VERSION 3.15)

project(kekmonitors)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

set(spdlog_version v1.8.5)
set(json_version v3.9.1)

FetchContent_Declare(
		spdlog
		PREFIX spdlog
		SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/spdlog
		GIT_REPOSITORY https://github.com/gabime/spdlog.git
		GIT_TAG ${spdlog_version}
		GIT_SHALLOW True
		CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
		-DCMAKE_INSTALL_PREFIX=${STAGING_DIR}
		-DSPDLOG_MASTER_PROJECT=OFF
		-DSPDLOG_BUILD_SHARED=OFF
)
include_directories(${PROJECT_SOURCE_DIR}/deps/spdlog/include)

FetchContent_Declare(
		json
		PREFIX json
		SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/json/include/nlohmann
		URL https://github.com/nlohmann/json/releases/download/${json_version}/json.hpp
		DOWNLOAD_NO_EXTRACT True
		CONFIGURE_COMMAND ""
		BUILD_COMMAND ""
)
include_directories(${PROJECT_SOURCE_DIR}/deps/json/include)

FetchContent_MakeAvailable(spdlog json)

add_subdirectory(src)

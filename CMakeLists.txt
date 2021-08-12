cmake_minimum_required(VERSION 3.15)

project(kekmonitors)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

set(spdlog_version v1.8.5)
set(json_version v3.9.1)
set(mongoc_version 1.18.0)
set(mongocxx_version r3.6.5)

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
#FetchContent_GetProperties(spdlog)
#if (NOT spdlog_POPULATED)
#	FetchContent_Populate(spdlog)
#	add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
#endif()


FetchContent_Declare(
		json
		PREFIX json
		SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/json/nlohmann/json
		URL https://github.com/nlohmann/json/releases/download/${json_version}/json.hpp
		DOWNLOAD_NO_EXTRACT True
		CONFIGURE_COMMAND ""
		BUILD_COMMAND ""
)
#FetchContent_GetProperties(json)
#if (NOT json_POPULATED)
#	FetchContent_Populate(json)
#	include_directories(${PROJECT_SOURCE_DIR}/deps/json)
#endif()

FetchContent_Declare(
		mongoc
		PREFIX mongoc
		SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/mongoc
		GIT_REPOSITORY https://github.com/mongodb/mongo-c-driver.git
		GIT_TAG ${mongoc_version}
		GIT_SHALLOW True
		CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
		-DCMAKE_INSTALL_PREFIX=${STAGING_DIR}
		-DENABLE_STATIC=On
)
#FetchContent_GetProperties(mongoc)
#if (NOT bsonc_POPULATED)
#	FetchContent_Populate(mongoc)
#	add_subdirectory(${mongoc_SOURCE_DIR} ${mongoc_BINARY_DIR})
#endif()

include(ExternalProject)
ExternalProject_Add(
		mongocxx
		PREFIX mongocxx
		SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/mongocxx
		GIT_REPOSITORY https://github.com/mongodb/mongo-cxx-driver.git
		GIT_TAG ${mongocxx_version}
		GIT_SHALLOW True
		CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
		-DCMAKE_INSTALL_PREFIX=${STAGING_DIR}
		-DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
		-DBUILD_SHARED_AND_STATIC_LIBS=On
)

#FetchContent_GetProperties(mongocxx)
#if (NOT mongocxx_POPULATED)
#	FetchContent_Populate(mongocxx)
#	add_subdirectory(${mongocxx_SOURCE_DIR} ${mongocxx_BINARY_DIR})
#endif()



add_subdirectory(src)

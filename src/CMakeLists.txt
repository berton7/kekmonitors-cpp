set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(KEKMONITORS_SHARED_LIBS "Build kekmonitors shared or static" OFF)

include(FetchContent)

FetchContent_Declare(
		spdlog
		PREFIX spdlog
		SOURCE_DIR ${PROJECT_SOURCE_DIR}/deps/spdlog
		GIT_REPOSITORY https://github.com/gabime/spdlog.git
		GIT_TAG v1.8.5
		GIT_SHALLOW True
		CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
		-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
		-DCMAKE_INSTALL_PREFIX=${STAGING_DIR}
		-DSPDLOG_MASTER_PROJECT=OFF
		-DSPDLOG_BUILD_SHARED=OFF
)

FetchContent_MakeAvailable(spdlog)
FetchContent_GetProperties(spdlog)
if (NOT spdlog_POPULATED)
	FetchContent_Populate(spdlog)
	add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
endif()

find_package(Boost 1.66.0 REQUIRED COMPONENTS system filesystem)
find_package(libmongocxx REQUIRED)
find_package(libbsoncxx REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/deps/include)
include_directories(${LIBMONGOCXX_INCLUDE_DIRS})
include_directories(${LIBBSONCXX_INCLUDE_DIRS})

set(KEKMONITORS_SOURCE lib/inotify-cxx.cpp lib/msg.cpp lib/server.cpp lib/utils.cpp lib/config.cpp lib/core.cpp lib/connection.cpp ../include/kekmonitors/connection.hpp)
set(KEKMONITORS_LIB_DEPS kekmonitors pthread Boost::system Boost::filesystem ${LIBMONGOCXX_LIBRARIES})

if (KEKMONITORS_SHARED_LIBS)
	add_library(kekmonitors SHARED ${KEKMONITORS_SOURCE})
else()
	add_library(kekmonitors STATIC ${KEKMONITORS_SOURCE})
endif()
add_dependencies(kekmonitors spdlog)

add_executable(moman bin/moman.cpp)
target_link_libraries(moman ${KEKMONITORS_LIB_DEPS})

add_executable(stopmm bin/stopmm.cpp)
target_link_libraries(stopmm ${KEKMONITORS_LIB_DEPS})

add_executable(cli bin/moman_cli.cpp)
target_link_libraries(cli ${KEKMONITORS_LIB_DEPS})

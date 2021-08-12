set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(KEKMONITORS_SHARED_LIBS "Build kekmonitors shared or static" OFF)

include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX(filesystem HAVE_STD_FILESYSTEM)
if (HAVE_STD_FILESYSTEM)
	add_compile_definitions(HAVE_STD_FILESYSTEM)
	set(REQUIRED_BOOST_PKGS "")
	set(REQUIRED_BOOST_LIBS "")
else()
	set(REQUIRED_BOOST_PKGS system filesystem)
	set(REQUIRED_BOOST_LIBS Boost::system Boost::filesystem)
endif()

if (REQUIRED_BOOST_PKGS)
	find_package(Boost 1.66.0 REQUIRED COMPONENTS ${REQUIRED_BOOST_PKGS})
endif()

FetchContent_MakeAvailable(spdlog json mongoc)


#find_package(mongocxx REQUIRED)
#find_package(bsoncxx REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/deps/include)

set(KEKMONITORS_SOURCE lib/inotify-cxx.cpp lib/msg.cpp lib/utils.cpp lib/config.cpp lib/core.cpp lib/connection.cpp)

if (KEKMONITORS_SHARED_LIBS)
	add_library(kekmonitors SHARED ${KEKMONITORS_SOURCE})
	set(REQUIRED_MONGO_LIBS mongo::bsoncxx_shared mongo::mongocxx_shared)
	set(VAR "")
	get_target_property(VAR mongo::mongocxx_shared INTERFACE_INCLUDE_DIRECTORIES)
	include_directories(${VAR})
else()
	add_library(kekmonitors STATIC ${KEKMONITORS_SOURCE})
	set(REQUIRED_MONGO_LIBS mongo::bsoncxx_static mongo::mongocxx_static)
	set(VAR "")
	get_target_property(VAR mongo::mongocxx_shared INTERFACE_INCLUDE_DIRECTORIES)
	include_directories(${VAR})
endif()

set(KEKMONITORS_LIB_DEPS kekmonitors pthread ${REQUIRED_BOOST_LIBS} ${REQUIRED_MONGO_LIBS})

add_dependencies(kekmonitors spdlog mongocxx)

add_executable(moman bin/moman.cpp bin/server.cpp)
target_link_libraries(moman ${KEKMONITORS_LIB_DEPS})

add_executable(stopmm bin/stopmm.cpp)
target_link_libraries(stopmm ${KEKMONITORS_LIB_DEPS})

add_executable(cli bin/cli.cpp)
target_link_libraries(cli ${KEKMONITORS_LIB_DEPS})

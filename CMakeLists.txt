cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# обратите внимание на аргумент TARGETS у команды conan_basic_setup
include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Добавляем библиотеку, указывая, что она статическая.
add_library(GameLib STATIC 
	src/model.h
	src/model.cpp
	src/extra_data.h
	src/extra_data.cpp
	src/tagged.h
	src/loot_generator.h
	src/loot_generator.cpp
	src/collision_detector.h
	src/collision_detector.cpp
	src/geom.h
	src/model_serialization.h
)

# Добавляем сторонние библиотеки. Указываем видимость PUBLIC, т. к. 
# они должны быть ввидны и в библиотеке GameLib и в зависимостях.
target_include_directories(GameLib PUBLIC CONAN_PKG::zlib)
target_link_libraries(GameLib PUBLIC Threads::Threads CONAN_PKG::boost CONAN_PKG::zlib CONAN_PKG::libpqxx)

# Создаем исполняемый файл game_server
add_executable(game_server
	src/main.cpp
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/request_handler.cpp
	src/request_handler.h
	src/logger.h
	src/logger.cpp
	src/ticker.h
	src/ticker.cpp
	src/postgresql.h
	src/postgresql.cpp
	src/tagged_uuid.h
	src/tagged_uuid.cpp
)

# Связываем game_server с библиотеками
target_link_libraries(game_server GameLib CONAN_PKG::libpqxx)

# Создаем исполняемый файл для тестов
add_executable(game_server_tests
	tests/model_tests.cpp
	tests/loot_generator_tests.cpp
	tests/collision-detector-tests.cpp
	tests/state-serialization-tests.cpp
	tests/main_tests.cpp
)

# Связываем тесты с библиотеками
target_link_libraries(game_server_tests GameLib CONAN_PKG::catch2 CONAN_PKG::libpqxx)

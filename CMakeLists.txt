cmake_minimum_required(VERSION 3.20)
project(XiangqiMSVC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(raylib CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_executable(xiangqi
    src/main.cpp
    src/XiangqiTypes.hpp
    src/XiangqiEngine.hpp
    src/XiangqiEngine.cpp
    src/Database.hpp
    src/Database.cpp
    src/XiangqiUI.hpp
    src/XiangqiUI.cpp
)

target_link_libraries(xiangqi PRIVATE
    raylib
    unofficial::sqlite3::sqlite3
)

if(MSVC)
    target_compile_options(xiangqi PRIVATE /W4 /utf-8)

endif()
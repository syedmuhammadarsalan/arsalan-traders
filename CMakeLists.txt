cmake_minimum_required(VERSION 3.10)
project(ArsalanTraders CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(arsalan_traders
    src/main.cpp
    src/database.cpp
)

target_include_directories(arsalan_traders PRIVATE include)
target_link_libraries(arsalan_traders PRIVATE SQLite::SQLite3 Threads::Threads)

# Copy the frontend next to the built binary so ./public is found at runtime
add_custom_command(TARGET arsalan_traders POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/public $<TARGET_FILE_DIR:arsalan_traders>/public
)

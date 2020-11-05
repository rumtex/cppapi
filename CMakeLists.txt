cmake_minimum_required(VERSION 3.5)

# Set the project name
set(PROJECT loki)
project(${PROJECT} CXX)

# для новых версий gcc
add_definitions(-DPQXX_HIDE_EXP_OPTIONAL)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

if (RELEASE)
    set(DEBUG_LOGS false)
    set(ALLOW_CORS false)
else()
    set(DEBUG_LOGS true)
    set(ALLOW_CORS true)
endif()

if (ALLOW_CORS)
    add_definitions(-DALLOW_CORS)
endif()

if (DEBUG_LOGS)
    add_definitions(-DDEBUG_LOGS)
endif()

###
# sources
###
file(GLOB_RECURSE src src/*.cpp)

# binary
add_executable(${PROJECT} ${src})

# deps
target_link_libraries(${PROJECT}
    pthread
    pqxx
)

# -I/directory/path/
target_include_directories(${PROJECT}
    PRIVATE 
        ${PROJECT_SOURCE_DIR}/include
)

###
# tests
###
file(GLOB_RECURSE tests tests/*.cpp)
add_executable(tests ${tests})

# deps
target_link_libraries(tests
    pthread
)

target_include_directories(tests
    PRIVATE 
        ${PROJECT_SOURCE_DIR}/tests
)
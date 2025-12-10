cmake_minimum_required(VERSION 3.16)
project(VideoPipelineFramework VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")

# Find packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Check for optional dependencies
pkg_check_modules(YAML_CPP yaml-cpp)

# Include directories
include_directories(include)
include_directories(src)

# Core source files
set(FRAMEWORK_SOURCES
    src/core/buffer.cpp
    src/core/block.cpp
    src/core/video_source.cpp
    src/core/video_sink.cpp
    src/core/pipeline_manager.cpp
    src/core/block_registry.cpp
    src/core/config_parser.cpp
    src/core/threading.cpp
    src/core/framework.cpp
    
    src/blocks/test_pattern_source.cpp
    src/blocks/file_sink.cpp
    src/blocks/console_sink.cpp
    
    src/utils/logger.cpp
    src/utils/timer.cpp
)

# Add platform-specific sources if they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/linux/v4l2_source.cpp")
    list(APPEND FRAMEWORK_SOURCES src/platform/linux/v4l2_source.cpp)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/linux/framebuffer_sink.cpp")
    list(APPEND FRAMEWORK_SOURCES src/platform/linux/framebuffer_sink.cpp)
endif()

# Create framework library
add_library(videopipeline STATIC ${FRAMEWORK_SOURCES})
target_link_libraries(videopipeline Threads::Threads)

if(YAML_CPP_FOUND)
    target_link_libraries(videopipeline ${YAML_CPP_LIBRARIES})
    target_include_directories(videopipeline PRIVATE ${YAML_CPP_INCLUDE_DIRS})
    add_definitions(-DHAVE_YAML_CPP)
endif()

# CLI executable
add_executable(pipeline_cli src/main.cpp)
target_link_libraries(pipeline_cli videopipeline)

# Examples
add_subdirectory(examples)

# Tests (if we want to add them later)
# add_subdirectory(tests)

# Install targets
install(TARGETS videopipeline pipeline_cli
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
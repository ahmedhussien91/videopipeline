# Example configurations and demo programs

# Copy example configuration files to build directory
file(GLOB CONFIG_FILES "*.yaml" "*.json" "*.conf")
foreach(CONFIG_FILE ${CONFIG_FILES})
    configure_file(${CONFIG_FILE} ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
endforeach()

# Simple test pattern example
add_executable(simple_test_pattern simple_test_pattern.cpp)
target_link_libraries(simple_test_pattern videopipeline)

# Performance test example
add_executable(performance_test performance_test.cpp)
target_link_libraries(performance_test videopipeline)

# Install examples
install(TARGETS simple_test_pattern performance_test
    RUNTIME DESTINATION bin/examples
)

install(FILES ${CONFIG_FILES}
    DESTINATION share/videopipeline/examples
)
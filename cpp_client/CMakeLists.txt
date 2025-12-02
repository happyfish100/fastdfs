cmake_minimum_required(VERSION 3.12)
project(fastdfs-client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build test programs" OFF)

# Include directories
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Source files
set(SOURCES
    ${SOURCE_DIR}/client.cpp
    ${SOURCE_DIR}/internal/connection.cpp
    ${SOURCE_DIR}/internal/connection_pool.cpp
    ${SOURCE_DIR}/internal/protocol.cpp
    ${SOURCE_DIR}/internal/operations.cpp
)

# Header files
set(HEADERS
    ${INCLUDE_DIR}/fastdfs/client.hpp
    ${INCLUDE_DIR}/fastdfs/types.hpp
    ${INCLUDE_DIR}/fastdfs/errors.hpp
    ${SOURCE_DIR}/internal/connection.hpp
    ${SOURCE_DIR}/internal/connection_pool.hpp
    ${SOURCE_DIR}/internal/protocol.hpp
    ${SOURCE_DIR}/internal/operations.hpp
)

# Create library
add_library(fastdfs-client ${SOURCES} ${HEADERS})

target_include_directories(fastdfs-client
    PUBLIC
        $<BUILD_INTERFACE:${INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${SOURCE_DIR}
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(fastdfs-client PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(fastdfs-client ws2_32)
else()
    target_link_libraries(fastdfs-client pthread)
endif()

# Install rules
install(TARGETS fastdfs-client
    EXPORT fastdfs-client-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY ${INCLUDE_DIR}/fastdfs
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT fastdfs-client-targets
    FILE fastdfs-client-targets.cmake
    NAMESPACE fastdfs-client::
    DESTINATION lib/cmake/fastdfs-client
)

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()


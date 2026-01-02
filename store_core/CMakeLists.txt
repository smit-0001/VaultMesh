cmake_minimum_required(VERSION 3.10)

# Project Name
project(VaultMeshStorage VERSION 1.0)

# Use C++17 Standard (Required for modern features like filesystem)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include Directories (Where to look for .h files)
include_directories(include)
include_directories(include/common)

# Find standard threading library (Good practice for network apps)
find_package(Threads REQUIRED)

# Define the Source Files
set(SOURCES
    src/main.cpp
    src/StorageNode.cpp
    src/Utils.cpp
)

# Create the Executable
add_executable(vault_storage ${SOURCES})

# Link Libraries
target_link_libraries(vault_storage Threads::Threads)

message(STATUS "Build setup complete for VaultMesh Storage Node")
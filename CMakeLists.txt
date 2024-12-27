cmake_minimum_required(VERSION 3.10)
project(TradingSystem)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Boost
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Add executable
add_executable(trading_system 
    main.cpp
)

# Include directories
target_include_directories(trading_system PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    /opt/homebrew/Cellar/boost/1.86.0/include
)

# Link libraries
target_link_libraries(trading_system PRIVATE
    ${Boost_LIBRARIES}
    pthread
)

# Copy all txt and csv files to build directory
file(COPY 
    ${CMAKE_SOURCE_DIR}/miniprices.txt
    ${CMAKE_SOURCE_DIR}/trades.txt
    ${CMAKE_SOURCE_DIR}/mini_market_data.txt
    ${CMAKE_SOURCE_DIR}/inquiries.txt
    ${CMAKE_SOURCE_DIR}/TBonds.csv
    DESTINATION ${CMAKE_BINARY_DIR}
)

# Set working directory for the executable to be the source directory
set_target_properties(trading_system PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
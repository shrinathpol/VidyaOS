cmake_minimum_required(VERSION 3.20.0)

# Tell Zephyr where to find our custom YAML blueprints BEFORE finding Zephyr
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(EmbeddedSystem LANGUAGES C CXX)

zephyr_include_directories(include)

# Add both your main app AND your new kernel driver to the build
target_sources(app PRIVATE 
    src/main.cpp
    src/graphics.cpp
    src/shell.cpp
    src/state.cpp
    src/footprint.cpp
    src/virtual_sensor.c
)
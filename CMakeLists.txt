cmake_minimum_required(VERSION 3.15)
project(phoenix-sdr-utils VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# Find dependencies
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# Phoenix-DSP
find_path(PN_DSP_INCLUDE_DIR pn_dsp.h
    PATHS ../phoenix-dsp/include
    DOC "Phoenix DSP include directory"
)
find_library(PN_DSP_LIBRARY pn_dsp
    PATHS ../phoenix-dsp/lib
    DOC "Phoenix DSP library"
)

# Phoenix-Discovery
find_path(PN_DISCOVERY_INCLUDE_DIR pn_discovery.h
    PATHS ../phoenix-discovery/include
    DOC "Phoenix Discovery include directory"
)
find_library(PN_DISCOVERY_LIBRARY pn_discovery
    PATHS ../phoenix-discovery/build ../phoenix-discovery/lib
    DOC "Phoenix Discovery library"
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${PN_DSP_INCLUDE_DIR}
    ${PN_DISCOVERY_INCLUDE_DIR}
)

# Platform-specific libraries
if(WIN32)
    set(PLATFORM_LIBS ws2_32 winmm iphlpapi)
else()
    set(PLATFORM_LIBS pthread m)
endif()

#=============================================================================
# Executables
#=============================================================================

# I/Q Playback
add_executable(iqr_play
    src/iqr_play.c
    src/iqr_meta.c
    src/iq_recorder.c
)

# Simple AM Receiver (network client)
add_executable(simple_am_receiver
    src/simple_am_receiver.c
)
target_link_libraries(simple_am_receiver
    ${PN_DSP_LIBRARY}
    ${PN_DISCOVERY_LIBRARY}
    ${PLATFORM_LIBS}
)

# GPS Time
add_executable(gps_time
    src/gps_time.c
    src/gps_serial.c
)

# GPS Verification
add_executable(wwv_gps_verify
    src/wwv_gps_verify.c
    src/gps_serial.c
)
if(WIN32)
    target_link_libraries(wwv_gps_verify ws2_32)
endif()

#=============================================================================
# Install targets
#=============================================================================

install(TARGETS iqr_play simple_am_receiver gps_time wwv_gps_verify
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/phoenix-sdr-utils
    FILES_MATCHING PATTERN "*.h"
)

#=============================================================================
# Status messages
#=============================================================================

message(STATUS "Phoenix SDR Utils ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
if(PN_DSP_LIBRARY)
    message(STATUS "Phoenix DSP: ${PN_DSP_LIBRARY}")
else()
    message(WARNING "Phoenix DSP library not found - simple_am_receiver will not build")
endif()
if(PN_DISCOVERY_LIBRARY)
    message(STATUS "Phoenix Discovery: ${PN_DISCOVERY_LIBRARY}")
else()
    message(WARNING "Phoenix Discovery library not found - simple_am_receiver will not build")
endif()

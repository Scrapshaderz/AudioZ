# AudioZ - Audio-Editor und Mastering-Werkzeug
# Copyright (C) 2026 AudioZ-Mitwirkende
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.24)

project(AudioZ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Unter Windows die C++-Laufzeit statisch einbinden: die Setup.exe braucht dann
# kein separates Visual-C++-Redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/utf-8)
endif()

option(AUDIOZ_BUILD_APP "Die AudioZ-Oberflaeche (JUCE) bauen" ON)
option(AUDIOZ_BUILD_TESTS "Tests der Kern-Bibliothek bauen" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Dependencies)

# ---------------------------------------------------------------------------
# Kern-Bibliothek: Bearbeitung, Lautheit, Pitching, BPM, MP3 (ohne GUI)
# ---------------------------------------------------------------------------
add_library(audioz_core STATIC
    src/core/AudioData.cpp
    src/core/Bpm.cpp
    src/core/Dither.cpp
    src/core/Fades.cpp
    src/core/Gain.cpp
    src/core/Limiter.cpp
    src/core/Loudness.cpp
    src/core/LoudnessNormalize.cpp
    src/core/Mp3Encoder.cpp
    src/core/Pitch.cpp
    src/core/Resample.cpp)

target_include_directories(audioz_core PUBLIC src/core)
target_link_libraries(audioz_core PUBLIC audioz_ebur128 audioz_aubio audioz_rubberband)

if(TARGET audioz_lame)
    target_link_libraries(audioz_core PRIVATE audioz_lame)
    target_compile_definitions(audioz_core PRIVATE AUDIOZ_HAS_LAME=1)
else()
    target_compile_definitions(audioz_core PRIVATE AUDIOZ_HAS_LAME=0)
endif()

if(MSVC)
    target_compile_options(audioz_core PRIVATE /W3)
else()
    target_compile_options(audioz_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(AUDIOZ_BUILD_TESTS)
    enable_testing()
    add_executable(audioz_core_tests tests/core_tests.cpp)
    target_link_libraries(audioz_core_tests PRIVATE audioz_core)
    add_test(NAME audioz_core_tests COMMAND audioz_core_tests)
endif()

# ---------------------------------------------------------------------------
# Oberflaeche
# ---------------------------------------------------------------------------
if(AUDIOZ_BUILD_APP)
    add_subdirectory(src/app)
endif()

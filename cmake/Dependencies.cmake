# AudioZ - Audio-Editor und Mastering-Werkzeug
# Copyright (C) 2026 AudioZ-Mitwirkende
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Externe Bibliotheken. Alles ausser LAME wird beim Konfigurieren aus dem
# Quellcode geladen und direkt mitgebaut, so dass unter Windows keine
# DLLs mitgeliefert werden muessen:
#
#   libebur128  (MIT)            Lautheitsmessung nach EBU R128
#   aubio       (GPL-3.0+)       Tempo-Erkennung
#   Rubber Band (GPL-2.0+)       Pitch-Shifting ohne Tempoaenderung
#   LAME        (LGPL-2.0)       MP3-Export (optional, z. B. ueber vcpkg)
#   JUCE        (AGPL-3.0)       Oberflaeche und Audio-Ein-/Ausgabe (siehe src/app)

include(FetchContent)

# Die Quellen koennen lokal ueberschrieben werden (Offline-Build), z. B.:
#   -DFETCHCONTENT_SOURCE_DIR_LIBEBUR128_SRC=/pfad/zu/libebur128

# --- libebur128 -------------------------------------------------------------
FetchContent_Declare(libebur128_src
    GIT_REPOSITORY https://github.com/jiixyj/libebur128.git
    GIT_TAG        v1.2.6
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  audioz-nicht-hinzufuegen)  # nur laden, dessen CMakeLists nicht verwenden
FetchContent_MakeAvailable(libebur128_src)

add_library(audioz_ebur128 STATIC "${libebur128_src_SOURCE_DIR}/ebur128/ebur128.c")
target_include_directories(audioz_ebur128 PUBLIC "${libebur128_src_SOURCE_DIR}/ebur128")
# Eigene Kopie von <sys/queue.h> (fehlt unter Windows)
target_include_directories(audioz_ebur128 SYSTEM BEFORE PRIVATE "${libebur128_src_SOURCE_DIR}/ebur128/queue")
if(MSVC)
    target_compile_definitions(audioz_ebur128 PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
elseif(NOT WIN32)
    target_link_libraries(audioz_ebur128 PUBLIC m)
endif()

# --- aubio ------------------------------------------------------------------
FetchContent_Declare(aubio_src
    GIT_REPOSITORY https://github.com/aubio/aubio.git
    GIT_TAG        0.4.9
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(aubio_src)

set(_aubio_dir "${aubio_src_SOURCE_DIR}/src")
file(GLOB _aubio_sources CONFIGURE_DEPENDS
    "${_aubio_dir}/*.c"
    "${_aubio_dir}/onset/*.c"
    "${_aubio_dir}/pitch/*.c"
    "${_aubio_dir}/spectral/*.c"
    "${_aubio_dir}/tempo/*.c"
    "${_aubio_dir}/temporal/*.c"
    "${_aubio_dir}/utils/*.c")
# Dateiformat-Module, Windows-DLL-Hilfen und Sampler werden nicht gebraucht
list(FILTER _aubio_sources EXCLUDE REGEX "/utils/windll\\.c$")

add_library(audioz_aubio STATIC ${_aubio_sources})
target_include_directories(audioz_aubio PUBLIC "${_aubio_dir}")
target_compile_definitions(audioz_aubio PRIVATE
    HAVE_STDLIB_H=1 HAVE_STDIO_H=1 HAVE_MATH_H=1 HAVE_STRING_H=1
    HAVE_LIMITS_H=1 HAVE_STDARG_H=1 HAVE_ERRNO_H=1 HAVE_C99_VARARGS_MACROS=1)
if(MSVC)
    target_compile_definitions(audioz_aubio PRIVATE _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
    target_compile_options(audioz_aubio PRIVATE /W1)
else()
    target_compile_options(audioz_aubio PRIVATE -w)
    if(NOT WIN32)
        target_link_libraries(audioz_aubio PUBLIC m)
    endif()
endif()

# --- Rubber Band (Single-File-Build) ----------------------------------------
FetchContent_Declare(rubberband_src
    GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG        v4.0.0
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(rubberband_src)

add_library(audioz_rubberband STATIC "${rubberband_src_SOURCE_DIR}/single/RubberBandSingle.cpp")
target_include_directories(audioz_rubberband PUBLIC "${rubberband_src_SOURCE_DIR}")
if(MSVC)
    target_compile_options(audioz_rubberband PRIVATE /W1)
    target_compile_definitions(audioz_rubberband PRIVATE _USE_MATH_DEFINES NOMINMAX)
else()
    target_compile_options(audioz_rubberband PRIVATE -w)
endif()

# --- LAME (optional) --------------------------------------------------------
# Unter Windows liefert vcpkg (Manifest vcpkg.json) das Paket "mp3lame".
find_package(mp3lame CONFIG QUIET)

if(TARGET mp3lame::mp3lame)
    add_library(audioz_lame INTERFACE)
    target_link_libraries(audioz_lame INTERFACE mp3lame::mp3lame)
else()
    find_path(AUDIOZ_LAME_INCLUDE_DIR lame/lame.h)
    find_library(AUDIOZ_LAME_LIBRARY NAMES mp3lame libmp3lame libmp3lame-static)

    if(AUDIOZ_LAME_INCLUDE_DIR AND AUDIOZ_LAME_LIBRARY)
        add_library(audioz_lame INTERFACE)
        target_include_directories(audioz_lame INTERFACE "${AUDIOZ_LAME_INCLUDE_DIR}")
        target_link_libraries(audioz_lame INTERFACE "${AUDIOZ_LAME_LIBRARY}")
    endif()
endif()

if(TARGET audioz_lame)
    message(STATUS "AudioZ: MP3-Export mit LAME aktiviert")
else()
    message(WARNING "AudioZ: LAME nicht gefunden - MP3-Export wird deaktiviert")
endif()

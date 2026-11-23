# SPDX-License-Identifier: GPL-3.0-or-later
# Stadia button icons (README.md): an OpenRAC native mod for Ratchet & Clank and Going Commando,
# forked from xbox-icons. The mod's library (C, on the SDK's openrac/openrac_mod.h), one for both
# games (src/mod.c picks the game; src/rac1 and src/rac2 hold each game's addresses, src/common the
# painting and the sharp cells), and its tests: painting on fonts made in the test, the sharp cells
# against host tables with and without them, and the library as the port loads it, in each game.
# tools/preview.c paints fonts into a PNG, to see the buttons without the game.
#
#   python3 tools/fetch_sdk.py                     # the SDK sdk.lock pins, into .sdk/
#   cmake -S . -B build -G Ninja
#   cmake --build build && ctest --test-dir build
#   python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/stadia_icons
#
# A tag v<version> (the manifest's) makes the workflow publish the release the launcher installs.
cmake_minimum_required(VERSION 3.20)
project(stadia_icons LANGUAGES C)

# The SDK: the release sdk.lock pins (tools/fetch_sdk.py puts it in .sdk/); OPENRAC_SDK=<dir> (an
# unpacked SDK, or sdk/ in a checkout of OpenRAC) or OpenRACMod_DIR=<dir>/cmake for another.
if(NOT OpenRACMod_DIR)
  if(DEFINED OPENRAC_SDK)
    set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" lock)
    string(JSON sdk_version GET "${lock}" version)
    set(OpenRACMod_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${sdk_version}/cmake")
    if(NOT EXISTS "${OpenRACMod_DIR}/OpenRACModConfig.cmake")
      message(FATAL_ERROR "No OpenRAC mod SDK ${sdk_version}: run python3 tools/fetch_sdk.py first "
                          "(or pass -DOPENRAC_SDK=<an unpacked SDK>)")
    endif()
  endif()
endif()
# 1.36: the SDK's sharp cells (openrac_api.sharp_cells, src/common/sharp.h).
find_package(OpenRACMod 1.36 REQUIRED)

set(STADIA_ICONS_COMMON src/common/paint.c)
set(STADIA_ICONS_GAMES src/rac1/rac1.c src/rac2/rac2.c)
openrac_add_mod(stadia_icons MANIFEST openrac-mod.json
                SOURCES src/mod.c src/common/sharp.c ${STADIA_ICONS_COMMON} ${STADIA_ICONS_GAMES})
target_include_directories(stadia_icons PRIVATE src/common)
if(NOT MSVC)
  target_compile_options(stadia_icons PRIVATE -Wall -Wextra)
endif()

function(stadia_icons_tool target)
  set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
  target_include_directories(${target} PRIVATE src/common tests/common)
  if(NOT MSVC)
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()
endfunction()

enable_testing()
add_executable(stadia_icons_paint_test tests/common/paint_test.c ${STADIA_ICONS_COMMON})
stadia_icons_tool(stadia_icons_paint_test)
add_test(NAME stadia_icons_paint COMMAND stadia_icons_paint_test)

add_executable(stadia_icons_sharp_test tests/common/sharp_test.c src/common/sharp.c
                                       ${STADIA_ICONS_COMMON})
stadia_icons_tool(stadia_icons_sharp_test)
target_link_libraries(stadia_icons_sharp_test PRIVATE OpenRAC::mod)
add_test(NAME stadia_icons_sharp COMMAND stadia_icons_sharp_test)

# The library as the port loads it, against a host table and game memory of the test's own, in each
# game; OPENRAC_RAC1_LEVEL / OPENRAC_RAC2_LEVEL=<decompiled level> try the game's own fonts too.
foreach(game rac1 rac2)
  add_executable(stadia_icons_${game}_load_test tests/${game}/load_test.c ${STADIA_ICONS_COMMON}
                                                ${STADIA_ICONS_GAMES})
  stadia_icons_tool(stadia_icons_${game}_load_test)
  target_link_libraries(stadia_icons_${game}_load_test PRIVATE OpenRAC::mod ${CMAKE_DL_LIBS})
  add_dependencies(stadia_icons_${game}_load_test stadia_icons)
  add_test(NAME stadia_icons_${game}_load
           COMMAND stadia_icons_${game}_load_test $<TARGET_FILE:stadia_icons>)
endforeach()

add_executable(stadia_icons_preview tools/preview.c ${STADIA_ICONS_COMMON})
stadia_icons_tool(stadia_icons_preview)

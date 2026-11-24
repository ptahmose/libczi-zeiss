# SPDX-FileCopyrightText: 2026 Carl Zeiss Microscopy GmbH
#
# SPDX-License-Identifier: LGPL-3.0-or-later

if (NOT WIN32)
  message(FATAL_ERROR "The standalone JXR benchmark currently requires Windows.")
endif()

enable_language(RC)
add_executable(JxrBenchmark main.cpp fixture.rc)
target_include_directories(JxrBenchmark PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(JxrBenchmark PRIVATE JxrDecodeStatic bcrypt)
set_target_properties(JxrBenchmark PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

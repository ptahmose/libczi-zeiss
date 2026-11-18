# SPDX-FileCopyrightText: 2026 Carl Zeiss Microscopy GmbH
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# The bundled TurboJPEG build force-includes turbojpeg_defines.h to give its
# symbols libCZI-specific names and avoid collisions with another JPEG library
# linked into the same application.
#
# Since TurboJPEG 3.2, turbojpeg.h defines tj3Init() as a function-like macro
# that calls tj3InitVersion() with the header's API version. In turbojpeg.c,
# upstream undefines tj3Init before defining the older compatibility function.
# That #undef also removes our forced-included rename, leaving the compatibility
# symbol unprefixed even though tj3InitVersion is successfully renamed.
#
# Restore the mapping immediately after the #undef so the compatibility
# declaration, definition, and subsequent calls in this file use the private
# name too. Read that name from RENAME_HEADER to keep this patch consistent with
# the rest of the symbol mappings. FILE identifies the downloaded turbojpeg.c.
# Repeated application is harmless. An unexpected source layout fails explicitly
# so an upstream update cannot silently leave this symbol unprefixed.
#
# Before the patch, we look for this sequence in turbojpeg.c:
#   #undef tj3Init
#   DLLEXPORT tjhandle tj3Init(int initType);
#
# After the patch (assuming the header maps tj3Init to libCZI_tj3Init):
#   #undef tj3Init
#   #define tj3Init libCZI_tj3Init
#   DLLEXPORT tjhandle tj3Init(int initType);
#
# The preprocessor then renames the declaration above and the later function
# definition to libCZI_tj3Init.
file(READ "${RENAME_HEADER}" _header)
string(REGEX MATCH "#define[ \t]+tj3Init[ \t]+([A-Za-z_][A-Za-z0-9_]*)" _mapping "${_header}")
if(NOT _mapping)
  message(FATAL_ERROR "No tj3Init mapping found in ${RENAME_HEADER}")
endif()
set(_replacement "#undef tj3Init\n#define tj3Init ${CMAKE_MATCH_1}")

file(READ "${FILE}" _source)
string(FIND "${_source}" "${_replacement}" _already_patched)
if(NOT _already_patched EQUAL -1)
  return()
endif()

set(_original "#undef tj3Init\nDLLEXPORT tjhandle tj3Init(int initType);")
string(FIND "${_source}" "${_original}" _position)
if(_position EQUAL -1)
  message(FATAL_ERROR "Unexpected tj3Init implementation in ${FILE}")
endif()

string(REPLACE "${_original}"
  "${_replacement}\nDLLEXPORT tjhandle tj3Init(int initType);"
  _patched "${_source}")
file(WRITE "${FILE}" "${_patched}")

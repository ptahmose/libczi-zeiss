
#[[
if (LIBCZI_BUILD_WITH_CURLSUPPORT)
  # include "CPR"
  #  - required libraries: zlib & LibSSH2
  #         .\vcpkg.exe install zlib --triplet x64-windows-static
  #                       find_package(ZLIB REQUIRED)
  #                       target_link_libraries(main PRIVATE ZLIB::ZLIB)
  #
  #         .\vcpkg.exe install libssh2 --triplet x64-windows-static
  #                       find_package(Libssh2 CONFIG REQUIRED)
  #                       target_link_libraries(main PRIVATE Libssh2::libssh2)
  include(FetchContent)
  set (BUILD_SHARED_LIBS OFF)
  FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                           GIT_TAG db351ffbbadc6c4e9239daaa26e9aefa9f0ec82d) # The commit hash for 1.8.3. Replace with the latest from: https://github.com/libcpr/cpr/releases
  FetchContent_MakeAvailable(cpr)

  find_package(Libssh2 QUIET)
endif()
]]#

 find_package(cpr CONFIG REQUIRED)
#    target_link_libraries(main PRIVATE cpr::cpr)


# build "JxrDecode" as an "object library" (cf. https://cmake.org/cmake/help/v3.0/command/add_library.html) -> we want it to be merged into the
#   static libCIZ-library
add_library(StreamsObjects OBJECT streamsFactory.h cprInputStream.h cprInputStream.cpp streamsFactory.cpp)

set_target_properties(StreamsObjects  PROPERTIES CXX_STANDARD 17)

if (LIBCZI_BUILD_CZICMD_WITH_CURLSUPPORT)
  target_link_libraries(StreamsObjects PRIVATE cpr::cpr)
  set(STREAMSOBJECTS_HAS_CURL 1)
else()
  set(STREAMSOBJECTS_HAS_CURL 0)
endif()

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/streams_Config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/streams_Config.h"
  )

target_include_directories(StreamsObjects PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# shared libraries need PIC
set_property(TARGET StreamsObjects PROPERTY POSITION_INDEPENDENT_CODE ON)


 include(CTest)

 find_package(Python QUIET)

 if (Python_FOUND)
  add_test(NAME CZICmd_Tests 
            COMMAND ${Python_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/run_tests.py"  "$<TARGET_FILE:CZIcmd>"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

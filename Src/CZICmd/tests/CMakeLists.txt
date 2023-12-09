 include(CTest)

 find_package(Python QUIET)

 if (Python_FOUND)
  add_test(NAME CZICmd_Tests 
            COMMAND ${Python_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/run_tests.py"  --czicmd-executable "$<TARGET_FILE:CZIcmd>" --testcases "${CMAKE_CURRENT_SOURCE_DIR}/testcases.txt"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

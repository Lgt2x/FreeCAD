# SPDX-License-Identifier: LGPL-2.1-or-later

# Discover the existing Init.py / InitGui.py registrations when CTest runs,
# after the application has been built. No second list of suites to maintain.
if(NOT BUILD_TEST)
    return()
endif()
set(PY_EXECUTABLE "$<TARGET_FILE:FreeCADMainCmd>")
if(BUILD_GUI)
    set(GUI_EXECUTABLE "$<TARGET_FILE:FreeCADMain>")
endif()
configure_file(${CMAKE_CURRENT_LIST_DIR}/PythonTests.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/PythonTests.configured.cmake @ONLY)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/PythonTests-$<CONFIG>.cmake"
     INPUT "${CMAKE_CURRENT_BINARY_DIR}/PythonTests.configured.cmake")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/PythonTests.cmake"
    "if(NOT CTEST_CONFIGURATION_TYPE)\n"
    "  set(CTEST_CONFIGURATION_TYPE \"${CMAKE_BUILD_TYPE}\")\n"
    "endif()\n"
    "include(\"${CMAKE_CURRENT_BINARY_DIR}/PythonTests-\${CTEST_CONFIGURATION_TYPE}.cmake\")\n")
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
             "${CMAKE_CURRENT_BINARY_DIR}/PythonTests.cmake")

add_test(NAME FreeCAD-PY-TestCTestCache
         COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/TestCTestCache.py")
set_tests_properties(FreeCAD-PY-TestCTestCache PROPERTIES
                    ENVIRONMENT "CMAKE_COMMAND=${CMAKE_COMMAND}")

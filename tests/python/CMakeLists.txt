# SPDX-License-Identifier: LGPL-2.1-or-later

# These suites are still available through the installed Test workbench. Keep
# the flat module names so existing registrations and FreeCAD -t commands work.
set(CoreTest_SRCS
    base/BaseTests.py
    base/StringHasher.py
    base/UnicodeTests.py
    base/UnitTests.py
    app/Document.py
    app/FreeCADInitTests.py
    app/Metadata.py
    app/TestCrashReporter.py
    app/TestIntPairList.py
    app/TestLinkSuppression.py
    app/TestPythonSyntax.py
    gui/GuiDocument.py
    gui/TestCoinNodeSnapshots.py
    gui/TestCornerAxisCrossVisual.py
    gui/TestGraphicsViewWrapping.py
    gui/TestGroupVisibilityRestoreGui.py
    gui/TestMDIView.py
    gui/TestPySideUic.py
    gui/TestRubberbandSelection.py
    gui/TestSelectionVisual.py
    gui/TestTreeSelection.py
    gui/TestView3DFramebufferCapture.py
    gui/TestViewProviderLink.py
    gui/Workbench.py
    integration/GuiRecompute.py
    integration/Recompute.py
    integration/testmakeWireString.py
    performance/AutoSaverStress.py
    performance/RunAutoSaverStress.py
    performance/TestPerf.py
)

set(CoreTestData_SRCS
    app/TestData/basic_metadata.xml
    app/TestData/bad_root_node.xml
    app/TestData/bad_xml.xml
    app/TestData/bad_version.xml
    app/TestData/content_items.xml
    app/TestData/DXFSample.dxf
)

if(INSTALL_PREFER_SYMLINKS)
    set(_test_copy_command create_symlink)
else()
    set(_test_copy_command copy)
endif()

set(_core_test_outputs)
foreach(_source IN LISTS CoreTest_SRCS CoreTestData_SRCS)
    get_filename_component(_name "${_source}" NAME)
    if(_source IN_LIST CoreTestData_SRCS)
        set(_destination "${CMAKE_BINARY_DIR}/Mod/Test/TestData")
        set(_install_destination "Mod/Test/TestData")
    else()
        set(_destination "${CMAKE_BINARY_DIR}/Mod/Test")
        set(_install_destination "Mod/Test")
    endif()
    set(_input "${CMAKE_CURRENT_SOURCE_DIR}/${_source}")
    set(_output "${_destination}/${_name}")
    add_custom_command(
        OUTPUT "${_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_destination}"
        COMMAND "${CMAKE_COMMAND}" -E ${_test_copy_command} "${_input}" "${_output}"
        MAIN_DEPENDENCY "${_input}"
        COMMENT "Copying ${_input} to ${_output}"
        VERBATIM
    )
    list(APPEND _core_test_outputs "${_output}")
    target_sources(Test PRIVATE "${_input}")
    install(FILES "${_input}" DESTINATION "${_install_destination}")
endforeach()

add_custom_target(CoreTestSources DEPENDS ${_core_test_outputs})
add_dependencies(TestSources CoreTestSources)

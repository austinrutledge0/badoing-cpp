cmake_minimum_required(VERSION 3.16)

project(badoing-cpp VERSION 1.0.0
    HOMEPAGE_URL "https://github.com/austinrutledge0/badoing-cpp"
    LANGUAGES CXX)

include(GNUInstallDirs)

option(BADOING_ENABLE_SOUND "Use Qt Multimedia for the embedded sound" ON)
option(BADOING_BUILD_TESTS "Build the small, dependency-free logic tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Prefer Qt 6, but support distributions which still ship Qt 5.15.
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
if(Qt6_FOUND)
    set(BADOING_QT_PACKAGE Qt6)
else()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets)
    set(BADOING_QT_PACKAGE Qt5)
endif()

set(BADOING_HAS_MULTIMEDIA 0)
if(BADOING_ENABLE_SOUND)
    find_package(${BADOING_QT_PACKAGE} QUIET COMPONENTS Multimedia)
    if(TARGET ${BADOING_QT_PACKAGE}::Multimedia)
        set(BADOING_HAS_MULTIMEDIA 1)
    else()
        message(STATUS "Qt Multimedia was not found; building without sound")
    endif()
endif()

add_executable(badoing-pet
    src/main.cpp
    src/themestartup.cpp
    src/themestartup.h
    src/petwindow.cpp
    src/petwindow.h
    src/settings.cpp
    src/settings.h
    src/loginstartup.cpp
    src/loginstartup.h
    src/petlogic.cpp
    src/petlogic.h
    resources.qrc
)
set_target_properties(badoing-pet PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "BADOING"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.badoing.pet"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
)
target_include_directories(badoing-pet PRIVATE src)
target_link_libraries(badoing-pet PRIVATE
    ${BADOING_QT_PACKAGE}::Core
    ${BADOING_QT_PACKAGE}::Gui
    ${BADOING_QT_PACKAGE}::Widgets
)
if(BADOING_HAS_MULTIMEDIA)
    target_link_libraries(badoing-pet PRIVATE ${BADOING_QT_PACKAGE}::Multimedia)
endif()
if(WIN32 AND BADOING_QT_PACKAGE STREQUAL "Qt5")
    target_link_libraries(badoing-pet PRIVATE Qt5::WinMain)
endif()
if(WIN32)
    target_link_libraries(badoing-pet PRIVATE ole32 shell32 uuid)
endif()
target_compile_definitions(badoing-pet PRIVATE
    BADOING_VERSION="${PROJECT_VERSION}"
    BADOING_HAS_MULTIMEDIA=${BADOING_HAS_MULTIMEDIA}
)
target_compile_options(badoing-pet PRIVATE
    "$<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>"
    "$<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/utf-8>"
)

install(TARGETS badoing-pet
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES packaging/badoing-pet.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    install(FILES packaging/badoing-pet.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
endif()
install(FILES README.md INSTALL.md LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES docs/PORTING.md DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs)

set(badoing_deploy_default OFF)
if((WIN32 OR APPLE) AND BADOING_QT_PACKAGE STREQUAL "Qt6"
        AND COMMAND qt_generate_deploy_app_script)
    set(badoing_deploy_default ON)
endif()
option(BADOING_DEPLOY_QT "Bundle Qt runtime libraries at installation (Qt 6.3+)" ${badoing_deploy_default})
if(BADOING_DEPLOY_QT)
    if(NOT BADOING_QT_PACKAGE STREQUAL "Qt6" OR NOT COMMAND qt_generate_deploy_app_script)
        message(FATAL_ERROR "BADOING_DEPLOY_QT requires Qt 6.3 or newer. Use OFF to install only the application.")
    endif()
    if(Qt6_VERSION VERSION_LESS "6.5")
        qt_generate_deploy_app_script(TARGET badoing-pet FILENAME_VARIABLE badoing_deploy_script)
    else()
        qt_generate_deploy_app_script(TARGET badoing-pet OUTPUT_SCRIPT badoing_deploy_script)
    endif()
    install(SCRIPT "${badoing_deploy_script}")
    if(MSVC)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
        include(InstallRequiredSystemLibraries)
    endif()
endif()

if(BADOING_BUILD_TESTS)
    enable_testing()
    add_executable(badoing-logic-tests
        tests/test_petlogic.cpp
        src/petlogic.cpp
    )
    set_target_properties(badoing-logic-tests PROPERTIES AUTOMOC OFF AUTORCC OFF)
    target_include_directories(badoing-logic-tests PRIVATE src)
    target_compile_options(badoing-logic-tests PRIVATE
        "$<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>"
        "$<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/utf-8>"
    )
    add_test(NAME pet-logic COMMAND badoing-logic-tests)

    # Exercise the real widget and embedded assets, without QtTest or a real display.
    add_executable(badoing-integration-tests
        tests/test_integration.cpp
        src/petwindow.cpp src/petwindow.h
        src/settings.cpp src/settings.h
        src/loginstartup.cpp src/loginstartup.h
        src/petlogic.cpp src/petlogic.h
        resources.qrc
    )
    target_include_directories(badoing-integration-tests PRIVATE src)
    target_link_libraries(badoing-integration-tests PRIVATE
        ${BADOING_QT_PACKAGE}::Core
        ${BADOING_QT_PACKAGE}::Gui
        ${BADOING_QT_PACKAGE}::Widgets
    )
    if(BADOING_HAS_MULTIMEDIA)
        target_link_libraries(badoing-integration-tests PRIVATE ${BADOING_QT_PACKAGE}::Multimedia)
    endif()
    if(WIN32)
        target_link_libraries(badoing-integration-tests PRIVATE ole32 shell32 uuid)
    endif()
    target_compile_definitions(badoing-integration-tests PRIVATE
        BADOING_HAS_MULTIMEDIA=${BADOING_HAS_MULTIMEDIA}
    )
    target_compile_options(badoing-integration-tests PRIVATE
        "$<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>"
        "$<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/utf-8>"
    )
    add_test(NAME pet-integration COMMAND badoing-integration-tests)
    set_tests_properties(pet-integration PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30
    )
    add_test(NAME pet-cli COMMAND ${CMAKE_COMMAND}
        "-DPET_EXECUTABLE=$<TARGET_FILE:badoing-pet>"
        "-DTEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/cli-test"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.cmake")
    set_tests_properties(pet-cli PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 30)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_library(badoing-failing-theme MODULE tests/failing_theme.cpp)
        set_target_properties(badoing-failing-theme PROPERTIES AUTOMOC OFF AUTORCC OFF)
        add_test(NAME pet-theme-startup COMMAND ${CMAKE_COMMAND}
            "-DPET_EXECUTABLE=$<TARGET_FILE:badoing-pet>"
            "-DFAILURE_LIBRARY=$<TARGET_FILE:badoing-failing-theme>"
            "-DTEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/theme-test"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_theme_startup.cmake")
        set_tests_properties(pet-theme-startup PROPERTIES TIMEOUT 30)
    endif()
endif()

set(CPACK_PACKAGE_NAME "badoing-cpp")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packages")
set(CPACK_PACKAGE_VENDOR "BADOING contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "An animated desktop pet")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME "badoing-cpp-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "badoing-cpp")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)

message(STATUS "BADOING: using ${BADOING_QT_PACKAGE}; sound=${BADOING_HAS_MULTIMEDIA}")

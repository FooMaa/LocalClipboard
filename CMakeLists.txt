cmake_minimum_required(VERSION 3.21)
project(LocalClipboard VERSION 2.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)
include(cmake/QtMacOSOpenGL.cmake)
localclipboard_fix_macos_opengl()
add_library(monocypher STATIC third_party/monocypher/monocypher.c)
set_target_properties(monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(monocypher PUBLIC third_party/monocypher)
add_library(clipboard_core STATIC
    src/Profile.h src/Profile.cpp src/SecureProtocol.h src/SecureProtocol.cpp
    src/ClipboardPayload.h src/ClipboardPayload.cpp src/SyncEngine.h src/SyncEngine.cpp)
set_target_properties(clipboard_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(clipboard_core PUBLIC src)
target_link_libraries(clipboard_core PUBLIC Qt6::Core Qt6::Network Qt6::Widgets PRIVATE monocypher)
qt_add_executable(LocalClipboard MANUAL_FINALIZATION src/main.cpp
    src/ClipboardSync.h src/ClipboardSync.cpp src/Registration.h src/Registration.cpp
    src/Autostart.h src/Autostart.cpp)
target_link_libraries(LocalClipboard PRIVATE clipboard_core)
if(ANDROID)
    target_sources(LocalClipboard PRIVATE src/AndroidBridge.h src/AndroidBridge.cpp)
    set_target_properties(LocalClipboard PROPERTIES
        QT_ANDROID_PACKAGE_NAME "org.localclipboard"
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_TARGET_SDK_VERSION 35
        QT_ANDROID_MIN_SDK_VERSION 26
        QT_ANDROID_VERSION_CODE 20000
    )
elseif(APPLE)
    set_target_properties(LocalClipboard PROPERTIES MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.localclipboard"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
elseif(WIN32)
    set_target_properties(LocalClipboard PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
qt_finalize_executable(LocalClipboard)
# On Android qt_add_executable creates a MODULE library packaged by androiddeployqt.
if(UNIX AND NOT APPLE AND NOT ANDROID)
    install(PROGRAMS "$<TARGET_FILE:LocalClipboard>" DESTINATION bin RENAME localclipboard)
    install(FILES packaging/linux/localclipboard.desktop DESTINATION share/applications)
else()
    install(TARGETS LocalClipboard BUNDLE DESTINATION . RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
endif()
include(CTest)
if(BUILD_TESTING AND NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(clipboard_tests tests/CoreTests.cpp)
    target_link_libraries(clipboard_tests PRIVATE clipboard_core Qt6::Test)
    qt_add_executable(app_tests tests/AppTests.cpp src/ClipboardSync.h src/ClipboardSync.cpp
        src/Registration.h src/Registration.cpp src/Autostart.h src/Autostart.cpp)
    target_link_libraries(app_tests PRIVATE clipboard_core Qt6::Test)
    add_test(NAME clipboard_core COMMAND clipboard_tests)
    add_test(NAME background_window COMMAND app_tests)
    set_tests_properties(clipboard_core background_window PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
if(NOT ANDROID)
    add_custom_target(installers
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package" -t native -b "${CMAKE_BINARY_DIR}" -c "$<CONFIG>"
        DEPENDS LocalClipboard USES_TERMINAL)
else()
    add_custom_target(installers
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package" -t android -b "${CMAKE_BINARY_DIR}" -c "$<CONFIG>"
        DEPENDS apk USES_TERMINAL)
endif()

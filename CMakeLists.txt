cmake_minimum_required(VERSION 3.21.1)
project(LocalClipboard VERSION 2.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets Multimedia Concurrent)
include(cmake/QtMacOSOpenGL.cmake)
localclipboard_fix_macos_opengl()
# Keep the QR implementation local and static on every platform.
function(localclipboard_add_zxing)
    set(BUILD_SHARED_LIBS OFF)
    set(ZXING_READERS ON)
    set(ZXING_WRITERS OLD)
    set(ZXING_EXPERIMENTAL_API OFF)
    set(CMAKE_AUTOMOC OFF)
    add_subdirectory(third_party/zxing/core EXCLUDE_FROM_ALL)
endfunction()
localclipboard_add_zxing()
add_library(monocypher STATIC third_party/monocypher/monocypher.c)
set_target_properties(monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(monocypher PUBLIC third_party/monocypher)
add_library(clipboard_core STATIC
    src/Profile.h src/Profile.cpp src/SecureProtocol.h src/SecureProtocol.cpp
    src/ClipboardPayload.h src/ClipboardPayload.cpp src/SyncEngine.h src/SyncEngine.cpp
    src/PendingFiles.h src/PendingFiles.cpp)
target_sources(clipboard_core PRIVATE src/I18n.h src/I18n.cpp src/Russian.inc)
set_target_properties(clipboard_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(clipboard_core PUBLIC src)
target_link_libraries(clipboard_core PUBLIC Qt6::Core Qt6::Network Qt6::Widgets Qt6::Concurrent PRIVATE monocypher)
add_library(clipboard_qr STATIC src/QrCode.h src/QrCode.cpp src/QrScanner.h src/QrScanner.cpp)
set_target_properties(clipboard_qr PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(clipboard_qr PUBLIC src)
target_link_libraries(clipboard_qr PUBLIC clipboard_core Qt6::Multimedia PRIVATE ZXing::ZXing)
qt_add_executable(LocalClipboard MANUAL_FINALIZATION src/main.cpp
    src/ClipboardSync.h src/ClipboardSync.cpp src/Registration.h src/Registration.cpp
    src/Autostart.h src/Autostart.cpp)
target_sources(LocalClipboard PRIVATE src/Ui.h src/Ui.cpp)
if(NOT ANDROID AND NOT IOS)
    target_sources(LocalClipboard PRIVATE src/Uninstall.h src/Uninstall.cpp)
    qt_add_resources(LocalClipboard uninstall_resources PREFIX "/uninstall" BASE packaging/macos FILES packaging/macos/uninstall.sh)
endif()
set(LOCALCLIPBOARD_ICON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons")
qt_add_resources(LocalClipboard brand_icons PREFIX "/icons" BASE assets/icons FILES
    assets/icons/localclipboard-32.png assets/icons/localclipboard-64.png
    assets/icons/localclipboard-128.png assets/icons/localclipboard-256.png
    assets/icons/localclipboard-tray-32.png assets/icons/localclipboard-tray-64.png)
target_link_libraries(LocalClipboard PRIVATE clipboard_core clipboard_qr)
qt_add_resources(LocalClipboard third_party_licenses PREFIX "/licenses" BASE third_party FILES
    third_party/monocypher/LICENCE.md third_party/zxing/LICENSE third_party/zxing/NOTICES.txt)
target_compile_definitions(LocalClipboard PRIVATE LOCALCLIPBOARD_VERSION="${PROJECT_VERSION}")
if(IOS)
    # Static iOS Qt kits also auto-import FFmpeg, whose codecs may be absent.
    # QR capture uses Apple's AVFoundation backend; keep other plugin types enabled.
    if(NOT TARGET Qt6::QDarwinMediaPlugin)
        message(FATAL_ERROR "The Qt iOS kit must include the native AVFoundation multimedia plugin")
    endif()
    qt_import_plugins(LocalClipboard INCLUDE_BY_TYPE multimedia Qt6::QDarwinMediaPlugin)
    enable_language(OBJCXX)
    target_sources(LocalClipboard PRIVATE ios/Assets.xcassets)
    set_source_files_properties(ios/Assets.xcassets PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set(LOCALCLIPBOARD_IOS_BUNDLE_ID "org.localclipboard.ios" CACHE STRING "Unique iOS app identifier for your Apple team")
    set(LOCALCLIPBOARD_IOS_TEAM "" CACHE STRING "Apple development team ID (optional for unsigned CI builds)")
    target_sources(LocalClipboard PRIVATE src/IOSBridge.h src/IOSBridge.mm)
    set_source_files_properties(src/IOSBridge.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(LocalClipboard PRIVATE "-framework UIKit" "-framework PhotosUI" "-framework UniformTypeIdentifiers")
    set_target_properties(LocalClipboard PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "${LOCALCLIPBOARD_IOS_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_VERSION "20100"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${LOCALCLIPBOARD_IOS_BUNDLE_ID}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${LOCALCLIPBOARD_IOS_TEAM}"
        XCODE_GENERATE_SCHEME TRUE)
elseif(ANDROID)
    target_sources(clipboard_qr PRIVATE src/AndroidUi.h src/AndroidUi.cpp)
    target_sources(LocalClipboard PRIVATE src/AndroidBridge.h src/AndroidBridge.cpp)
    set_target_properties(LocalClipboard PROPERTIES
        QT_ANDROID_PACKAGE_NAME "org.localclipboard"
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_TARGET_SDK_VERSION 35
        QT_ANDROID_MIN_SDK_VERSION 26
        QT_ANDROID_VERSION_CODE 20100
    )
elseif(APPLE)
    target_sources(LocalClipboard PRIVATE assets/icons/localclipboard.icns)
    set_source_files_properties(assets/icons/localclipboard.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(LocalClipboard PROPERTIES MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE localclipboard.icns
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.localclipboard"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
elseif(WIN32)
    enable_language(RC)
    configure_file(packaging/windows/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
    target_sources(LocalClipboard PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
    set_target_properties(LocalClipboard PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
qt_finalize_executable(LocalClipboard)
# On Android qt_add_executable creates a MODULE library packaged by androiddeployqt.
if(UNIX AND NOT APPLE AND NOT ANDROID)
    install(PROGRAMS "$<TARGET_FILE:LocalClipboard>" DESTINATION bin RENAME localclipboard)
    install(FILES packaging/linux/localclipboard.desktop DESTINATION share/applications)
    install(FILES assets/icons/localclipboard.svg DESTINATION share/icons/hicolor/scalable/apps)
    foreach(size 16 24 32 48 64 128 256 512)
        install(FILES "assets/icons/localclipboard-${size}.png"
            DESTINATION "share/icons/hicolor/${size}x${size}/apps" RENAME localclipboard.png)
    endforeach()
else()
    install(TARGETS LocalClipboard BUNDLE DESTINATION . RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
endif()
include(CTest)
if(BUILD_TESTING AND NOT ANDROID AND NOT IOS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(clipboard_tests tests/CoreTests.cpp)
    target_link_libraries(clipboard_tests PRIVATE clipboard_core Qt6::Test)
    qt_add_executable(app_tests tests/AppTests.cpp src/ClipboardSync.h src/ClipboardSync.cpp
        src/Registration.h src/Registration.cpp src/Autostart.h src/Autostart.cpp)
    target_sources(app_tests PRIVATE src/Ui.h src/Ui.cpp src/Uninstall.h src/Uninstall.cpp)
    qt_add_resources(app_tests test_uninstall_resources PREFIX "/uninstall" BASE packaging/macos FILES packaging/macos/uninstall.sh)
    target_link_libraries(app_tests PRIVATE clipboard_core clipboard_qr Qt6::Test)
    add_test(NAME clipboard_core COMMAND clipboard_tests)
    add_test(NAME background_window COMMAND app_tests)
    set_tests_properties(clipboard_core background_window PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1")
endif()
if(IOS)
    add_custom_target(installers
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package" -t ios -b "${CMAKE_BINARY_DIR}" -c "$<CONFIG>"
        DEPENDS LocalClipboard USES_TERMINAL)
elseif(NOT ANDROID)
    add_custom_target(installers
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package" -t native -b "${CMAKE_BINARY_DIR}" -c "$<CONFIG>"
        DEPENDS LocalClipboard USES_TERMINAL)
else()
    add_custom_target(installers
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package" -t android -b "${CMAKE_BINARY_DIR}" -c "$<CONFIG>"
        DEPENDS apk USES_TERMINAL)
endif()

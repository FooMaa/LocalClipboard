cmake_minimum_required(VERSION 3.21.1)
project(LocalClipboard VERSION 2.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
option(LOCALCLIPBOARD_SANITIZERS "Instrument project and bundled code with address/undefined behavior sanitizers" OFF)
option(LOCALCLIPBOARD_FUZZING "Build the Clang libFuzzer protocol/payload harness" OFF)
option(LOCALCLIPBOARD_PRODUCTION "Require a release configuration and a maintained Qt baseline" OFF)
if(LOCALCLIPBOARD_SANITIZERS OR LOCALCLIPBOARD_FUZZING)
    if(MSVC OR ANDROID OR IOS)
        message(FATAL_ERROR "Run sanitizer/fuzzer checks with a native Clang/GCC desktop build")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
if(LOCALCLIPBOARD_FUZZING)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "libFuzzer requires Clang")
    endif()
    add_compile_options(-fsanitize=fuzzer-no-link)
endif()
if(MSVC)
    add_compile_options(/utf-8)
endif()
set(LOCALCLIPBOARD_QT_COMPONENTS Core Gui Network Widgets Concurrent Quick QuickControls2)
if(NOT ANDROID)
    list(APPEND LOCALCLIPBOARD_QT_COMPONENTS Multimedia)
endif()
find_package(Qt6 REQUIRED COMPONENTS ${LOCALCLIPBOARD_QT_COMPONENTS})
if(LOCALCLIPBOARD_PRODUCTION)
    if(Qt6_VERSION VERSION_LESS 6.11.2)
        message(FATAL_ERROR "Production builds require Qt 6.11.2 or newer; review current Qt advisories before publishing")
    endif()
    if(NOT CMAKE_CONFIGURATION_TYPES AND (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug"))
        message(FATAL_ERROR "Production builds require Release, RelWithDebInfo or MinSizeRel")
    endif()
endif()
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
    src/Profile.h src/Profile.cpp src/SecretStore.h src/SecretStore.cpp src/SecureProtocol.h src/SecureProtocol.cpp
    src/ClipboardPayload.h src/ClipboardPayload.cpp src/SyncEngine.h src/SyncEngine.cpp
    src/PendingFiles.h src/PendingFiles.cpp src/FileStorage.h src/FileStorage.cpp
    src/ConnectionInfo.h src/ConnectionInfo.cpp)
target_sources(clipboard_core PRIVATE src/I18n.h src/I18n.cpp src/Russian.inc)
set_target_properties(clipboard_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(clipboard_core PUBLIC src)
target_link_libraries(clipboard_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network PRIVATE monocypher)
add_library(clipboard_qr STATIC src/QrCode.h src/QrCode.cpp src/QrScanner.h src/QrScanner.cpp)
set_target_properties(clipboard_qr PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(clipboard_qr PUBLIC src)
target_link_libraries(clipboard_qr PUBLIC clipboard_core Qt6::Widgets PRIVATE ZXing::ZXing)
if(NOT ANDROID)
    target_link_libraries(clipboard_qr PUBLIC Qt6::Multimedia)
endif()
add_library(clipboard_session STATIC src/ClipboardSession.h src/ClipboardSession.cpp src/Autostart.h src/Autostart.cpp)
set_target_properties(clipboard_session PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(clipboard_session PUBLIC clipboard_core Qt6::Widgets Qt6::Concurrent)
qt_add_executable(LocalClipboard MANUAL_FINALIZATION src/main.cpp)
if(NOT ANDROID AND NOT IOS)
    qt_add_resources(LocalClipboard uninstall_resources PREFIX "/uninstall" BASE packaging/macos FILES packaging/macos/uninstall.sh)
endif()
set(LOCALCLIPBOARD_ICON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons")
qt_add_resources(LocalClipboard brand_icons PREFIX "/icons" BASE assets/icons FILES
    assets/icons/localclipboard-32.png assets/icons/localclipboard-64.png
    assets/icons/localclipboard-128.png assets/icons/localclipboard-256.png
    assets/icons/localclipboard-tray-32.png assets/icons/localclipboard-tray-64.png)
target_link_libraries(LocalClipboard PRIVATE clipboard_session clipboard_qr)
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
    target_sources(clipboard_session PRIVATE src/IOSBridge.h src/IOSBridge.mm)
    set_source_files_properties(src/IOSBridge.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(clipboard_session PRIVATE "-framework UIKit" "-framework PhotosUI" "-framework UniformTypeIdentifiers")
    set_target_properties(LocalClipboard PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "${LOCALCLIPBOARD_IOS_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_VERSION "20200"
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
    target_sources(clipboard_session PRIVATE src/AndroidBridge.h src/AndroidBridge.cpp)
    set_target_properties(LocalClipboard PROPERTIES
        QT_ANDROID_PACKAGE_NAME "org.localclipboard"
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_TARGET_SDK_VERSION 36
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_VERSION_CODE 20200
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
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
option(LOCALCLIPBOARD_MOBILE_PREVIEW "Build an isolated preview of the shared Quick interface on desktop" OFF)
add_library(clipboard_ui STATIC src/AppController.h src/AppController.cpp src/AppUi.h src/AppUi.cpp
    src/AppWindow.h src/AppWindow.cpp src/Ui.h src/Ui.cpp)
set_target_properties(clipboard_ui PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT ANDROID AND NOT IOS)
    target_sources(clipboard_ui PRIVATE src/Uninstall.h src/Uninstall.cpp)
endif()
target_link_libraries(clipboard_ui PUBLIC clipboard_session clipboard_qr Qt6::Quick Qt6::QuickControls2)
set(LOCALCLIPBOARD_QML_FILES qml/Main.qml qml/Navigation.qml qml/BodyText.qml qml/ActionButton.qml qml/Editor.qml
    qml/Card.qml qml/ScrollPage.qml qml/SettingSwitch.qml qml/Theme.js)
target_link_libraries(LocalClipboard PRIVATE clipboard_ui)
qt_add_resources(LocalClipboard mobile_ui PREFIX "/app" BASE qml FILES ${LOCALCLIPBOARD_QML_FILES})
if(LOCALCLIPBOARD_MOBILE_PREVIEW AND NOT ANDROID AND NOT IOS)
    qt_add_executable(mobile_preview tests/MobilePreview.cpp)
    target_link_libraries(mobile_preview PRIVATE clipboard_ui)
    qt_add_resources(mobile_preview mobile_ui PREFIX "/app" BASE qml FILES ${LOCALCLIPBOARD_QML_FILES})
    qt_add_resources(mobile_preview preview_icons PREFIX "/icons" BASE assets/icons FILES assets/icons/localclipboard-64.png)
    qt_add_resources(mobile_preview preview_uninstall PREFIX "/uninstall" BASE packaging/macos FILES packaging/macos/uninstall.sh)
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
    qt_add_executable(app_tests tests/AppTests.cpp)
    qt_add_resources(app_tests test_uninstall_resources PREFIX "/uninstall" BASE packaging/macos FILES packaging/macos/uninstall.sh)
    target_link_libraries(app_tests PRIVATE clipboard_ui Qt6::Test)
    add_test(NAME clipboard_core COMMAND clipboard_tests)
    add_test(NAME background_window COMMAND app_tests)
    set_tests_properties(clipboard_core background_window PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1")
    qt_add_executable(mobile_tests tests/MobileTests.cpp)
    target_link_libraries(mobile_tests PRIVATE clipboard_ui Qt6::Test)
    qt_add_resources(mobile_tests mobile_ui PREFIX "/app" BASE qml FILES ${LOCALCLIPBOARD_QML_FILES})
    qt_add_resources(mobile_tests test_uninstall PREFIX "/uninstall" BASE packaging/macos FILES packaging/macos/uninstall.sh)
    qt_add_resources(mobile_tests test_icons PREFIX "/icons" BASE assets/icons FILES assets/icons/localclipboard-64.png)
    add_test(NAME mobile_interface COMMAND mobile_tests)
    set_tests_properties(mobile_interface PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
if(LOCALCLIPBOARD_FUZZING)
    add_executable(protocol_fuzz tests/ProtocolFuzz.cpp)
    target_link_libraries(protocol_fuzz PRIVATE clipboard_core)
    set(LOCALCLIPBOARD_FUZZER_LIBRARY "" CACHE FILEPATH "Optional upstream libFuzzer archive for Clang kits without a bundled runtime")
    if(LOCALCLIPBOARD_FUZZER_LIBRARY)
        target_link_libraries(protocol_fuzz PRIVATE "${LOCALCLIPBOARD_FUZZER_LIBRARY}")
    else()
        target_compile_options(protocol_fuzz PRIVATE -fsanitize=fuzzer)
        target_link_options(protocol_fuzz PRIVATE -fsanitize=fuzzer)
    endif()
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

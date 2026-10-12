cmake_minimum_required(VERSION 3.21)
project(LocalClipboard VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)
qt_add_executable(LocalClipboard MANUAL_FINALIZATION
    src/main.cpp
    src/ClipboardSync.h src/ClipboardSync.cpp
)
target_link_libraries(LocalClipboard PRIVATE Qt6::Core Qt6::Network Qt6::Widgets)
if(ANDROID)
    set_target_properties(LocalClipboard PROPERTIES
        QT_ANDROID_PACKAGE_NAME "org.localclipboard"
        QT_ANDROID_PERMISSIONS "android.permission.INTERNET;android.permission.ACCESS_NETWORK_STATE"
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
install(TARGETS LocalClipboard BUNDLE DESTINATION . RUNTIME DESTINATION bin)

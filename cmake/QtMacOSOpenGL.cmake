# Qt 6.8.3's FindWrapOpenGL adds AGL, which is absent from the macOS 26 SDK.
# AGL's Carbon API is unused by this application; retain the OpenGL framework.
function(localclipboard_fix_macos_opengl)
    if(NOT APPLE OR IOS OR NOT TARGET WrapOpenGL::WrapOpenGL)
        return()
    endif()
    get_target_property(qt_opengl_libraries WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
    if(qt_opengl_libraries)
        list(FILTER qt_opengl_libraries EXCLUDE REGEX "(^|[/ ])AGL(\\.framework(/.*)?|$)")
        set_property(TARGET WrapOpenGL::WrapOpenGL PROPERTY
            INTERFACE_LINK_LIBRARIES "${qt_opengl_libraries}")
    endif()
endfunction()

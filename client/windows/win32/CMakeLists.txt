include(FetchContent)

set(wxBUILD_SHARED OFF)
set(wxBUILD_TESTS OFF)
set(wxBUILD_SAMPLES OFF)
set(wxBUILD_DEMOS OFF)
set(wxBUILD_BENCHMARKS OFF)
set(wxBUILD_INSTALL OFF)

if(MSVC)
    if(NOT CMAKE_MSVC_RUNTIME_LIBRARY OR CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
        set(deskhub_wx_static_runtime OFF)
    else()
        set(deskhub_wx_static_runtime ON)
    endif()
    set(wxBUILD_USE_STATIC_RUNTIME ${deskhub_wx_static_runtime} CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(wxwidgets
    URL https://github.com/wxWidgets/wxWidgets/releases/download/v3.3.1/wxWidgets-3.3.1.tar.bz2
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(wxwidgets)

add_executable(deskhub_app WIN32
    Deskhub.rc
    main.cpp
    MainFrame.cpp
    SourcePickerDialog.cpp
    TerminalWindow.cpp
)
target_include_directories(deskhub_app PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(deskhub_app PRIVATE deskhub_win_view wx::core wx::base)
set_target_properties(deskhub_app PROPERTIES OUTPUT_NAME "Deskhub")
deskhub_target_warnings(deskhub_app)
if(MSVC)
    target_compile_options(deskhub_app PRIVATE /sdl)
    set_property(TARGET deskhub_app APPEND_STRING PROPERTY LINK_FLAGS
        " /MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
endif()

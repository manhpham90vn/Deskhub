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
    URL_HASH SHA256=f936c8d694f9c49a367a376f99c751467150a4ed7cbf8f4723ef19b2d2d9998d
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
deskhub_target_hardening(deskhub_app)
if(MSVC)
    set_property(TARGET deskhub_app APPEND_STRING PROPERTY LINK_FLAGS
        " /MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
endif()

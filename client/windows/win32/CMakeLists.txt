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

set(deskhub_wx_url "https://github.com/wxWidgets/wxWidgets/releases/download/v3.3.1/wxWidgets-3.3.1.tar.bz2")
set(deskhub_wx_sha256 "f936c8d694f9c49a367a376f99c751467150a4ed7cbf8f4723ef19b2d2d9998d")
set(deskhub_wx_archive "${CMAKE_BINARY_DIR}/_deps/wxWidgets-3.3.1.tar.bz2")

foreach(deskhub_wx_attempt RANGE 1 5)
    if(EXISTS "${deskhub_wx_archive}")
        file(SHA256 "${deskhub_wx_archive}" deskhub_wx_existing_sha256)
        if(deskhub_wx_existing_sha256 STREQUAL deskhub_wx_sha256)
            break()
        endif()
        file(REMOVE "${deskhub_wx_archive}")
    endif()
    if(deskhub_wx_attempt GREATER 1)
        math(EXPR deskhub_wx_sleep "${deskhub_wx_attempt} * 5")
        message(STATUS "Retrying wxWidgets download in ${deskhub_wx_sleep}s (attempt ${deskhub_wx_attempt}/5)...")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep "${deskhub_wx_sleep}")
    endif()
    file(DOWNLOAD "${deskhub_wx_url}" "${deskhub_wx_archive}"
        TLS_VERIFY ON
        INACTIVITY_TIMEOUT 60
        STATUS deskhub_wx_status
    )
    if(EXISTS "${deskhub_wx_archive}")
        file(SHA256 "${deskhub_wx_archive}" deskhub_wx_existing_sha256)
        if(deskhub_wx_existing_sha256 STREQUAL deskhub_wx_sha256)
            break()
        endif()
        file(REMOVE "${deskhub_wx_archive}")
    endif()
endforeach()

if(EXISTS "${deskhub_wx_archive}")
    set(deskhub_wx_fetch_url "${deskhub_wx_archive}")
else()
    set(deskhub_wx_fetch_url "${deskhub_wx_url}")
endif()

FetchContent_Declare(wxwidgets
    URL "${deskhub_wx_fetch_url}"
    URL_HASH SHA256=${deskhub_wx_sha256}
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

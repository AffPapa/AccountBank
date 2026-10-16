# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

cmake_minimum_required(VERSION 3.25...3.31)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(cmake/validate_special_target.cmake)
include(cmake/version.cmake)
desktop_app_parse_version(Telegram/build/version)

if (NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(configuration_types_init 1)
endif()

project(Telegram
    LANGUAGES C CXX
    VERSION ${desktop_app_version_cmake}
    DESCRIPTION "AccountBank unofficial messaging client"
    HOMEPAGE_URL "https://github.com/AffPapa/AccountBank"
)

if (APPLE)
    enable_language(OBJC OBJCXX)
    # AccountBank does not offer the macOS on-device translation UI. Keep
    # fresh Release configurations free of the Swift 6 runtime dependency.
    set(DESKTOP_APP_DISABLE_SWIFT6 ON CACHE BOOL
        "Disable local on-device translation in AccountBank." FORCE)
endif()

if (configuration_types_init
    AND CMAKE_CONFIGURATION_TYPES
    AND NOT MinSizeRel IN_LIST CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES};MinSizeRel" CACHE STRING "" FORCE)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Telegram)

get_filename_component(third_party_loc "Telegram/ThirdParty" REALPATH)
get_filename_component(submodules_loc "Telegram" REALPATH)
get_filename_component(cmake_helpers_loc "cmake" REALPATH)

if (NOT DESKTOP_APP_USE_PACKAGED AND WIN32)
    set(Python3_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/python/Scripts/python)
endif()

include(cmake/variables.cmake)
# Optional, ignored local API credentials. Keep them out of command lines and Git.
include("${CMAKE_CURRENT_SOURCE_DIR}/accountbank-api.local.cmake" OPTIONAL)
# AccountBank must never launch Telegram Desktop's upstream updater. A build
# configured with a special target may otherwise enable it by default.
if (NOT DESKTOP_APP_DISABLE_AUTOUPDATE)
    message(FATAL_ERROR "AccountBank requires DESKTOP_APP_DISABLE_AUTOUPDATE=ON")
endif()
include(cmake/nice_target_sources.cmake)
include(cmake/target_compile_options_if_exists.cmake)
include(cmake/target_link_frameworks.cmake)
include(cmake/target_link_options_if_exists.cmake)
include(cmake/init_target.cmake)
include(cmake/generate_target.cmake)
include(cmake/nuget.cmake)
include(cmake/validate_d3d_compiler.cmake)
include(cmake/target_prepare_qrc.cmake)

include(cmake/options.cmake)
include(cmake/external/qt/package.cmake)

set(desktop_app_skip_libs
    glibmm
    variant
)

add_subdirectory(cmake)
add_subdirectory(Telegram)

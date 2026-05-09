cmake_minimum_required(VERSION 3.28)

project(BooruBox VERSION 0.1.1 LANGUAGES CXX)

option(BOORUBOX_BUILD_TESTS "Build BooruBox tests" ON)
set(BOORUBOX_SANITIZERS "" CACHE STRING
    "Comma-separated sanitizer list, for example address,undefined or thread")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

include(FetchContent)

find_package(CURL REQUIRED)
find_package(SQLite3 QUIET)
find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_MakeAvailable(nlohmann_json)

# Generate a single-source-of-truth version header for C++ code to read.
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app/Version.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/app/Version.hpp
  @ONLY
)

add_library(boorubox_core
  src/app/App.cpp
  src/app/Config.cpp
  src/download/DownloadManager.cpp
  src/download/FilenameTemplate.cpp
  src/http/CurlHandle.cpp
  src/http/HttpClient.cpp
  src/index/JsonlArchiveIndex.cpp
  src/index/SqliteArchiveIndex.cpp
  src/model/DownloadJob.cpp
  src/preview/PreviewCache.cpp
  src/providers/DanbooruProvider.cpp
  src/providers/E621Provider.cpp
  src/providers/GelbooruProvider.cpp
  src/providers/ProviderUtil.cpp
  src/providers/SafebooruProvider.cpp
  src/util/ContentFilter.cpp
  src/util/Logger.cpp
  src/util/PathUtil.cpp
  src/util/RateLimiter.cpp
  src/util/StringUtil.cpp
)

target_include_directories(boorubox_core
  PUBLIC
    src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(boorubox_core
  PUBLIC
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

if(SQLite3_FOUND)
  target_link_libraries(boorubox_core PUBLIC SQLite::SQLite3)
  target_compile_definitions(boorubox_core PRIVATE BOORUBOX_WITH_SQLITE=1)
endif()

if(MSVC)
  target_compile_options(boorubox_core PRIVATE /W4)
else()
  target_compile_options(boorubox_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

function(boorubox_apply_sanitizers target)
  if(BOORUBOX_SANITIZERS AND NOT MSVC)
    target_compile_options(${target} PRIVATE
      -fsanitize=${BOORUBOX_SANITIZERS}
      -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE
      -fsanitize=${BOORUBOX_SANITIZERS}
    )
  endif()
endfunction()

boorubox_apply_sanitizers(boorubox_core)

add_executable(boorubox
  src/main.cpp
  src/gui/GalleryPage.cpp
  src/gui/ImagePreviewWidget.cpp
  src/gui/LogsPage.cpp
  src/gui/MainWindow.cpp
  src/gui/QtUtil.cpp
  src/gui/QueuePage.cpp
  src/gui/ResultsPage.cpp
  src/gui/SafetyUtil.cpp
  src/gui/SearchPage.cpp
  src/gui/SettingsPage.cpp
  src/gui/ThumbnailLoader.cpp
)

target_link_libraries(boorubox
  PRIVATE
    boorubox_core
    Qt6::Concurrent
    Qt6::Widgets
)

if(MSVC)
  target_compile_options(boorubox PRIVATE /W4)
else()
  target_compile_options(boorubox PRIVATE -Wall -Wextra -Wpedantic)
endif()
boorubox_apply_sanitizers(boorubox)

if(BOORUBOX_BUILD_TESTS)
  include(CTest)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.3
  )
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

  set(BOORUBOX_TEST_SOURCES
    tests/test_url_building.cpp
    tests/test_json_parsing.cpp
    tests/test_filename_sanitize.cpp
    tests/test_blacklist.cpp
    tests/test_config_validation.cpp
    tests/test_archive_index.cpp
    tests/test_download_manager.cpp
    tests/test_http_client.cpp
  )
  if(SQLite3_FOUND)
    list(APPEND BOORUBOX_TEST_SOURCES tests/test_sqlite_archive_index.cpp)
  endif()

  add_executable(boorubox_tests ${BOORUBOX_TEST_SOURCES})
  target_link_libraries(boorubox_tests
    PRIVATE
      boorubox_core
      Catch2::Catch2WithMain
  )
  boorubox_apply_sanitizers(boorubox_tests)
  include(Catch)
  catch_discover_tests(boorubox_tests)
endif()

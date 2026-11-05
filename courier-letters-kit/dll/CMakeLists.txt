cmake_minimum_required(VERSION 3.21)
project(CourierLettersKit VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CommonLibSSE-NG (alandtse's CommonLibVR-ng), shared with the SOES NG and Socketing builds
set(COMMONLIB $ENV{HOME}/src/SOES-NG/extern/CommonLibVR-ng)
include(${COMMONLIB}/cmake/CommonLibSSE.cmake)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${COMMONLIB} commonlib)
find_package(spdlog REQUIRED CONFIG)

add_commonlibsse_plugin(${PROJECT_NAME} AUTHOR zeroregard SOURCES src/Main.cpp)
target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /EHsc)
target_link_options(${PROJECT_NAME} PRIVATE /INCREMENTAL:NO /OPT:REF /OPT:ICF)
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE spdlog::spdlog)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

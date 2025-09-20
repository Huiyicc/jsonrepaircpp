cmake_minimum_required(VERSION 3.29)
project(jsonrepair VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/utf-8)
endif()


add_library(libjsonrepair STATIC
    jsonrepair/jsonrepair.cpp
    jsonrepair/jsonrepair.hpp
)

set_target_properties(libjsonrepair PROPERTIES
    OUTPUT_NAME "jsonrepair"
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

install(FILES jsonrepair/jsonrepair.h
    DESTINATION include/jsonrepair
)

install(TARGETS libjsonrepair
    EXPORT jsonrepairTargets
    LIBRARY DESTINATION lib      # Linux/macOS
    ARCHIVE DESTINATION lib      # .a / .lib
    INCLUDES DESTINATION include
)

install(EXPORT jsonrepairTargets
    FILE jsonrepairTargets.cmake
    NAMESPACE jsonrepair::
    DESTINATION lib/cmake/jsonrepair
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/jsonrepairConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/jsonrepairConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/jsonrepairConfig.cmake"
    INSTALL_DESTINATION lib/cmake/jsonrepair
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/jsonrepairConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/jsonrepairConfigVersion.cmake"
    DESTINATION lib/cmake/jsonrepair
)

# test
add_executable(jsonrepair_test jsonrepair_test.cpp)
target_link_libraries(jsonrepair_test PRIVATE libjsonrepair)
target_include_directories(jsonrepair_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/jsonrepair)
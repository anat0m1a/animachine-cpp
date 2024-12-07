cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VERSION)
string(STRIP "${VERSION}" VERSION)

project(Animachine VERSION ${VERSION})

# Generate .pc file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/animachine.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/animachine.pc
    @ONLY
)

# Install the .pc file
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/animachine.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMEDIAINFO REQUIRED libmediainfo)

add_subdirectory(src)
include_directories(${CMAKE_SOURCE_DIR}/src)

add_subdirectory(cpp-inquirer)
target_link_libraries(animachine PRIVATE Inquirer)
target_include_directories(animachine PRIVATE ${CMAKE_SOURCE_DIR}/cpp-inquirer/src)

target_include_directories(animachine PRIVATE ${LIBMEDIAINFO_INCLUDE_DIRS})
target_link_libraries(animachine PRIVATE ${LIBMEDIAINFO_LIBRARIES})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(animachine PRIVATE DEBUG)
endif()

if(APPLE)
    target_link_libraries(animachine PRIVATE "-framework Foundation")
endif()
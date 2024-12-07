set(SOURCES
    animachine.cpp
    util.cpp
    audio.cpp
    video.cpp
    text.cpp
    file.cpp
)

add_executable(animachine ${SOURCES})

target_include_directories(animachine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
cmake_minimum_required(VERSION 3.29)
project(ReplayClipper)

set(CMAKE_CXX_STANDARD 20)

add_executable(ReplayClipper)

target_sources(
        ReplayClipper
        PRIVATE
        src/Logging.h
        src/Stopwatch.h
        src/DirectoryNavigator.cpp
        src/Application.cpp
        src/AudioPlayer.cpp
        src/Clipper.cpp
        src/VideoStream.cpp
        src/Main.cpp
)

################################################################################
# RESOURCE COPYING
################################################################################

add_custom_target(
        replay_clipper_copy_resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/res
        $<TARGET_FILE_DIR:ReplayClipper>/res
)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)

find_package(glad CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glad::glad)

find_package(FFMPEG REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARIES})

find_package(RtAudio CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE RtAudio::rtaudio)
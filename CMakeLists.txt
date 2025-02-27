#establish project
cmake_minimum_required(VERSION 3.10.0)
set(my_executable KinesisEngine)

#get c/cpp specifications
project(${my_executable} VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

#main SRCs
set(CODE_DIR ${PROJECT_SOURCE_DIR}/src)
set (SRCS
  ${CODE_DIR}/main.cpp
  )

#IMGUI SRCs
set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/lib/imgui)
include_directories(${IMGUI_DIR})
include_directories(${IMGUI_DIR}/backends)
set(IMGUI_SRCS
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

add_executable(${my_executable} 
    MACOSX_BUNDLE
    ${SRCS}
    ${IMGUI_SRCS}
    )

include(cmake/CPM.cmake)

find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(${my_executable} ${Vulkan_LIBRARIES})

CPMAddPackage("gh:glfw/glfw#3.4")
target_link_libraries(${my_executable} glfw)

CPMAddPackage("gh:g-truc/glm#1.0.1")
target_link_libraries(${my_executable} glm)

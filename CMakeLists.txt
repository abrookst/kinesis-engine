#establish project
cmake_minimum_required(VERSION 3.10.0)
set(my_executable KinesisEngine)

#get c/cpp specifications
project(${my_executable} VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

#might have to add more to SRCs here
add_executable(${my_executable} main.cpp)

include(cmake/CPM.cmake)

find_package(Vulkan REQUIRED)
if(VULKAN_FOUND)
    include_directories(${Vulkan_INCLUDE_DIRS})
    target_link_libraries(${my_executable} ${Vulkan_LIBRARIES})
else(VULKAN_FOUND)
    message("Couldn't find Vulkan... Make sure your Enviroment Variables are set correctly!")
endif(VULKAN_FOUND)

CPMAddPackage("gh:glfw/glfw#3.4")
target_link_libraries(${my_executable} glfw)

CPMAddPackage("gh:g-truc/glm#1.0.1")
target_link_libraries(${my_executable} glm)



#include "kinesis.h"
#include "window.h"
#include "GUI.h"

using namespace Kinesis;

void Kinesis::initialize(int width, int height){
    Kinesis::Window::initialize(width, height);
    Kinesis::GUI::initialize();
}

// Main code
bool Kinesis::run()
{
    // Main loop
    //TODO: add init check
    if(glfwWindowShouldClose(Kinesis::Window::window)){
        vkDeviceWaitIdle(g_Device);
        Kinesis::Window::cleanup();
        return false;
    }
    else{
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        glfwPollEvents();
        Kinesis::GUI::update_imgui();
        Kinesis::Window::drawFrame();
        

        // Main Vulkan/Rendering Logic here
        //Kinesis::Rendering::update_render() or something
        

        return true;
    }
}

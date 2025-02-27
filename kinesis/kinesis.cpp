#include "kinesis.h"
#include "window.h"
#include "GUI.h"

using namespace Kinesis;

void Kinesis::initialize(){
    Kinesis::Window::initialize();
    Kinesis::GUI::initialize();
}

// Main code
bool Kinesis::run()
{
    // Main loop
    while (!glfwWindowShouldClose(Kinesis::Window::window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        glfwPollEvents();

        Kinesis::GUI::update_imgui();

        // Main Vulkan/Rendering Logic here
        //Kinesis::Rendering::update_render() or something
    }

    // Cleanup
    Kinesis::Window::cleanup();

    return 0;
}

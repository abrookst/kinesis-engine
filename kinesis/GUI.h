#ifndef GUI_H
#define GUI_H

#include "kinesis.h"
#include "window.h"

namespace Kinesis::GUI
{
    bool show_demo;
    bool show_scene;
    bool show_rendering;
    bool show_gameobject;
    bool show_toolbar;
    bool dark_mode;
    ImVec4 clear_color;

    static void HelpMarker(const char *desc)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::BeginItemTooltip())
        {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void toolbar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Windows"))
            {
                ImGui::MenuItem("Scene Viewer", "", &show_scene);
                ImGui::MenuItem("Render Editor", "", &show_rendering);
                ImGui::MenuItem("GameObject Manager", "", &show_gameobject);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Options"))
            {
                if (ImGui::BeginMenu("ImGui"))
                {
                    ImGui::MenuItem("Show ImGui Demo", "", &show_demo);
                    ImGui::MenuItem("Dark Mode", "", &dark_mode);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    void gameobject_manager(bool *p_open)
    {
        if (!ImGui::Begin("GameObject Manager", p_open))
        {
            ImGui::End();
            return;
        }
    }

    void scene_viewer(bool *p_open)
    {
        if (!ImGui::Begin("Scene Viewer", p_open))
        {
            ImGui::End();
            return;
        }
    }

    void rendering_editor(bool *p_open)
    {
        if (!ImGui::Begin("Rendering Editor", p_open))
        {
            ImGui::End();
            return;
        }
    }

    void initialize()
    {
        // Our state
        ImGui::StyleColorsDark();
        show_demo = false;
        show_scene = false;
        show_rendering = false;
        show_gameobject = false;
        show_toolbar = true;
        dark_mode = true;
        clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    }

    void update_imgui()
    {
        // Resize swap chain?
        int fb_width, fb_height;
        glfwGetFramebufferSize(Kinesis::Window::window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (g_SwapChainRebuild || g_MainWindowData.Width != fb_width || g_MainWindowData.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, fb_width, fb_height, g_MinImageCount);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }
        if (glfwGetWindowAttrib(Kinesis::Window::window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // show windows/darkmode if enabled
        show_demo ? ImGui::ShowDemoWindow(&show_demo) : void();
        show_toolbar ? toolbar() : void();
        show_gameobject ? gameobject_manager(&show_gameobject) : void();
        show_scene ? scene_viewer(&show_scene) : void();
        show_rendering ? rendering_editor(&show_rendering) : void();
        dark_mode ? ImGui::StyleColorsDark() : ImGui::StyleColorsLight();

        // Rendering
        ImGui::Render();
        ImDrawData *draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized)
        {
            ImGui_ImplVulkanH_Window *imguiwin = Kinesis::Window::wd;
            imguiwin->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            imguiwin->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            imguiwin->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            imguiwin->ClearValue.color.float32[3] = clear_color.w;
            Kinesis::Window::FrameRender(imguiwin, draw_data);
            Kinesis::Window::FramePresent(imguiwin);
        }
    }
}

#endif
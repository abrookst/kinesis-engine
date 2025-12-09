#include "GUI.h"
#include <iostream>

namespace Kinesis::GUI
{
    // Initialize global variables
    bool show_demo = false;
    bool show_scene = false;
    bool show_rendering = false;
    bool show_gameobject = false;
    bool show_toolbar = true;
    bool dark_mode = true;
    bool raytracing_available = false;
    bool enable_raytracing_pass = false;
    int gbuffer_debug_mode = 0; // 0=Off, 1=Position, 2=Normal, 3=Albedo, 4=Properties
    int samples_per_pixel = 8; // Default SPP
    int max_ray_depth = 12; // Default max bounces
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 0.0f);

    void HelpMarker(const char *desc)
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
                // Conditionally add the raytracing toggle
                if (Kinesis::GUI::raytracing_available) // <<< ADD CHECK
                {
                    // Use a MenuItem with a checkmark, controlled by enable_raytracing_pass
                    if (ImGui::BeginMenu("Raytracing")) // Renamed for clarity
                    {
                        ImGui::MenuItem("Enable Raytracing Pass", nullptr, &enable_raytracing_pass); // <<< ADD THIS LINE
                        ImGui::EndMenu();
                    }
                }
                
                // G-Buffer Debug Visualization
                if (ImGui::BeginMenu("G-Buffer Debug"))
                {
                    ImGui::RadioButton("Off", &gbuffer_debug_mode, 0);
                    ImGui::RadioButton("Position", &gbuffer_debug_mode, 1);
                    ImGui::RadioButton("Normal", &gbuffer_debug_mode, 2);
                    ImGui::RadioButton("Albedo", &gbuffer_debug_mode, 3);
                    ImGui::RadioButton("Properties", &gbuffer_debug_mode, 4);
                    ImGui::EndMenu();
                }

                // Existing ImGui Options Submenu
                if (ImGui::BeginMenu("ImGui Style")) // Renamed for clarity
                {
                    ImGui::MenuItem("Show ImGui Demo", "", &show_demo);
                    ImGui::MenuItem("Dark Mode", "", &dark_mode);
                    ImGui::EndMenu();
                }
                // Removed duplicate ImGui menu entry
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
        
        if (ImGui::CollapsingHeader("Ray Tracing Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Enable Ray Tracing", &enable_raytracing_pass);
            
            ImGui::Separator();
            
            // Samples Per Pixel slider
            ImGui::SliderInt("Samples Per Pixel", &samples_per_pixel, 1, 32);
            HelpMarker("Number of rays traced per pixel. Higher = better quality but slower.");
            
            // Max Ray Depth slider
            ImGui::SliderInt("Max Ray Depth", &max_ray_depth, 1, 20);
            HelpMarker("Maximum number of ray bounces. Higher = more accurate indirect lighting but slower.");
            
            ImGui::Separator();
            
            ImGui::Text("Performance: %.1f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        }
        
        if (ImGui::CollapsingHeader("G-Buffer Debug"))
        {
            ImGui::RadioButton("Off", &gbuffer_debug_mode, 0);
            ImGui::RadioButton("Position", &gbuffer_debug_mode, 1);
            ImGui::RadioButton("Normal", &gbuffer_debug_mode, 2);
            ImGui::RadioButton("Albedo", &gbuffer_debug_mode, 3);
            ImGui::RadioButton("Properties", &gbuffer_debug_mode, 4);
        }
        
        ImGui::End();
    }

    void initialize()
    {
        ImGui::StyleColorsDark();
        show_demo = false;
        show_scene = false;
        show_rendering = false;
        show_gameobject = false;
        show_toolbar = true;
        dark_mode = true;
        clear_color = ImVec4(0.45f, 0.55f, 0.60f, 0.0f);
        enable_raytracing_pass = false;
        gbuffer_debug_mode = 0;
        samples_per_pixel = 8;
        max_ray_depth = 12;
    }

    void update_imgui()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        show_demo ? ImGui::ShowDemoWindow(&show_demo) : void();
        show_toolbar ? toolbar() : void();
        show_gameobject ? gameobject_manager(&show_gameobject) : void();
        show_scene ? scene_viewer(&show_scene) : void();
        show_rendering ? rendering_editor(&show_rendering) : void();
        dark_mode ? ImGui::StyleColorsDark() : ImGui::StyleColorsLight();

        ImGui::Render();
    }
}
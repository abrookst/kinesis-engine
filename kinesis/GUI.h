#ifndef GUI_H
#define GUI_H

#include "kinesis.h"

namespace Kinesis::GUI
{
    // Toggle flags for GUI windows
    extern bool show_demo;
    extern bool show_scene;
    extern bool show_rendering;
    extern bool show_gameobject;
    extern bool show_toolbar;
    extern bool dark_mode;
    extern ImVec4 clear_color;

    /**
     * @brief Displays a help marker with a tooltip description
     * @param desc The description text to show in the tooltip
     */
    void HelpMarker(const char *desc);

    /**
     * @brief Renders the main toolbar menu
     */
    void toolbar();

    /**
     * @brief Renders the GameObject Manager window
     * @param p_open Pointer to the window's visibility state
     */
    void gameobject_manager(bool *p_open);

    /**
     * @brief Renders the Scene Viewer window
     * @param p_open Pointer to the window's visibility state
     */
    void scene_viewer(bool *p_open);

    /**
     * @brief Renders the Rendering Editor window
     * @param p_open Pointer to the window's visibility state
     */
    void rendering_editor(bool *p_open);

    /**
     * @brief Initializes the GUI system with default settings
     */
    void initialize();

    /**
     * @brief Updates and renders all ImGui components
     */
    void update_imgui();
}

#endif
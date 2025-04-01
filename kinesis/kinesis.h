#ifndef KINESIS_H
#define KINESIS_H

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <stdio.h>  // printf, fprintf
#include <stdlib.h> // abort
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Volk headers
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
#define VOLK_IMPLEMENTATION
#include <volk.h>
#endif

// #define APP_USE_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
#endif

namespace Kinesis
{
    // Data
    static VkAllocationCallbacks *g_Allocator = nullptr;
    static VkInstance g_Instance = VK_NULL_HANDLE;
    static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
    static VkDevice g_Device = VK_NULL_HANDLE;
    static uint32_t g_QueueFamily = (uint32_t)-1;
    static VkQueue g_Queue = VK_NULL_HANDLE;
    static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
    static VkPipelineCache g_PipelineCache = VK_NULL_HANDLE;
    static VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;

    static ImGui_ImplVulkanH_Window g_MainWindowData;
    static uint32_t g_MinImageCount = 2;
    static bool g_SwapChainRebuild = false;

    //namespace functions
    bool run();
    void initialize(int width = 600, int height = 600);
}

#endif
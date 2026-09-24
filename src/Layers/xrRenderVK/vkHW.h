#pragma once

#include "Layers/xrRender/HWCaps.h"
#include "xrCore/ModuleLookup.hpp"
#include "CommonTypes.h"

namespace xray::render::RENDER_NAMESPACE
{
class CHW
    : public pureAppActivate,
      public pureAppDeactivate
{
public:
    CHW();
    ~CHW();

    void CreateDevice(SDL_Window* sdlWnd);
    void DestroyDevice();

    void Reset();

    void SetPrimaryAttributes(u32& windowFlags);

    IRender::RenderContext GetCurrentContext() const;
    int MakeContextCurrent(IRender::RenderContext context) const;

    static std::pair<u32, u32> GetSurfaceSize();
    DeviceState GetDeviceState() const;

public:
    void BeginScene();
    void EndScene();
    void Present();

    void ClearColor(float r, float g, float b, float a);

public:
    void OnAppActivate() override;
    void OnAppDeactivate() override;

public:
    void BeginPixEvent(pcstr name) const;
    void EndPixEvent() const;

    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

private:
    void CreateInstance();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapchain();
    void CreateImageViews();
    void CreateDefaultRenderPass();
    void CreateFramebuffers();
    void CreateCommandPoolAndBuffers();
    void CreateSyncObjects();
    void CleanupSwapchain();
    void RecreateSwapchain();

    bool ThisInstanceIsGlobal() const;

public:
    static constexpr auto IMM_CTX_ID = 0;
    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

    CHWCaps Caps;

    u32 BackBufferCount{ 2 };
    u32 CurrentBackBuffer{ 0 };

    SDL_Window* m_window{};

    // Core Vulkan state
    VkInstance m_instance{ VK_NULL_HANDLE };
    VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
    VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };
    VkPhysicalDeviceProperties m_deviceProperties{};
    VkDevice m_device{ VK_NULL_HANDLE };

    uint32_t m_graphicsQueueFamily{ UINT32_MAX };
    uint32_t m_presentQueueFamily{ UINT32_MAX };
    VkQueue m_graphicsQueue{ VK_NULL_HANDLE };
    VkQueue m_presentQueue{ VK_NULL_HANDLE };

    VkSwapchainKHR m_swapchain{ VK_NULL_HANDLE };
    VkFormat m_swapchainFormat{ VK_FORMAT_UNDEFINED };
    VkColorSpaceKHR m_swapchainColorSpace{ VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    VkExtent2D m_swapchainExtent{};

    xr_vector<VkImage> m_swapchainImages;
    xr_vector<VkImageView> m_swapchainImageViews;
    xr_vector<VkFramebuffer> m_framebuffers;

    VkRenderPass m_defaultRenderPass{ VK_NULL_HANDLE };
    VkCommandPool m_commandPool{ VK_NULL_HANDLE };

    VkCommandBuffer m_commandBuffers[MAX_FRAMES_IN_FLIGHT]{};
    VkSemaphore m_imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT]{};
    VkSemaphore m_renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT]{};
    VkFence m_inFlightFences[MAX_FRAMES_IN_FLIGHT]{};

    uint32_t m_currentFrame{ 0 };
    uint32_t m_currentImageIndex{ 0 };
    bool m_frameInProgress{ false };
    bool m_renderPassActive{ false };

    string256 AdapterName{};

    void BeginPixEvent(pcstr /*name*/) {}
    void EndPixEvent() {}
};

extern CHW HW;

} // namespace xray::render::RENDER_NAMESPACE

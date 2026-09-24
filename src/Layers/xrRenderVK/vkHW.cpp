#include "stdafx.h"
#pragma hdrstop

#include "vkHW.h"
#include "xrEngine/XR_IOConsole.h"
#include "xrEngine/device.h"

#if defined(__ANDROID__)
#include <android/log.h>
#define VK_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "OpenXRayVK", __VA_ARGS__)
#define VK_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "OpenXRayVK", __VA_ARGS__)
#else
#define VK_LOGI(...) Msg(__VA_ARGS__)
#define VK_LOGE(...) Msg("! " __VA_ARGS__)
#endif

namespace xray::render::RENDER_NAMESPACE
{
CHW HW;

bool CHW::ThisInstanceIsGlobal() const
{
    return this == &HW;
}

CHW::CHW()
{
    if (!ThisInstanceIsGlobal())
        return;

    Device.seqAppActivate.Add(this);
    Device.seqAppDeactivate.Add(this);
}

CHW::~CHW()
{
    if (!ThisInstanceIsGlobal())
        return;

    Device.seqAppActivate.Remove(this);
    Device.seqAppDeactivate.Remove(this);

    DestroyDevice();
}

void CHW::OnAppActivate()
{
    if (m_window)
    {
        SDL_RestoreWindow(m_window);
    }
}

void CHW::OnAppDeactivate()
{
    if (m_window)
    {
        if (psDeviceMode.WindowStyle == rsFullscreen || psDeviceMode.WindowStyle == rsFullscreenBorderless)
            SDL_MinimizeWindow(m_window);
    }
}

void CHW::BeginPixEvent(pcstr /*name*/) const
{
}

void CHW::EndPixEvent() const
{
}

IRender::RenderContext CHW::GetCurrentContext() const
{
    return IRender::PrimaryContext;
}

int CHW::MakeContextCurrent(IRender::RenderContext /*context*/) const
{
    return 0;
}

std::pair<u32, u32> CHW::GetSurfaceSize()
{
    return { Device.dwWidth, Device.dwHeight };
}

DeviceState CHW::GetDeviceState() const
{
    return DeviceState::Normal;
}

void CHW::SetPrimaryAttributes(u32& windowFlags)
{
    windowFlags |= SDL_WINDOW_VULKAN;

    if (SDL_Vulkan_LoadLibrary(nullptr) != 0)
    {
        VK_LOGE("Failed to load Vulkan library via SDL: %s", SDL_GetError());
    }
    else
    {
        VK_LOGI("Vulkan loader initialized successfully via SDL_Vulkan_LoadLibrary");
    }
}

void CHW::CreateDevice(SDL_Window* sdlWnd)
{
    ZoneScoped;
    m_window = sdlWnd;
    R_ASSERT(m_window);

    VK_LOGI("Starting Vulkan Device & Swapchain initialization...");

    CreateInstance();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapchain();
    CreateImageViews();
    CreateDefaultRenderPass();
    CreateFramebuffers();
    CreateCommandPoolAndBuffers();
    CreateSyncObjects();

    Caps.fTarget = D3DFMT_A8R8G8B8;
    Caps.fDepth = D3DFMT_D24S8;

    Msg("* GPU vendor: [%s] device: [%s]", AdapterName, AdapterName);
    Msg("* Vulkan API Version: %d.%d.%d",
        VK_VERSION_MAJOR(m_deviceProperties.apiVersion),
        VK_VERSION_MINOR(m_deviceProperties.apiVersion),
        VK_VERSION_PATCH(m_deviceProperties.apiVersion));
    Msg("* Vulkan Driver Version: %d", m_deviceProperties.driverVersion);
    VK_LOGI("Vulkan initialization complete. Primary swapchain active (%ux%u).",
        m_swapchainExtent.width, m_swapchainExtent.height);
}

void CHW::CreateInstance()
{
    ZoneScoped;

    unsigned int extensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(m_window, &extensionCount, nullptr))
    {
        R_ASSERT2(false, SDL_GetError());
    }

    xr_vector<const char*> extensions(extensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(m_window, &extensionCount, extensions.data()))
    {
        R_ASSERT2(false, SDL_GetError());
    }

    VK_LOGI("Required Vulkan Instance Extensions count: %u", extensionCount);
    for (const auto* ext : extensions)
    {
        VK_LOGI("  Instance Extension: %s", ext);
    }

    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "OpenXRay";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 6, 2);
    appInfo.pEngineName = "X-Ray Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 6, 2);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;

    CHK_VK(vkCreateInstance(&createInfo, nullptr, &m_instance));
    VK_LOGI("Vulkan VkInstance created successfully (Vulkan 1.1)");
}

void CHW::CreateSurface()
{
    ZoneScoped;
    R_ASSERT(m_instance && m_window);

    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface))
    {
        R_ASSERT2(false, SDL_GetError());
    }
    VK_LOGI("Vulkan VkSurfaceKHR created successfully");
}

void CHW::PickPhysicalDevice()
{
    ZoneScoped;
    R_ASSERT(m_instance && m_surface);

    uint32_t deviceCount = 0;
    CHK_VK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr));
    R_ASSERT2(deviceCount > 0, "No Vulkan physical devices found on system!");

    xr_vector<VkPhysicalDevice> devices(deviceCount);
    CHK_VK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data()));

    m_physicalDevice = VK_NULL_HANDLE;

    for (const auto& dev : devices)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        xr_vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

        uint32_t graphicsIdx = UINT32_MAX;
        uint32_t presentIdx = UINT32_MAX;

        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                graphicsIdx = i;

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_surface, &presentSupport);
            if (presentSupport)
                presentIdx = i;

            if (graphicsIdx != UINT32_MAX && presentIdx != UINT32_MAX)
                break;
        }

        if (graphicsIdx != UINT32_MAX && presentIdx != UINT32_MAX)
        {
            m_physicalDevice = dev;
            m_deviceProperties = props;
            m_graphicsQueueFamily = graphicsIdx;
            m_presentQueueFamily = presentIdx;
            xr_strcpy(AdapterName, sizeof(AdapterName), props.deviceName);
            VK_LOGI("Selected Vulkan GPU: %s (Type: %d)", props.deviceName, props.deviceType);
            break;
        }
    }

    R_ASSERT2(m_physicalDevice != VK_NULL_HANDLE, "Failed to find a suitable Vulkan physical device with graphics & present support!");
}

void CHW::CreateLogicalDevice()
{
    ZoneScoped;
    R_ASSERT(m_physicalDevice);

    float queuePriority = 1.0f;
    xr_vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    xr_set<uint32_t> uniqueQueueFamilies = { m_graphicsQueueFamily, m_presentQueueFamily };

    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;
    createInfo.enabledLayerCount = 0;

    CHK_VK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);
    VK_LOGI("Vulkan VkDevice and Queues created successfully");
}

void CHW::CreateSwapchain()
{
    ZoneScoped;
    R_ASSERT(m_device && m_surface);

    VkSurfaceCapabilitiesKHR capabilities;
    CHK_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities));

    uint32_t formatCount = 0;
    CHK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr));
    R_ASSERT(formatCount > 0);
    xr_vector<VkSurfaceFormatKHR> formats(formatCount);
    CHK_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data()));

    m_swapchainFormat = formats[0].format;
    m_swapchainColorSpace = formats[0].colorSpace;
    for (const auto& fmt : formats)
    {
        if ((fmt.format == VK_FORMAT_B8G8R8A8_UNORM || fmt.format == VK_FORMAT_R8G8B8A8_UNORM) &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            m_swapchainFormat = fmt.format;
            m_swapchainColorSpace = fmt.colorSpace;
            break;
        }
    }

    uint32_t presentModeCount = 0;
    CHK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr));
    xr_vector<VkPresentModeKHR> presentModes(presentModeCount);
    CHK_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data()));

    VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR; // Guaranteed by Vulkan spec
    for (const auto& pm : presentModes)
    {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            selectedPresentMode = pm;
            break;
        }
    }

    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        m_swapchainExtent = capabilities.currentExtent;
    }
    else
    {
        int w = 0, h = 0;
        SDL_Vulkan_GetDrawableSize(m_window, &w, &h);
        m_swapchainExtent.width = std::clamp(static_cast<uint32_t>(w),
            capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        m_swapchainExtent.height = std::clamp(static_cast<uint32_t>(h),
            capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
    {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = m_swapchainFormat;
    createInfo.imageColorSpace = m_swapchainColorSpace;
    createInfo.imageExtent = m_swapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t queueFamilyIndices[] = { m_graphicsQueueFamily, m_presentQueueFamily };
    if (m_graphicsQueueFamily != m_presentQueueFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = selectedPresentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    CHK_VK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain));

    uint32_t actualImageCount = 0;
    CHK_VK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualImageCount, nullptr));
    m_swapchainImages.resize(actualImageCount);
    CHK_VK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualImageCount, m_swapchainImages.data()));

    BackBufferCount = actualImageCount;
    VK_LOGI("Vulkan Swapchain created: %ux%u (format: %d, present mode: %d, image count: %u)",
        m_swapchainExtent.width, m_swapchainExtent.height, m_swapchainFormat, selectedPresentMode, actualImageCount);
}

void CHW::CreateImageViews()
{
    ZoneScoped;
    m_swapchainImageViews.resize(m_swapchainImages.size());

    for (size_t i = 0; i < m_swapchainImages.size(); ++i)
    {
        VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        createInfo.image = m_swapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_swapchainFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        CHK_VK(vkCreateImageView(m_device, &createInfo, nullptr, &m_swapchainImageViews[i]));
    }
}

void CHW::CreateDefaultRenderPass()
{
    ZoneScoped;

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    CHK_VK(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_defaultRenderPass));
}

void CHW::CreateFramebuffers()
{
    ZoneScoped;
    m_framebuffers.resize(m_swapchainImageViews.size());

    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i)
    {
        VkImageView attachments[] = { m_swapchainImageViews[i] };

        VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        framebufferInfo.renderPass = m_defaultRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_swapchainExtent.width;
        framebufferInfo.height = m_swapchainExtent.height;
        framebufferInfo.layers = 1;

        CHK_VK(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffers[i]));
    }
}

void CHW::CreateCommandPoolAndBuffers()
{
    ZoneScoped;

    VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_graphicsQueueFamily;

    CHK_VK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool));

    VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    CHK_VK(vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers));
}

void CHW::CreateSyncObjects()
{
    ZoneScoped;

    VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        CHK_VK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]));
        CHK_VK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]));
        CHK_VK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]));
    }
}

void CHW::CleanupSwapchain()
{
    if (!m_device)
        return;

    vkDeviceWaitIdle(m_device);

    for (auto fb : m_framebuffers)
        vkDestroyFramebuffer(m_device, fb, nullptr);
    m_framebuffers.clear();

    if (m_defaultRenderPass)
    {
        vkDestroyRenderPass(m_device, m_defaultRenderPass, nullptr);
        m_defaultRenderPass = VK_NULL_HANDLE;
    }

    for (auto iv : m_swapchainImageViews)
        vkDestroyImageView(m_device, iv, nullptr);
    m_swapchainImageViews.clear();

    if (m_swapchain)
    {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

void CHW::RecreateSwapchain()
{
    ZoneScoped;
    if (!m_device)
        return;

    CleanupSwapchain();

    CreateSwapchain();
    CreateImageViews();
    CreateDefaultRenderPass();
    CreateFramebuffers();
}

void CHW::DestroyDevice()
{
    if (!m_device)
        return;

    vkDeviceWaitIdle(m_device);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if (m_imageAvailableSemaphores[i])
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        if (m_renderFinishedSemaphores[i])
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        if (m_inFlightFences[i])
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }

    if (m_commandPool)
    {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    CleanupSwapchain();

    if (m_device)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_surface)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_instance)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    VK_LOGI("Vulkan device and instance destroyed successfully");
}

void CHW::Reset()
{
    RecreateSwapchain();
}

void CHW::BeginScene()
{
    ZoneScoped;
    if (!m_device || !m_swapchain)
        return;

    CHK_VK(vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX));

    VkResult acquireRes = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_currentImageIndex);

    if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain();
        return;
    }
    else if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR)
    {
        CHK_VK(acquireRes);
    }

    CHK_VK(vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]));

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    CHK_VK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CHK_VK(vkBeginCommandBuffer(cmd, &beginInfo));

    m_frameInProgress = true;
    m_renderPassActive = false;
}

void CHW::ClearColor(float r, float g, float b, float a)
{
    if (!m_frameInProgress)
        return;

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];

    VkClearValue clearColor = { {{ r, g, b, a }} };

    VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassInfo.renderPass = m_defaultRenderPass;
    renderPassInfo.framebuffer = m_framebuffers[m_currentImageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_swapchainExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchainExtent.width);
    viewport.height = static_cast<float>(m_swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_swapchainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_renderPassActive = true;
}

void CHW::EndScene()
{
    ZoneScoped;
    if (!m_frameInProgress)
        return;

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];

    if (!m_renderPassActive)
    {
        // Default clear to Zone slate gray if no pass was started
        ClearColor(0.08f, 0.09f, 0.10f, 1.0f);
    }

    if (m_renderPassActive)
    {
        vkCmdEndRenderPass(cmd);
        m_renderPassActive = false;
    }

    CHK_VK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    CHK_VK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]));

    m_frameInProgress = false;
}

void CHW::Present()
{
    ZoneScoped;
    if (!m_device || !m_swapchain)
        return;

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &m_currentImageIndex;

    VkResult presentRes = vkQueuePresentKHR(m_presentQueue, &presentInfo);

    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR)
    {
        RecreateSwapchain();
    }
    else if (presentRes != VK_SUCCESS)
    {
        CHK_VK(presentRes);
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    CurrentBackBuffer = m_currentFrame;
}

uint32_t CHW::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    R_ASSERT2(false, "Failed to find suitable Vulkan memory type!");
    return 0;
}

void CHW::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    if (!m_device || size == 0)
        return;

    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    CHK_VK(vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    CHK_VK(vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory));
    CHK_VK(vkBindBufferMemory(m_device, buffer, bufferMemory, 0));
}

} // namespace xray::render::RENDER_NAMESPACE

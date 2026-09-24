#pragma once

#if defined(XR_PLATFORM_WINDOWS)
#include <d3d9types.h>
#else
#include "Common/d3d9compat.hpp"
#endif

#include <vulkan/vulkan.h>
#include <SDL_vulkan.h>

namespace xray::render::RENDER_NAMESPACE
{
inline const char* vk_result_to_string(VkResult result)
{
    switch (result)
    {
    case VK_SUCCESS:                        return "VK_SUCCESS";
    case VK_NOT_READY:                      return "VK_NOT_READY";
    case VK_TIMEOUT:                        return "VK_TIMEOUT";
    case VK_EVENT_SET:                      return "VK_EVENT_SET";
    case VK_EVENT_RESET:                    return "VK_EVENT_RESET";
    case VK_INCOMPLETE:                     return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:       return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:     return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:    return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:              return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:    return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:      return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:      return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:         return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:     return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:          return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_SURFACE_LOST_KHR:         return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:                 return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:          return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:    return "VK_ERROR_VALIDATION_FAILED_EXT";
    default:                                return "VK_UNKNOWN_ERROR";
    }
}

#define CHK_VK(expr) \
    do { \
        const VkResult _res = (expr); \
        R_ASSERT3(_res == VK_SUCCESS, "Vulkan error: " #expr, vk_result_to_string(_res)); \
    } while (0)

class vkState;

typedef enum D3D_CLEAR_FLAG {
    D3D_CLEAR_DEPTH = 0x1L,
    D3D_CLEAR_STENCIL = 0x2L
} D3D_CLEAR_FLAG;

typedef enum D3D_COMPARISON_FUNC {
    D3D_COMPARISON_NEVER = VK_COMPARE_OP_NEVER,
    D3D_COMPARISON_LESS = VK_COMPARE_OP_LESS,
    D3D_COMPARISON_EQUAL = VK_COMPARE_OP_EQUAL,
    D3D_COMPARISON_LESS_EQUAL = VK_COMPARE_OP_LESS_OR_EQUAL,
    D3D_COMPARISON_GREATER = VK_COMPARE_OP_GREATER,
    D3D_COMPARISON_NOT_EQUAL = VK_COMPARE_OP_NOT_EQUAL,
    D3D_COMPARISON_GREATER_EQUAL = VK_COMPARE_OP_GREATER_OR_EQUAL,
    D3D_COMPARISON_ALWAYS = VK_COMPARE_OP_ALWAYS
} D3D_COMPARISON_FUNC;

struct XR_VK_VIEWPORT
{
    float TopLeftX, TopLeftY;
    float Width, Height;
    float MinDepth, MaxDepth;
};

struct D3D_VIEWPORT : XR_VK_VIEWPORT
{
    using XR_VK_VIEWPORT::XR_VK_VIEWPORT;

    template <typename TopLeftCoords, typename Dimensions>
    D3D_VIEWPORT(TopLeftCoords x, TopLeftCoords y, Dimensions w, Dimensions h, float minZ, float maxZ)
        : XR_VK_VIEWPORT{
            static_cast<float>(x), static_cast<float>(y),
            static_cast<float>(w), static_cast<float>(h),
            minZ, maxZ
        }
    {}
};

using D3D_QUERY = enum XR_VK_QUERY
{
    D3D_QUERY_EVENT,
    D3D_QUERY_OCCLUSION
};

using ID3DState = vkState;

#define DX11_ONLY(expr) do {} while (0)

using unused_t = int[0];

using IndexBufferHandle = VkBuffer;
using VertexBufferHandle = VkBuffer;
using ConstantBufferHandle = VkBuffer;
using HostBufferHandle = void*;

using VertexElement = D3DVERTEXELEMENT9;
using InputElementDesc = unused_t;

} // namespace xray::render::RENDER_NAMESPACE

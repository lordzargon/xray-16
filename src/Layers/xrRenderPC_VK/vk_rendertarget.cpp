#include "stdafx.h"
#include "vk_rendertarget.h"

#include "Layers/xrRenderVK/vkHW.h"

namespace xray::render::RENDER_NAMESPACE
{
CRenderTarget::CRenderTarget()
{
    param_blur = 0.f;
    param_gray = 0.f;
    param_noise = 0.f;
    param_duality_h = 0.f;
    param_duality_v = 0.f;
    param_noise_fps = 25.f;
    param_noise_scale = 1.f;

    param_color_base = color_rgba(127, 127, 127, 0);
    param_color_gray = color_rgba(85, 85, 85, 0);
    param_color_add.set(0.0f, 0.0f, 0.0f);

    param_color_map_influence = 0.0f;
    param_color_map_interpolate = 0.0f;

    dwLightMarkerID = 5;

    for (int id = 0; id < R__NUM_CONTEXTS; ++id)
    {
        dwWidth[id] = Device.dwWidth;
        dwHeight[id] = Device.dwHeight;
    }

    // PP
    s_postprocess.create("postprocess");
    g_postprocess.create(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX3,
        RImplementation.Vertex.Buffer(), RImplementation.QuadIB);
    s_postprocess_msaa = s_postprocess;

    // Menu
    s_menu.create("distort");
    g_menu.create(FVF::F_TL, RImplementation.Vertex.Buffer(), RImplementation.QuadIB);
}

CRenderTarget::~CRenderTarget()
{
    g_menu.destroy();
    s_menu.destroy();
    g_postprocess.destroy();
    s_postprocess.destroy();
    s_postprocess_msaa.destroy();
}

VkImageView CRenderTarget::get_base_rt()
{
    if (HW.CurrentBackBuffer < HW.m_swapchainImageViews.size())
        return HW.m_swapchainImageViews[HW.CurrentBackBuffer];
    return VK_NULL_HANDLE;
}

VkImageView CRenderTarget::get_base_zb()
{
    return VK_NULL_HANDLE;
}

void CRenderTarget::build_textures()
{
}

void CRenderTarget::u_setrt(CBackend& cmd_list, const ref_rt& _1, const ref_rt& _2, const ref_rt& _3, const ref_rt& zb)
{
    cmd_list.set_pass_targets(_1, _2, _3, zb);
}

void CRenderTarget::u_setrt(CBackend& cmd_list, u32 W, u32 H, VkImageView _1, VkImageView _2, VkImageView _3, VkImageView zb)
{
    cmd_list.curr_rt_width = W;
    cmd_list.curr_rt_height = H;
    cmd_list.set_RT(_1, 0);
    cmd_list.set_RT(_2, 1);
    cmd_list.set_RT(_3, 2);
    cmd_list.set_ZB(zb);
}

void CRenderTarget::phase_scene_prepare() {}
void CRenderTarget::phase_scene_begin() {}
void CRenderTarget::phase_scene_end() {}
void CRenderTarget::phase_occq() {}
void CRenderTarget::phase_wallmarks() {}
void CRenderTarget::phase_smap_direct(CBackend& /*cmd_list*/, light* /*L*/, u32 /*sub_phase*/) {}
void CRenderTarget::phase_smap_direct_tsh(CBackend& /*cmd_list*/, light* /*L*/, u32 /*sub_phase*/) {}
void CRenderTarget::phase_smap_spot_clear(CBackend& /*cmd_list*/) {}
void CRenderTarget::phase_smap_spot(CBackend& /*cmd_list*/, light* /*L*/) {}
void CRenderTarget::phase_smap_spot_tsh(CBackend& /*cmd_list*/, light* /*L*/) {}
void CRenderTarget::phase_accumulator(CBackend& /*cmd_list*/) {}
void CRenderTarget::phase_vol_accumulator() {}

void CRenderTarget::create_minmax_SM(CBackend& /*cmd_list*/) {}
void CRenderTarget::phase_rain(CBackend& /*cmd_list*/) {}
void CRenderTarget::draw_rain(CBackend& /*cmd_list*/, light& /*RainSetup*/) {}
void CRenderTarget::mark_msaa_edges() {}

bool CRenderTarget::need_to_render_sunshafts() { return false; }
bool CRenderTarget::use_minmax_sm_this_frame() { return false; }

bool CRenderTarget::enable_scissor(light* /*L*/) { return false; }
void CRenderTarget::enable_dbt_bounds(light* /*L*/) {}
void CRenderTarget::disable_aniso() {}

void CRenderTarget::draw_volume(CBackend& /*cmd_list*/, light* /*L*/) {}
void CRenderTarget::accum_direct(CBackend& /*cmd_list*/, u32 /*sub_phase*/) {}
void CRenderTarget::accum_direct_cascade(CBackend& /*cmd_list*/, u32 /*sub_phase*/, Fmatrix& /*xform*/, Fmatrix& /*xform_prev*/, float /*fBias*/) {}
void CRenderTarget::accum_direct_f(CBackend& /*cmd_list*/, u32 /*sub_phase*/) {}
void CRenderTarget::accum_direct_lum(CBackend& /*cmd_list*/) {}
void CRenderTarget::accum_direct_blend(CBackend& /*cmd_list*/) {}
void CRenderTarget::accum_direct_volumetric(u32 /*sub_phase*/, const u32 /*Offset*/, const Fmatrix& /*mShadow*/) {}
void CRenderTarget::accum_point(CBackend& /*cmd_list*/, light* /*L*/) {}
void CRenderTarget::accum_spot(CBackend& /*cmd_list*/, light* /*L*/) {}
void CRenderTarget::accum_reflected(CBackend& /*cmd_list*/, light* /*L*/) {}
void CRenderTarget::accum_volumetric(CBackend& /*cmd_list*/, light* /*L*/) {}

void CRenderTarget::phase_bloom() {}
void CRenderTarget::phase_luminance() {}
void CRenderTarget::phase_combine() {}
void CRenderTarget::phase_combine_volumetric() {}
void CRenderTarget::phase_pp() {}

void CRenderTarget::reset_light_marker(CBackend& /*cmd_list*/, bool /*bResetStencil*/) {}
void CRenderTarget::increment_light_marker(CBackend& /*cmd_list*/) {}

} // namespace xray::render::RENDER_NAMESPACE

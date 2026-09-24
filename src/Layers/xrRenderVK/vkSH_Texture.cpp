#include "stdafx.h"
#pragma hdrstop

#include "Layers/xrRender/ResourceManager.h"
#include "Layers/xrRender/SH_Texture.h"
#include "r2.h"
#include "vkHW.h"

#ifdef XR_PLATFORM_WINDOWS
#include "xrEngine/tntQAVI.h"
#endif
#include "xrEngine/xrTheora_Surface.h"

namespace xray::render::RENDER_NAMESPACE
{
void fix_texture_name(pstr fn);

void resptrcode_texture::create(LPCSTR _name)
{
    _set(RImplementation.Resources->_CreateTexture(_name));
}

//--------------------------------------------------------------------------------------------------
CTexture::CTexture()
{
    pSurface = VK_NULL_HANDLE;
    m_view = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    pAVI = nullptr;
    pTheora = nullptr;
    seqMSPF = 0;
    flags.MemoryUsage = 0;
    flags.bLoaded = false;
    flags.bUser = false;
    flags.seqCycles = FALSE;
    m_material = 1.0f;
    bind = fastdelegate::FastDelegate2<CBackend&, u32>(this, &CTexture::apply_load);
}

CTexture::~CTexture()
{
    Unload();
    RImplementation.Resources->_DeleteTexture(this);
}

void CTexture::surface_set(VkImage surf, VkImageView view)
{
    pSurface = surf;
    m_view = view;
}

VkImage CTexture::surface_get() const
{
    return pSurface;
}

void CTexture::PostLoad()
{
    if (pTheora) bind = fastdelegate::FastDelegate2<CBackend&, u32>(this, &CTexture::apply_theora);
    else if (pAVI) bind = fastdelegate::FastDelegate2<CBackend&, u32>(this, &CTexture::apply_avi);
    else if (!seqDATA.empty()) bind = fastdelegate::FastDelegate2<CBackend&, u32>(this, &CTexture::apply_seq);
    else bind = fastdelegate::FastDelegate2<CBackend&, u32>(this, &CTexture::apply_normal);
}

void CTexture::apply_load(CBackend& cmd_list, u32 dwStage)
{
    if (!flags.bLoaded) Load();
    PostLoad();
    bind(cmd_list, dwStage);
}

void CTexture::apply_theora(CBackend& cmd_list, u32 dwStage)
{
}

void CTexture::apply_avi(CBackend& cmd_list, u32 dwStage) const
{
}

void CTexture::apply_seq(CBackend& cmd_list, u32 dwStage)
{
    if (seqDATA.empty()) return;

    u32 frame = Device.dwTimeContinual / (seqMSPF ? seqMSPF : 33);
    u32 frame_data = seqDATA.size();
    u32 frame_id = flags.seqCycles ? (frame % (frame_data * 2)) : (frame % frame_data);
    if (flags.seqCycles && frame_id >= frame_data)
        frame_id = frame_data - 1 - frame_id % frame_data;

    pSurface = seqDATA[frame_id];
}

void CTexture::apply_normal(CBackend& cmd_list, u32 dwStage) const
{
}

void CTexture::Preload()
{
    m_bumpmap = RImplementation.Resources->m_textures_description.GetBumpName(cName);
    m_material = RImplementation.Resources->m_textures_description.GetMaterial(cName);
}

void CTexture::Load()
{
    flags.bLoaded = true;
    if (pSurface) return;

    flags.bUser = false;
    flags.MemoryUsage = 0;
    if (nullptr == cName.c_str()) return;
    if (0 == xr_stricmp(cName.c_str(), "$null")) return;
    if (0 == strncmp(cName.c_str(), "$user$", sizeof("$user$") - 1))
    {
        flags.bUser = true;
        return;
    }

    Preload();

    // Normal texture
    u32 mem = 0;
    pSurface = RImplementation.texture_load(cName.c_str(), mem, m_view, m_memory);
    if (pSurface)
        flags.MemoryUsage = mem;

    PostLoad();
}

void CTexture::Unload()
{
    flags.bLoaded = false;
    seqDATA.clear();
    seqViews.clear();

    if (m_view != VK_NULL_HANDLE && HW.m_device != VK_NULL_HANDLE)
    {
        vkDestroyImageView(HW.m_device, m_view, nullptr);
        m_view = VK_NULL_HANDLE;
    }
    if (pSurface != VK_NULL_HANDLE && HW.m_device != VK_NULL_HANDLE)
    {
        vkDestroyImage(HW.m_device, pSurface, nullptr);
        pSurface = VK_NULL_HANDLE;
    }
    if (m_memory != VK_NULL_HANDLE && HW.m_device != VK_NULL_HANDLE)
    {
        vkFreeMemory(HW.m_device, m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }

#ifdef XR_PLATFORM_WINDOWS
    xr_delete(pAVI);
#endif
    xr_delete(pTheora);

    bind = fastdelegate::FastDelegate2<CBackend&, u32>(this, &CTexture::apply_load);
}

void CTexture::desc_update()
{
    desc_cache = pSurface;
    if (pTheora)
    {
        m_width = pTheora->Width(true);
        m_height = pTheora->Height(true);
        return;
    }
    if (m_width == 0) m_width = 1024;
    if (m_height == 0) m_height = 1024;
}

void CTexture::video_Play(BOOL looped, u32 _time)
{
    if (pTheora) pTheora->Play(looped, _time != 0xFFFFFFFF ? (m_play_time = _time) : Device.dwTimeContinual);
}

void CTexture::video_Pause(BOOL state) const
{
    if (pTheora) pTheora->Pause(state);
}

void CTexture::video_Stop() const
{
    if (pTheora) pTheora->Stop();
}

BOOL CTexture::video_IsPlaying() const
{
    return pTheora ? pTheora->IsPlaying() : FALSE;
}

} // namespace xray::render::RENDER_NAMESPACE

#include "stdafx.h"
#pragma hdrstop

#include "Layers/xrRender/ResourceManager.h"
#include "Layers/xrRender/SH_RT.h"
#include "vkHW.h"

namespace xray::render::RENDER_NAMESPACE
{
CRT::~CRT()
{
    destroy();
    RImplementation.Resources->_DeleteRT(this);
}

void CRT::set_slice_read(int slice) {}
void CRT::set_slice_write(u32 context_id, int slice) {}

void CRT::create(LPCSTR Name, u32 w, u32 h, D3DFORMAT f, u32 SampleCount /*= 1*/, u32 slices_num /*= 1*/, Flags32 /*flags = {}*/)
{
    if (pRT != VK_NULL_HANDLE || pSurface != VK_NULL_HANDLE)
        return;

    R_ASSERT(Name && Name[0] && w && h);
    _order = CPU::QPC();

    dwWidth = w;
    dwHeight = h;
    fmt = f;
    sampleCount = SampleCount;
    n_slices = slices_num;

    RImplementation.Resources->Evict();

    pTexture = RImplementation.Resources->_CreateTexture(Name);
    if (pTexture)
        pTexture->surface_set(VK_NULL_HANDLE);
}

void CRT::destroy()
{
    if (pTexture._get())
    {
        pTexture->surface_set(VK_NULL_HANDLE);
        pTexture = nullptr;
    }
    if (pRT != VK_NULL_HANDLE)
    {
        vkDestroyImageView(HW.m_device, pRT, nullptr);
        pRT = VK_NULL_HANDLE;
    }
    if (pZRT != VK_NULL_HANDLE && pZRT != pRT)
    {
        vkDestroyImageView(HW.m_device, pZRT, nullptr);
        pZRT = VK_NULL_HANDLE;
    }
    if (pSurface != VK_NULL_HANDLE)
    {
        vkDestroyImage(HW.m_device, pSurface, nullptr);
        pSurface = VK_NULL_HANDLE;
    }
    if (pMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(HW.m_device, pMemory, nullptr);
        pMemory = VK_NULL_HANDLE;
    }
}

void CRT::reset_begin()
{
    destroy();
}

void CRT::reset_end()
{
    create(cName.c_str(), dwWidth, dwHeight, fmt, sampleCount, n_slices, { dwFlags });
}

void CRT::resolve_into(CRT& /*destination*/) const
{
}

void resptrcode_crt::create(LPCSTR Name, u32 w, u32 h, D3DFORMAT f, u32 SampleCount /*= 1*/, u32 slices_num /*= 1*/, Flags32 flags /*= {}*/)
{
    _set(RImplementation.Resources->_CreateRT(Name, w, h, f, SampleCount, slices_num, flags));
}

} // namespace xray::render::RENDER_NAMESPACE

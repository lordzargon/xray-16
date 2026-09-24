#pragma once

#include "CommonTypes.h"
#include "vkHW.h"

namespace xray::render::RENDER_NAMESPACE
{
IC void CBackend::set_xform(u32 ID, const Fmatrix& M)
{
    stat.xforms++;
}

IC VkFramebuffer CBackend::get_FB()
{
    return pFB;
}

IC void CBackend::set_FB(VkFramebuffer FB)
{
    pFB = FB;
}

IC void CBackend::set_RT(VkImageView RT, u32 ID)
{
    if (ID < 4)
        pRT[ID] = RT;
}

IC void CBackend::set_ZB(VkImageView ZB)
{
    pZB = ZB;
}

IC void CBackend::ClearRT(VkImageView rt, const Fcolor& color)
{
    HW.ClearColor(color.r, color.g, color.b, color.a);
}

IC void CBackend::ClearZB(VkImageView zb, float depth)
{
}

IC void CBackend::ClearZB(VkImageView zb, float depth, u8 stencil)
{
}

IC bool CBackend::ClearRTRect(VkImageView rt, const Fcolor& color, size_t numRects, const Irect* rects)
{
    return true;
}

IC bool CBackend::ClearZBRect(VkImageView zb, float depth, size_t numRects, const Irect* rects)
{
    return true;
}

ICF void CBackend::set_Format(SDeclaration* _decl)
{
    decl = _decl;
}

ICF void CBackend::set_PS(VkShaderModule _ps, LPCSTR _n)
{
    ps = _ps;
}

ICF void CBackend::set_GS(VkShaderModule _gs, LPCSTR _n)
{
    gs = _gs;
}

ICF void CBackend::set_VS(VkShaderModule _vs, LPCSTR _n)
{
    vs = _vs;
}

ICF void CBackend::set_VS(ref_vs& _vs)
{
    set_VS(_vs->sh, _vs->cName.c_str());
}

ICF void CBackend::set_Vertices(VertexBufferHandle _vb, u32 _vb_stride)
{
    vb = _vb;
    vb_stride = _vb_stride;
}

ICF void CBackend::set_Indices(IndexBufferHandle _ib)
{
    ib = _ib;
}

IC void CBackend::set_Geometry(SGeometry* _geom)
{
    set_Format(&*_geom->dcl);
    set_Vertices(_geom->vb, _geom->vb_stride);
    set_Indices(_geom->ib);
}

ICF void CBackend::Render(D3DPRIMITIVETYPE T, u32 baseV, u32 startV, u32 countV, u32 startI, u32 PC)
{
}

ICF void CBackend::Render(D3DPRIMITIVETYPE T, u32 startV, u32 PC)
{
}

IC void CBackend::SetViewport(const D3D_VIEWPORT& viewport) const
{
}

IC void CBackend::set_Scissor(const Irect* rect)
{
}

IC void CBackend::set_Stencil(u32 _enable, u32 _func, u32 _ref, u32 _mask, u32 _writemask, u32 _fail, u32 _pass, u32 _zfail)
{
}

IC void CBackend::set_Z(u32 _enable)
{
    z_enable = _enable;
}

IC void CBackend::set_ZFunc(u32 _func)
{
    z_func = _func;
}

IC void CBackend::set_AlphaRef(u32 _value)
{
}

IC void CBackend::set_ColorWriteEnable(u32 _mask)
{
    colorwrite_mask = _mask;
}

ICF void CBackend::set_CullMode(u32 _mode)
{
    cull_mode = _mode;
}

ICF void CBackend::set_FillMode(u32 _mode)
{
    fill_mode = _mode;
}

ICF void CBackend::SetTextureFactor(u32 /*factor*/) const
{
}

ICF void CBackend::SetAmbient(u32 /*factor*/) const
{
}

IC void CBackend::set_Constants(R_constant_table* C)
{
    if (ctable == C) return;
    ctable = C;
    xforms.unmap();
    hemi.unmap();
    tree.unmap();
    if (nullptr == C) return;

    for (auto& Cs : C->table)
        if (Cs->handler) Cs->handler->setup(*this, &*Cs);
}

void CBackend::set_pass_targets(const ref_rt& _1, const ref_rt& _2, const ref_rt& _3, const ref_rt& zb)
{
    if (_1)
    {
        curr_rt_width  = _1->dwWidth;
        curr_rt_height = _1->dwHeight;
    }
    else if (zb)
    {
        curr_rt_width  = zb->dwWidth;
        curr_rt_height = zb->dwHeight;
    }
}

} // namespace xray::render::RENDER_NAMESPACE

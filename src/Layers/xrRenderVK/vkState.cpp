#include "stdafx.h"
#pragma hdrstop

#include "vkState.h"

namespace xray::render::RENDER_NAMESPACE
{
vkState::vkState()
{
    memset(m_samplerArray, 0, sizeof(m_samplerArray));

    rasterizerCullMode = D3DCULL_CCW;

    m_pDepthStencilState.DepthEnable = TRUE;
    m_pDepthStencilState.DepthFunc = D3DCMP_LESSEQUAL;
    m_pDepthStencilState.DepthWriteMask = TRUE;
    m_pDepthStencilState.StencilEnable = TRUE;
    m_pDepthStencilState.StencilFailOp = D3DSTENCILOP_KEEP;
    m_pDepthStencilState.StencilDepthFailOp = D3DSTENCILOP_KEEP;
    m_pDepthStencilState.StencilPassOp = D3DSTENCILOP_KEEP;
    m_pDepthStencilState.StencilFunc = D3DCMP_ALWAYS;
    m_pDepthStencilState.StencilMask = 0xFFFFFFFF;
    m_pDepthStencilState.StencilWriteMask = 0xFFFFFFFF;
    m_pDepthStencilState.StencilRef = 0;

    m_pBlendState.BlendEnable = TRUE;
    m_pBlendState.SrcBlend = D3DBLEND_ONE;
    m_pBlendState.DestBlend = D3DBLEND_ZERO;
    m_pBlendState.SrcBlendAlpha = D3DBLEND_ONE;
    m_pBlendState.DestBlendAlpha = D3DBLEND_ZERO;
    m_pBlendState.BlendOp = D3DBLENDOP_ADD;
    m_pBlendState.BlendOpAlpha = D3DBLENDOP_ADD;
    m_pBlendState.ColorMask = 0xF;

    m_uiMipLODBias = FLT_MAX;
}

vkState* vkState::Create()
{
    return xr_new<vkState>();
}

void vkState::Apply()
{
}

void vkState::Release()
{
}

void vkState::UpdateRenderState(u32 /*name*/, u32 /*value*/)
{
}

void vkState::UpdateSamplerState(u32 /*stage*/, u32 /*name*/, u32 /*value*/)
{
}

} // namespace xray::render::RENDER_NAMESPACE

#include "stdafx.h"
#pragma hdrstop

#include "TouchOverlay.h"
#include "xr_input.h"
#include "device.h"
#include "IGame_Level.h"

#include <imgui.h>

// ---------------------------------------------------------------------------
// CTouchOverlay - on-screen virtual gamepad HUD
// ---------------------------------------------------------------------------
// Rendering: uses ImGui background draw list within seqFrame so it draws
// during the active ImGui frame on all platforms including Android GLES/Vulkan.
// All coordinates are mapped from Device.dwWidth / Device.dwHeight.
// ---------------------------------------------------------------------------

CTouchOverlay* pTouchOverlay = nullptr;

CTouchOverlay::CTouchOverlay()
{
    Device.seqFrame.Add(this, REG_PRIORITY_LOW - 1000);
}

CTouchOverlay::~CTouchOverlay()
{
    Device.seqFrame.Remove(this);
}

static ImU32 ToImColor(u8 r, u8 g, u8 b, float alpha)
{
    const u8 a = static_cast<u8>(clampr(alpha, 0.f, 1.f) * 255.f);
    return IM_COL32(r, g, b, a);
}

void CTouchOverlay::OnFrame()
{
    if (!Device.b_is_Ready || !pInput)
        return;
    if (!psTouchEnable)
        return;

    // Only render the touch overlay HUD during active 3D gameplay
    if (!g_pGameLevel || !g_pGameLevel->bReady)
        return;

    // Auto-hide when a physical controller is actively driving input
    if (pInput->IsCurrentInputTypeController())
    {
        m_controllerIdleTime += Device.fTimeDeltaReal;
        if (m_controllerIdleTime > 0.5f)
            return;
    }
    else
    {
        m_controllerIdleTime = 0.f;
    }

    if (!ImGui::GetCurrentContext())
        return;

    RenderStick();
    RenderButtons();
}

void CTouchOverlay::RenderStick()
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl)
        return;

    const float H      = static_cast<float>(Device.dwHeight);
    const float alpha  = psTouchOpacity;
    const float outerR = H * 0.17f;
    const float innerR = outerR * 0.42f;
    const float cx     = H * 0.35f;
    const float cy     = H * 0.72f;

    const CInput::CTouchState& ts = pInput->iGetTouchState();

    // Outer base circle
    dl->AddCircleFilled(ImVec2(cx, cy), outerR, ToImColor(20, 20, 20, alpha * 0.25f), 32);
    dl->AddCircle(ImVec2(cx, cy), outerR, ToImColor(255, 255, 255, alpha * (ts.stickFingerActive ? 0.65f : 0.35f)), 32, 2.5f);

    // Inner knob - tracks finger deflection
    float kx = cx, ky = cy;
    if (ts.stickFingerActive)
    {
        const float dx  = ts.stickCurrent.x - ts.stickOrigin.x;
        const float dy  = ts.stickCurrent.y - ts.stickOrigin.y;
        const float mag = _sqrt(dx * dx + dy * dy);
        if (mag > 0.f)
        {
            const float clamped = _min(mag, outerR);
            kx = cx + (dx / mag) * clamped;
            ky = cy + (dy / mag) * clamped;
        }
    }

    const float knobAlpha = ts.stickFingerActive ? alpha * 0.85f : alpha * 0.50f;
    dl->AddCircleFilled(ImVec2(kx, ky), innerR, ToImColor(220, 220, 220, knobAlpha), 24);
    dl->AddCircle(ImVec2(kx, ky), innerR, ToImColor(255, 255, 255, knobAlpha * 1.1f), 24, 2.0f);
}

void CTouchOverlay::RenderButtons()
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl)
        return;

    const float W     = static_cast<float>(Device.dwWidth);
    const float H     = static_cast<float>(Device.dwHeight);
    const float alpha = psTouchOpacity;

    struct BtnDef { int idx; pcstr label; u8 r, g, b; };
    static const BtnDef defs[] =
    {
        {CInput::TB_FIRE,   "FIRE", 230,  60,  60},
        {CInput::TB_AIM,    "AIM",  255, 180,   0},
        {CInput::TB_JUMP,   "JUMP",  60, 200, 100},
        {CInput::TB_CROUCH, "CRCH", 100, 180, 230},
        {CInput::TB_USE,    "USE",  200, 200,  80},
        {CInput::TB_RELOAD, "RLD",  200, 120, 220},
        {CInput::TB_INV,    "INV",  160, 160, 180},
        {CInput::TB_PDA,    "PDA",  160, 160, 180},
        {CInput::TB_MENU,   "MENU", 160, 160, 180},
        {CInput::TB_QUICK1, "1",    180, 180, 180},
        {CInput::TB_QUICK2, "2",    180, 180, 180},
        {CInput::TB_QUICK3, "3",    180, 180, 180},
        {CInput::TB_QUICK4, "4",    180, 180, 180},
    };

    const CInput::CTouchState& ts = pInput->iGetTouchState();
    ImFont* font = ImGui::GetFont();

    for (const auto& d : defs)
    {
        const SDL_FRect& nr = ts.buttonRects[d.idx];
        const float bcx = (nr.x + nr.w * 0.5f) * W;
        const float bcy = (nr.y + nr.h * 0.5f) * H;
        const float br  = nr.w * 0.5f * W;
        const bool  dn  = ts.buttonDown[d.idx];
        const float ba  = dn ? alpha * 0.85f : alpha * 0.40f;

        // Button background circle
        dl->AddCircleFilled(ImVec2(bcx, bcy), br, ToImColor(d.r, d.g, d.b, ba), 28);

        // Button outline ring
        dl->AddCircle(ImVec2(bcx, bcy), br, ToImColor(255, 255, 255, dn ? 0.95f : alpha * 0.60f), 28, dn ? 2.5f : 1.5f);

        // Scaled text label via ImGui font
        const float fontSize = br * 0.52f;
        const ImVec2 textSize = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, d.label) : ImGui::CalcTextSize(d.label);
        const ImVec2 textPos(bcx - textSize.x * 0.5f, bcy - textSize.y * 0.5f);

        if (font)
            dl->AddText(font, fontSize, textPos, ToImColor(255, 255, 255, dn ? 1.0f : alpha * 0.95f), d.label);
        else
            dl->AddText(textPos, ToImColor(255, 255, 255, dn ? 1.0f : alpha * 0.95f), d.label);
    }
}

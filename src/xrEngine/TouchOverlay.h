#pragma once

#include "pure.h"

// ---------------------------------------------------------------------------
// CTouchOverlay
//
// Renders the on-screen virtual gamepad HUD for Android / touch-enabled builds.
// Draws:
//   - Virtual analogue stick (outer ring + inner knob) anchored in the lower-left
//   - Action buttons (circles) on the right side of the screen
//   - Text labels on each button
//
// Registers itself to Device.seqFrame so it renders via Dear ImGui during
// active 3D gameplay.
//
// Visibility rules:
//   - psTouchEnable == 0              : always hidden
//   - Main menu / dialog active       : hidden (clean UI navigation)
//   - Bluetooth controller active     : hidden (fades out after inactivity threshold)
//   - Otherwise                       : shown at psTouchOpacity
// ---------------------------------------------------------------------------

class ENGINE_API CTouchOverlay final : public pureFrame
{
public:
    CTouchOverlay();
    ~CTouchOverlay();

    // pureFrame
    virtual void OnFrame() override;

private:
    float m_controllerIdleTime{ 0.f };

    void RenderStick();
    void RenderButtons();
};

extern ENGINE_API CTouchOverlay* pTouchOverlay;

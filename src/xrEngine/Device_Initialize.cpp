#include "stdafx.h"
#include "embedded_resources_management.h"

#include "xr_input.h"
#include "GameFont.h"
#include "PerformanceAlert.hpp"
#include "xrCore/ModuleLookup.hpp"

#include <SDL.h>
#ifdef IMGUI_ENABLE_VIEWPORTS
#   include <SDL_syswm.h>
#endif

SDL_HitTestResult WindowHitTest(SDL_Window* win, const SDL_Point* area, void* data);

namespace
{
// This is put in a separate function due to bunch of defines.
// Keeping that in CRenderDevice::Initialize would harm the readability.
void SetSDLSettings(pcstr title)
{
#ifdef  SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
#endif
#ifdef  SDL_HINT_AUDIO_DEVICE_APP_NAME
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_APP_NAME, title);
#endif
#ifdef  SDL_HINT_APP_NAME
    SDL_SetHint(SDL_HINT_APP_NAME, title);
#endif
#ifdef  SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif
#ifdef  SDL_HINT_MOUSE_AUTO_CAPTURE
    SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
#endif
}
} // namespace

void CRenderDevice::Initialize()
{
    ZoneScoped;
    Log("Initializing Engine...");
    TimerGlobal.Start();
    TimerMM.Start();

    Msg("[OpenXRay] Device::Initialize: stage 1 start");
    {
#if defined(__ANDROID__)
        Uint32 flags = SDL_WINDOW_FULLSCREEN;
#else
        Uint32 flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
#endif

        GEnv.Render->ObtainRequiredWindowFlags(flags);
        Msg("[OpenXRay] Device::Initialize: stage 2 flags=0x%08x", flags);

        int icon = IDI_ICON_COP;
        pcstr title = "S.T.A.L.K.E.R.: Call of Pripyat";

        if (ShadowOfChernobylMode)
        {
            icon = IDI_ICON_SOC;
            title = "S.T.A.L.K.E.R.: Shadow of Chernobyl";
        }
        else if (ClearSkyMode)
        {
            icon = IDI_ICON_CS;
            title = "S.T.A.L.K.E.R.: Clear Sky";
        }

        xr_strcpy(Core.ApplicationTitle, title);
        SetSDLSettings(title);

        Msg("[OpenXRay] Device::Initialize: stage 3 calling SDL_CreateWindow");
#if defined(__ANDROID__)
        m_sdlWnd = SDL_CreateWindow(title, 0, 0, 0, 0, flags);
#else
        m_sdlWnd = SDL_CreateWindow(title, 0, 0, 640, 480, flags);
#endif
        R_ASSERT3(m_sdlWnd, "Unable to create SDL window", SDL_GetError());
        Msg("[OpenXRay] Device::Initialize: stage 4 SDL_CreateWindow done: %p", m_sdlWnd);

#if !defined(__ANDROID__)
        SDL_SetWindowHitTest(m_sdlWnd, WindowHitTest, nullptr);
        SDL_SetWindowMinimumSize(m_sdlWnd, 256, 192);
        ExtractAndSetWindowIcon(m_sdlWnd, icon);
#endif
        xrDebug::SetWindowHandler(this);
        Msg("[OpenXRay] Device::Initialize: stage 5 WindowHandler set");

        TracySetProgramName(title);
    }
    Msg("[OpenXRay] Device::Initialize: stage 6 inner block done");

#if defined(IMGUI_ENABLE_VIEWPORTS) && !defined(__ANDROID__)
    // Register main window handle (which is owned by the main application, not by us)
    // This is mostly for consistency, so that our code can use same logic for main and secondary viewports.
    {
        ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        main_viewport->PlatformUserData = IM_NEW(ImGuiViewportData){ m_sdlWnd };
        main_viewport->PlatformHandle = m_sdlWnd;
        main_viewport->PlatformHandleRaw = nullptr;
    }
#endif

    if (!GEnv.isDedicatedServer)
    {
        Msg("[OpenXRay] Device::Initialize: stage 7 adding m_editor");
        seqFrame.Add(&m_editor, -5);
    }
    Msg("[OpenXRay] Device::Initialize: stage 8 all done");
}

void CRenderDevice::DumpStatistics(IGameFont& font, IPerformanceAlert* alert)
{
    font.OutNext("*** ENGINE:   %2.2fms", stats.EngineTotal.result);
    font.OutNext("FPS/RFPS:     %3.1f/%3.1f", stats.fFPS, stats.fRFPS);
    font.OutNext("TPS:          %2.2f M", stats.fTPS);
    if (alert && stats.fFPS < 30)
        alert->Print(font, "FPS       < 30:   %3.1f", stats.fFPS);
}

SDL_HitTestResult WindowHitTest(SDL_Window* /*window*/, const SDL_Point* pArea, void* /*data*/)
{
    if (!Device.IsWindowDraggable())
        return SDL_HITTEST_NORMAL;

    SDL_Point area = *pArea; // copy
    const auto& rect = Device.m_rcWindowClient;

    // size of additional interactive area (in pixels)
    constexpr int hit = 15;
    constexpr int fix = 65535; // u32(-1)

    // Workaround for SDL bug
    if (area.x + hit >= fix && rect.w <= fix - hit)
        area.x -= fix;

    const bool leftSide = area.x <= rect.x + hit;
    const bool topSide = area.y <= rect.y + hit;
    const bool bottomSide = area.y >= rect.h - hit;
    const bool rightSide = area.x >= rect.w - hit;

    if (leftSide && topSide)
        return SDL_HITTEST_RESIZE_TOPLEFT;

    if (rightSide && topSide)
        return SDL_HITTEST_RESIZE_TOPRIGHT;

    if (rightSide && bottomSide)
        return SDL_HITTEST_RESIZE_BOTTOMRIGHT;

    if (leftSide && bottomSide)
        return SDL_HITTEST_RESIZE_BOTTOMLEFT;

    if (topSide)
        return SDL_HITTEST_RESIZE_TOP;

    if (rightSide)
        return SDL_HITTEST_RESIZE_RIGHT;

    if (bottomSide)
        return SDL_HITTEST_RESIZE_BOTTOM;

    if (leftSide)
        return SDL_HITTEST_RESIZE_LEFT;

    return SDL_HITTEST_DRAGGABLE;
}

void* CRenderDevice::GetApplicationWindowHandle() const
{
#if defined(XR_PLATFORM_WINDOWS)
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(m_sdlWnd, &info))
        return info.info.win.window;
#endif
    return nullptr;
}

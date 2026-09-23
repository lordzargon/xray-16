#include "stdafx.h"
#pragma hdrstop

#include "xr_input.h"
#include "IInputReceiver.h"
#include "GameFont.h"
#include "XR_IOConsole.h"
#include "xrCore/Text/StringConversion.hpp"
#include "IGame_Level.h"

#include <locale>

CInput* pInput = nullptr;

class DummyReceiver : public IInputReceiver
{
public:
    void IR_OnKeyboardPress(int dik) override
    {
        switch (GetBindedAction(dik))
        {
        case kQUIT:
            if (Console)
                Console->Execute("main_menu");
            return;

        case kCONSOLE:
            if (Console)
                Console->Show();
            return;

        case kEDITOR:
            if (Device.b_is_Ready)
                Device.editor().SwitchToNextState();
            return;
        }
    }
} dummyController;

ENGINE_API float psMouseSens = 1.f;
ENGINE_API float psMouseSensScale = 1.f;
ENGINE_API Flags32 psMouseInvert = {};

ENGINE_API float psControllerStickSensX = 0.12f;
ENGINE_API float psControllerStickSensY = 0.7f;
ENGINE_API float psControllerStickSensScale = 1.f;
ENGINE_API float psControllerStickInnerDeadZone = 0.15f;
ENGINE_API float psControllerStickOuterDeadZone = 0.96f;
ENGINE_API float psControllerStickAngularDeadZone = 0.95f;
ENGINE_API float psControllerSensorSens = 0.5f;
ENGINE_API float psControllerSensorDeadZone = 0.005f;
ENGINE_API Flags32 psControllerFlags = { ControllerEnableSensors };

ENGINE_API float psControllerCursorAutohideTime = 1.5f;

// Touch control variables (Android / touch-enabled platforms)
ENGINE_API int   psTouchEnable  = 1;    // enabled by default on Android
ENGINE_API float psTouchOpacity = 0.65f;
ENGINE_API float psTouchSens    = 1.0f;

static bool AltF4Pressed = false;

// Max events per frame
constexpr size_t MAX_KEYBOARD_EVENTS  = 64;
constexpr size_t MAX_MOUSE_EVENTS     = 256;
constexpr size_t MAX_CONTROLLER_EVENTS = 256;
constexpr size_t MAX_TOUCH_EVENTS     = 64;

CInput::CInput(const bool exclusive)
{
    ZoneScoped;

    exclusiveInput = exclusive;

    Log("Starting INPUT device...");

    mouseState.reset();
    keyboardState.reset();

    //===================== Dummy pack
    iCapture(&dummyController);

    SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1"); // We need to handle it manually

    Device.seqAppActivate.Add(this);
    Device.seqAppDeactivate.Add(this, REG_PRIORITY_HIGH);
    Device.seqFrame.Add(this, REG_PRIORITY_HIGH);

    mouseCursors[SDL_SYSTEM_CURSOR_ARROW]     = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    mouseCursors[SDL_SYSTEM_CURSOR_IBEAM]     = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
    mouseCursors[SDL_SYSTEM_CURSOR_WAIT]      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
    mouseCursors[SDL_SYSTEM_CURSOR_CROSSHAIR] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
    mouseCursors[SDL_SYSTEM_CURSOR_WAITARROW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAITARROW);
    mouseCursors[SDL_SYSTEM_CURSOR_SIZENWSE]  = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
    mouseCursors[SDL_SYSTEM_CURSOR_SIZENESW]  = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
    mouseCursors[SDL_SYSTEM_CURSOR_SIZEWE]    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    mouseCursors[SDL_SYSTEM_CURSOR_SIZENS]    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    mouseCursors[SDL_SYSTEM_CURSOR_SIZEALL]   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    mouseCursors[SDL_SYSTEM_CURSOR_NO]        = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);
    mouseCursors[SDL_SYSTEM_CURSOR_HAND]      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);

    for (int i = 0; i < SDL_NumJoysticks(); ++i)
        OpenController(i);

    // Load community controller database so Bluetooth gamepads are recognised correctly
    LoadGameControllerDatabase();
}

CInput::~CInput()
{
    ZoneScoped;

    GrabInput(false);

    for (auto& controller : controllers)
        SDL_GameControllerClose(controller);

    for (auto& cursor : mouseCursors)
    {
        SDL_FreeCursor(cursor);
        cursor = nullptr;
    }
    lastCursor = nullptr;

    Device.seqFrame.Remove(this);
    Device.seqAppDeactivate.Remove(this);
    Device.seqAppActivate.Remove(this);
}

void CInput::OpenController(int idx)
{
    if (!SDL_IsGameController(idx))
        return;

    const auto controller = SDL_GameControllerOpen(idx);
    if (!controller)
        return;

    if (psControllerFlags.test(ControllerEnableSensors))
        SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_TRUE);

    controllers.emplace_back(controller);
}

//-----------------------------------------------------------------------
// Load community gamecontrollerdb mappings from the app's assets or filesystem.
// Covers Xbox Bluetooth, PS4/PS5 DualShock/DualSense, Switch Pro, and
// generic Android Bluetooth HID gamepads.
void CInput::LoadGameControllerDatabase()
{
    // Try the assets path first (Android APK bundle)
    pcstr paths[] =
    {
        "gamecontrollerdb.txt",
        "assets/gamecontrollerdb.txt",
    };

    for (pcstr path : paths)
    {
        SDL_RWops* rw = SDL_RWFromFile(path, "rb");
        if (!rw)
            continue;

        const int added = SDL_GameControllerAddMappingsFromRW(rw, SDL_TRUE);
        if (added >= 0)
            Msg("~Input: Loaded %d controller mappings from %s", added, path);
        else
            Msg("!Input: Failed to parse controller mappings from %s: %s", path, SDL_GetError());
        return;
    }

    Msg("~Input: gamecontrollerdb.txt not found – using SDL built-in mappings only");
}


void CInput::DumpStatistics(IGameFont& font, IPerformanceAlert* alert)
{
    font.OutNext("*** INPUT:    %2.2fms", GetStats().FrameTime.result);
}

void CInput::SetCurrentInputType(InputType type)
{
    currentInputType = type;

    switch (type)
    {
    case KeyboardMouse:
        controllerState.id = -1;
        if (psControllerFlags.test(ControllerEnableSensors))
        {
            for (auto controller : controllers)
                SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_FALSE);
        }
        break;

    case Controller:
        if (psControllerFlags.test(ControllerEnableSensors))
        {
            for (auto controller : controllers)
                SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_TRUE);
        }
        break;
    }
    // Always flush it. On the first controller invocation,
    // prefer to receive sensor updates "from scratch",
    // on the next frame.
    SDL_FlushEvent(SDL_CONTROLLERSENSORUPDATE);
}

void CInput::MouseUpdate()
{
    ZoneScoped;

    // Mouse2 is a middle button in SDL,
    // but in X-Ray this is a right button
    constexpr int RemapIdx[] = { 0, 2, 1, 3, 4 };
    constexpr int IdxToKey[] = { MOUSE_1, MOUSE_2, MOUSE_3, MOUSE_4, MOUSE_5 };
    static_assert(std::size(RemapIdx) == COUNT_MOUSE_BUTTONS);
    static_assert(std::size(IdxToKey) == COUNT_MOUSE_BUTTONS);

    bool mouseMoved = false;
    int offs[2]{};
    float scroll[2]{};
    const auto mousePrev = mouseState;
    mouseAxisState[2] = 0;
    mouseAxisState[3] = 0;

    SDL_Event events[MAX_MOUSE_EVENTS];
    const auto count = SDL_PeepEvents(events, MAX_MOUSE_EVENTS,
        SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEWHEEL);

    for (int i = 0; i < count; ++i)
    {
        const SDL_Event& event = events[i];

        switch (event.type)
        {
        case SDL_MOUSEMOTION:
            if (psTouchEnable && g_pGameLevel && g_pGameLevel->bReady && event.motion.which == SDL_TOUCH_MOUSEID)
                break;
            mouseMoved = true;
            offs[0] += event.motion.xrel;
            offs[1] += event.motion.yrel;
            mouseAxisState[0] = event.motion.x;
            mouseAxisState[1] = event.motion.y;
            break;

        case SDL_MOUSEBUTTONDOWN:
        {
            if (psTouchEnable && g_pGameLevel && g_pGameLevel->bReady && event.button.which == SDL_TOUCH_MOUSEID)
                break;
            if (event.button.button >= 1 && event.button.button <= (Uint8)COUNT_MOUSE_BUTTONS)
            {
                const auto idx = RemapIdx[event.button.button - 1];
                mouseState[idx] = true;
                if (!cbStack.empty())
                    cbStack.back()->IR_OnMousePress(IdxToKey[idx]);
            }
            break;
        }
        case SDL_MOUSEBUTTONUP:
        {
            if (psTouchEnable && g_pGameLevel && g_pGameLevel->bReady && event.button.which == SDL_TOUCH_MOUSEID)
                break;
            if (event.button.button >= 1 && event.button.button <= (Uint8)COUNT_MOUSE_BUTTONS)
            {
                const auto idx = RemapIdx[event.button.button - 1];
                mouseState[idx] = false;
                if (!cbStack.empty())
                    cbStack.back()->IR_OnMouseRelease(IdxToKey[idx]);
            }
            break;
        }
        case SDL_MOUSEWHEEL:
            mouseMoved = true;
            scroll[0] += event.wheel.preciseX;
            scroll[1] += event.wheel.preciseY;
            mouseAxisState[2] += event.wheel.x;
            mouseAxisState[3] += event.wheel.y;
            break;
        }
    }

    if (!cbStack.empty())
    {
        for (int i = 0; i < MOUSE_COUNT; ++i)
        {
            if (mouseState[i] && mousePrev[i])
                cbStack.back()->IR_OnMouseHold(IdxToKey[i]);
        }

        if (mouseMoved)
        {
            if (offs[0] || offs[1])
                cbStack.back()->IR_OnMouseMove(offs[0], offs[1]);

            if (!fis_zero(scroll[0]) || !fis_zero(scroll[1]))
                cbStack.back()->IR_OnMouseWheel(scroll[0], scroll[1]);
        }
    }
}

void CInput::KeyUpdate()
{
    ZoneScoped;

    SDL_Event events[MAX_KEYBOARD_EVENTS];
    const auto count = SDL_PeepEvents(events, MAX_KEYBOARD_EVENTS,
        SDL_GETEVENT, SDL_KEYDOWN, SDL_KEYMAPCHANGED);

    // Let iGetAsyncKeyState work correctly during this frame immediately
    for (int i = 0; i < count; ++i)
    {
        const SDL_Event& event = events[i];

        switch (event.type)
        {
        case SDL_KEYDOWN:
            if (event.key.repeat)
                continue;
            keyboardState[event.key.keysym.scancode] = true;
            break;

        case SDL_KEYUP:
            keyboardState[event.key.keysym.scancode] = false;
            break;
        }
    }

    if (keyboardState[SDL_SCANCODE_F4] && (keyboardState[SDL_SCANCODE_LALT] || keyboardState[SDL_SCANCODE_RALT]))
    {
        AltF4Pressed = true;
        Engine.Event.Defer("KERNEL:disconnect");
        Engine.Event.Defer("KERNEL:quit");
        return;
    }

    if (count)
        SetCurrentInputType(KeyboardMouse);

    // If textInputCounter has changed,
    // we assume that text input target changed.
    // Theoretically, this is not always true, though.
    // But we always can change the solution.
    // If we find out something not work as expected.
    const auto cnt = textInputCounter;

    for (int i = 0; i < count; ++i)
    {
        const SDL_Event& event = events[i];

        switch (event.type)
        {
        case SDL_KEYDOWN:
            if (event.key.repeat)
                continue;
            if (!cbStack.empty())
                cbStack.back()->IR_OnKeyboardPress(event.key.keysym.scancode);
            break;

        case SDL_KEYUP:
            if (!cbStack.empty())
                cbStack.back()->IR_OnKeyboardRelease(event.key.keysym.scancode);
            break;

        case SDL_TEXTINPUT:
            if (cnt != textInputCounter)
                continue; // if input target changed, skip this frame
            if (!cbStack.empty())
                cbStack.back()->IR_OnTextInput(event.text.text);
            break;

        case SDL_KEYMAPCHANGED:
            seqKeyMapChanged.Process();
            break;
        }
    }

    if (!cbStack.empty())
    {
        for (u32 i = 0; i < COUNT_KB_BUTTONS; ++i)
            if (keyboardState[i])
                cbStack.back()->IR_OnKeyboardHold(i);
    }
}

bool ControllerState::attitude_changed() const
{
    // XXX: maybe check if magnitude is 0 instead?
    return gyroscope.similar(Fvector{ 0.f, 0.f, 0.f }, psControllerSensorDeadZone);
}

void CInput::ControllerUpdate()
{
    ZoneScoped;

    constexpr int ControllerButtonToKey[] =
    {
        XR_CONTROLLER_BUTTON_A,
        XR_CONTROLLER_BUTTON_B,
        XR_CONTROLLER_BUTTON_X,
        XR_CONTROLLER_BUTTON_Y,
        XR_CONTROLLER_BUTTON_BACK,
        XR_CONTROLLER_BUTTON_GUIDE,
        XR_CONTROLLER_BUTTON_START,
        XR_CONTROLLER_BUTTON_LEFTSTICK,
        XR_CONTROLLER_BUTTON_RIGHTSTICK,
        XR_CONTROLLER_BUTTON_LEFTSHOULDER,
        XR_CONTROLLER_BUTTON_RIGHTSHOULDER,
        XR_CONTROLLER_BUTTON_DPAD_UP,
        XR_CONTROLLER_BUTTON_DPAD_DOWN,
        XR_CONTROLLER_BUTTON_DPAD_LEFT,
        XR_CONTROLLER_BUTTON_DPAD_RIGHT,
        XR_CONTROLLER_BUTTON_MISC1,
        XR_CONTROLLER_BUTTON_PADDLE1,
        XR_CONTROLLER_BUTTON_PADDLE2,
        XR_CONTROLLER_BUTTON_PADDLE3,
        XR_CONTROLLER_BUTTON_PADDLE4,
        XR_CONTROLLER_BUTTON_TOUCHPAD,
    };

    SDL_Event events[MAX_CONTROLLER_EVENTS];
    auto count = SDL_PeepEvents(events, MAX_CONTROLLER_EVENTS,
        SDL_GETEVENT, SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMAPPED);

    for (int i = 0; i < count; ++i)
    {
        const SDL_Event& event = events[i];
        switch (event.type)
        {
        case SDL_CONTROLLERDEVICEADDED:
            OpenController(event.cdevice.which);
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
        {
            const auto controller = SDL_GameControllerFromInstanceID(event.cdevice.which);
            const auto it = std::find(controllers.begin(), controllers.end(), controller);
            if (it != controllers.end())
                controllers.erase(it);
            break;
        }

        case SDL_CONTROLLERDEVICEREMAPPED:
            // We are skipping it,
            // but it's in the SDL_PeepEvents call
            // to make sure it's removed from event queue
            break;
        } // switch (event.type)
    }

    if (!IsControllerAvailable())
        return;

    count = SDL_PeepEvents(nullptr, 0,
        SDL_PEEKEVENT, SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERTOUCHPADUP);

    if (count)
        SetCurrentInputType(Controller);
    else if (currentInputType != Controller)
        return;

    count = SDL_PeepEvents(events, MAX_CONTROLLER_EVENTS,
        SDL_GETEVENT, SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERSENSORUPDATE);

    constexpr ControllerAxisState pressedAxis{ 1.0f };
    constexpr ControllerAxisState releasedAxis{};

    static_assert(SDL_CONTROLLER_AXIS_MAX == 6, "Align the depending code with the changes in SDL_GameControllerAxis.");
    static float axes[SDL_CONTROLLER_AXIS_MAX]{};
    bool axisMoved[SDL_CONTROLLER_AXIS_MAX]{};
    const auto controllerPrev = controllerState;

    for (int i = 0; i < count; ++i)
    {
        const SDL_Event& event = events[i];

        switch (event.type)
        {
        case SDL_CONTROLLERAXISMOTION:
        {
            if (controllerState.id != event.caxis.which) // don't write if don't really need to
                controllerState.id = event.caxis.which;

            axisMoved[event.caxis.axis] = true;
            axes[event.caxis.axis] = event.caxis.value;
            break;
        }

        case SDL_CONTROLLERBUTTONDOWN:
            if (controllerState.id != event.cbutton.which) // don't write if don't really need to
                controllerState.id = event.cbutton.which;

            controllerState.buttons[event.cbutton.button] = true;
            cbStack.back()->IR_OnControllerPress(ControllerButtonToKey[event.cbutton.button], pressedAxis);
            break;

        case SDL_CONTROLLERBUTTONUP:
            if (controllerState.id != event.cbutton.which) // don't write if don't really need to
                controllerState.id = event.cbutton.which;

            controllerState.buttons[event.cbutton.button] = false;
            cbStack.back()->IR_OnControllerRelease(ControllerButtonToKey[event.cbutton.button], releasedAxis);
            break;

        case SDL_CONTROLLERSENSORUPDATE:
        {
            if (controllerState.id != event.csensor.which)
                break; // only use data from the recently used controller
            if (event.csensor.sensor != SDL_SENSOR_GYRO)
                break;

            controllerState.gyroscope = Fvector{ -event.csensor.data[1], -event.csensor.data[0], -event.csensor.data[2] };
            if (controllerState.attitude_changed())
                cbStack.back()->IR_OnControllerAttitudeChange(controllerState.gyroscope);
            break;
        }
        } // switch (event.type)
    }

    for (int i = 0; i < XR_CONTROLLER_BUTTON_COUNT; ++i)
    {
        if (controllerState.buttons[i] && controllerPrev.buttons[i])
            cbStack.back()->IR_OnControllerHold(ControllerButtonToKey[i], pressedAxis);
    }

    const float innerDeadZone = psControllerStickInnerDeadZone * SDL_JOYSTICK_AXIS_MAX;
    const float outerDeadZone = psControllerStickOuterDeadZone * SDL_JOYSTICK_AXIS_MAX;

    const auto applyStickDeadZone = [&](Fvector2 axis) -> ControllerAxisState
    {
        float magnitude = axis.magnitude();

        if (magnitude <= innerDeadZone || psControllerStickInnerDeadZone >= 1.0f)
            return {};

        axis.div(magnitude);

        if (magnitude > outerDeadZone)
            magnitude = outerDeadZone;

        const float normalizedMagnitude = (magnitude - innerDeadZone) / (outerDeadZone - innerDeadZone);
        axis.mul(normalizedMagnitude);
        return { axis, normalizedMagnitude };
    };

    const auto applyTriggerDeadZone = [](float value) -> ControllerAxisState
    {
        return value / SDL_JOYSTICK_AXIS_MAX;
    };

    if (axisMoved[0] || axisMoved[1])
        controllerState.axis.left = applyStickDeadZone({ axes[0], axes[1] });
    if (axisMoved[2] || axisMoved[3])
        controllerState.axis.right = applyStickDeadZone({ axes[2], axes[3] });
    if (axisMoved[4])
        controllerState.axis.trigger_left = applyTriggerDeadZone(axes[4]);
    if (axisMoved[5])
        controllerState.axis.trigger_right = applyTriggerDeadZone(axes[5]);

    const auto checkAxis = [this](int axis, const ControllerAxisState& state, const ControllerAxisState& prevState)
    {
        const bool isActive = !fis_zero(state.magnitude);
        const bool isPrevActive = !fis_zero(prevState.magnitude);

        if (isActive && isPrevActive)
            cbStack.back()->IR_OnControllerHold(axis, state);
        else if (isActive)
            cbStack.back()->IR_OnControllerPress(axis, state);
        else if (isPrevActive)
            cbStack.back()->IR_OnControllerRelease(axis, state);
    };

    checkAxis(XR_CONTROLLER_AXIS_LEFT,          controllerState.axis.left,          controllerPrev.axis.left);
    checkAxis(XR_CONTROLLER_AXIS_RIGHT,         controllerState.axis.right,         controllerPrev.axis.right);
    checkAxis(XR_CONTROLLER_AXIS_TRIGGER_LEFT,  controllerState.axis.trigger_left,  controllerPrev.axis.trigger_left);
    checkAxis(XR_CONTROLLER_AXIS_TRIGGER_RIGHT, controllerState.axis.trigger_right, controllerPrev.axis.trigger_right);
}

bool KbdKeyToButtonName(const int dik, xr_string& result)
{
    static std::locale locale("");

    if (dik >= 0)
    {
        cpcstr name = SDL_GetKeyName(SDL_GetKeyFromScancode((SDL_Scancode)dik));
        if (name && name[0])
        {
            result = StringFromUTF8(name, locale);
            return true;
        }
    }

    return false;
}

bool OtherDevicesKeyToButtonName(const int btn, xr_string& /*result*/)
{
    if (btn > CInput::COUNT_KB_BUTTONS)
    {
        // XXX: Not implemented
        return false;
    }

    return false;
}

bool CInput::GetKeyName(const int dik, pstr dest_str, int dest_sz)
{
    xr_string keyname;
    bool result;

    if (dik < COUNT_KB_BUTTONS)
        result = KbdKeyToButtonName(dik, keyname);
    else
        result = OtherDevicesKeyToButtonName(dik, keyname);

    if (keyname.empty())
        return false;

    xr_strcpy(dest_str, dest_sz, keyname.c_str());
    return result;
}

bool CInput::iGetAsyncKeyState(const int key)
{
    if (key < COUNT_KB_BUTTONS)
        return keyboardState[key];

    if (key > MOUSE_INVALID && key < MOUSE_MAX)
    {
        const int idx = key - (MOUSE_INVALID + 1);
        return mouseState[idx];
    }

    if (key > XR_CONTROLLER_BUTTON_INVALID && key < XR_CONTROLLER_BUTTON_MAX)
    {
        const int idx = key - (XR_CONTROLLER_BUTTON_INVALID + 1);
        return controllerState.buttons[idx];
    }

    if (key > XR_CONTROLLER_AXIS_INVALID && key < XR_CONTROLLER_AXIS_MAX)
    {
        return !fis_zero(controllerState.get_axis(key).magnitude);
    }

    // unknown key ???
    return false;
}

void CInput::iGetAsyncScrollPos(Ivector2& p) const
{
    p = { mouseAxisState[2], mouseAxisState[3] };
}

bool CInput::iGetAsyncMousePos(Ivector2& p, bool global /*= false*/) const
{
    if (global)
    {
#if SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE
        SDL_GetGlobalMouseState(&p.x, &p.y);
        return true;
#endif
        // if SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE unavailable
        // fallback to SDL_GetMouseState
        // but report false
    }
    SDL_GetMouseState(&p.x, &p.y);
    return !global;
}

bool CInput::iSetMousePos(const Ivector2& p, bool global /*= false*/) const
{
    if (global)
    {
#if SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE
        SDL_WarpMouseGlobal(p.x, p.y);
        return true;
#endif
        // if SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE unavailable
        // fallback to SDL_WarpMouseInWindow
        // but report false
    }

    SDL_WarpMouseInWindow(Device.m_sdlWnd, p.x, p.y);
    return !global;
}

void CInput::GrabInput(const bool grab)
{
    // Self descriptive
    ShowCursor(!grab);

    // Clip cursor to the current window
    // If SDL_HINT_GRAB_KEYBOARD is set then the keyboard will be grabbed too
    SDL_SetWindowGrab(Device.m_sdlWnd, grab ? SDL_TRUE : SDL_FALSE);

    // Grab the mouse
    if (exclusiveInput)
        SDL_SetRelativeMouseMode(grab ? SDL_TRUE : SDL_FALSE);

    // We're done here.
    inputGrabbed = grab;
}

bool CInput::InputIsGrabbed() const
{
    return inputGrabbed;
}

void CInput::ShowCursor(const bool show)
{
    SDL_ShowCursor(show ? SDL_TRUE : SDL_FALSE);
}

void CInput::SetCursor(const SDL_SystemCursor cursor)
{
    SDL_Cursor* expected_cursor = mouseCursors[cursor] ? mouseCursors[cursor] : mouseCursors[ImGuiMouseCursor_Arrow];
    if (lastCursor != expected_cursor) // SDL function doesn't have an early out
    {
        SDL_SetCursor(expected_cursor);
        lastCursor = expected_cursor;
    }
}

void CInput::EnableTextInput()
{
    ++textInputCounter;

    if (textInputCounter == 1)
        SDL_StartTextInput();

    SDL_PumpEvents();
    SDL_FlushEvents(SDL_TEXTEDITING, SDL_TEXTINPUT);
}

void CInput::DisableTextInput()
{
    --textInputCounter;
    if (textInputCounter < 0)
        textInputCounter = 0;

    if (textInputCounter == 0)
        SDL_StopTextInput();

    SDL_PumpEvents();
    SDL_FlushEvents(SDL_TEXTEDITING, SDL_TEXTINPUT);
}

bool CInput::IsTextInputEnabled() const
{
    return textInputCounter > 0;
}

void CInput::RegisterKeyMapChangeWatcher(pureKeyMapChanged* watcher, int priority /*= REG_PRIORITY_NORMAL*/)
{
    seqKeyMapChanged.Add(watcher, priority);
}

void CInput::RemoveKeyMapChangeWatcher(pureKeyMapChanged* watcher)
{
    seqKeyMapChanged.Remove(watcher);
}

void CInput::iCapture(IInputReceiver* p)
{
    VERIFY(p);

    // change focus
    if (!cbStack.empty())
        cbStack.back()->IR_OnDeactivate();
    cbStack.push_back(p);
    cbStack.back()->IR_OnActivate();

    // prepare for _new_ controller
    controllerState = {};
}

void CInput::iRelease(IInputReceiver* p)
{
    if (p == cbStack.back())
    {
        cbStack.back()->IR_OnDeactivate();
        cbStack.pop_back();
        cbStack.back()->IR_OnActivate();
    }
    else
    {
        // we are not topmost receiver, so remove the nearest one
        for (size_t cnt = cbStack.size(); cnt > 0; --cnt)
            if (cbStack[cnt - 1] == p)
            {
                xr_vector<IInputReceiver*>::iterator it = cbStack.begin();
                std::advance(it, cnt - 1);
                cbStack.erase(it);
                break;
            }
    }
}

void CInput::OnAppActivate(void)
{
    if (CurrentIR())
        CurrentIR()->IR_OnActivate();

    mouseState.reset();
    keyboardState.reset();
    controllerState = {};
}

void CInput::OnAppDeactivate(void)
{
    if (CurrentIR())
        CurrentIR()->IR_OnDeactivate();

    mouseState.reset();
    keyboardState.reset();
    controllerState = {};
}

void CInput::OnFrame(void)
{
    ZoneScoped;

    if (AltF4Pressed)
        return;

    stats.FrameStart();
    stats.FrameTime.Begin();

    // Pump *all* SDL events once at the start of the frame so that every
    // subsequent PeepEvents call works off a fresh, complete snapshot.
    SDL_PumpEvents();

    if (Device.dwPrecacheFrame == 0 && !Device.IsAnselActive)
    {
        ControllerUpdate();
        KeyUpdate();
        MouseUpdate();
        TouchUpdate();
    }

    stats.FrameTime.End();
    stats.FrameEnd();
}

IInputReceiver* CInput::CurrentIR()
{
    if (cbStack.size())
        return cbStack.back();

    return nullptr;
}

void CInput::ExclusiveMode(const bool exclusive)
{
    GrabInput(false);

    exclusiveInput = exclusive;

    GrabInput(true);
}

bool CInput::IsExclusiveMode() const
{
    return exclusiveInput;
}

void CInput::Feedback(FeedbackType type, float s1, float s2, float duration)
{
    const u16 s1_rumble = iFloor(u16(-1) * clampr(s1, 0.0f, 1.0f));
    const u16 s2_rumble = iFloor(u16(-1) * clampr(s2, 0.0f, 1.0f));
    const u32 duration_ms = duration < 0.f ? 0 : iFloor(duration * 1000.f);

    switch (type)
    {
    case FeedbackController:
    {
        if (controllerState.id != -1)
        {
            const auto controller = SDL_GameControllerFromInstanceID(controllerState.id);
            SDL_GameControllerRumble(controller, s1_rumble, s2_rumble, duration_ms);
        }
        break;
    }

    case FeedbackTriggers:
    {
        if (controllerState.id != -1)
        {
            const auto controller = SDL_GameControllerFromInstanceID(controllerState.id);
            SDL_GameControllerRumbleTriggers(controller, s1_rumble, s2_rumble, duration_ms);
        }
        break;
    }

    default: NODEFAULT;
    }
}

// ---------------------------------------------------------------------------
// TouchUpdate — process SDL finger events and synthesise game input
// ---------------------------------------------------------------------------
// Layout (all coords normalised 0..1 by SDL):
//   Left  40%  width  → virtual analogue stick zone
//   Right 60%  width  → look-swipe zone  (and action button zone)
//
// Action buttons (normalised positions, anchored to the right half):
//   FIRE  0.78, 0.70  r=0.07
//   AIM   0.88, 0.55  r=0.055
//   JUMP  0.93, 0.70  r=0.055
//   CROUCH 0.93, 0.85 r=0.05
//   USE   0.78, 0.85  r=0.05
//   RELOAD 0.88, 0.85 r=0.05
//   INV   0.05, 0.06  r=0.045
//   PDA   0.13, 0.06  r=0.045
//   MENU  0.21, 0.06  r=0.045
//   QUICK1-4  0.05..0.20, 0.15 r=0.04 each
// ---------------------------------------------------------------------------

// Key/button codes used for virtual button presses.
// These correspond to the input keys recognized by GetBindedAction().
#include "xr_level_controller.h"

static constexpr int TouchButtonKey[CInput::TB_COUNT] =
{
    MOUSE_1,             // TB_FIRE   -> kWPN_FIRE
    MOUSE_2,             // TB_AIM    -> kWPN_ZOOM
    SDL_SCANCODE_SPACE,  // TB_JUMP   -> kJUMP
    SDL_SCANCODE_LCTRL,  // TB_CROUCH -> kCROUCH
    SDL_SCANCODE_F,      // TB_USE    -> kUSE
    SDL_SCANCODE_R,      // TB_RELOAD -> kWPN_RELOAD
    SDL_SCANCODE_I,      // TB_INV    -> kINVENTORY
    SDL_SCANCODE_P,      // TB_PDA    -> kACTIVE_JOBS
    SDL_SCANCODE_ESCAPE, // TB_MENU   -> kQUIT
    SDL_SCANCODE_F1,     // TB_QUICK1 -> kQUICK_USE_1
    SDL_SCANCODE_F2,     // TB_QUICK2 -> kQUICK_USE_2
    SDL_SCANCODE_F3,     // TB_QUICK3 -> kQUICK_USE_3
    SDL_SCANCODE_F4,     // TB_QUICK4 -> kQUICK_USE_4
};

// Rebuild the button layout rects every frame (cheap; avoids resolution-change issues)
static void RebuildTouchButtonLayout(CInput::CTouchState& ts)
{
    const float W = static_cast<float>(Device.dwWidth);
    const float H = static_cast<float>(Device.dwHeight);

    // Helper: circle defined by center in pixels (cx_px, cy_px) and pixel radius r_px
    auto makeBtn = [W, H](float cx_px, float cy_px, float r_px) -> SDL_FRect {
        const float rw = r_px / W;
        const float rh = r_px / H;
        const float cx = cx_px / W;
        const float cy = cy_px / H;
        return { cx - rw, cy - rh, rw * 2.f, rh * 2.f };
    };

    // Right-side thumb cluster
    ts.buttonRects[CInput::TB_FIRE]   = makeBtn(W - H * 0.34f, H * 0.62f, H * 0.095f);
    ts.buttonRects[CInput::TB_AIM]    = makeBtn(W - H * 0.16f, H * 0.40f, H * 0.080f);
    ts.buttonRects[CInput::TB_JUMP]   = makeBtn(W - H * 0.13f, H * 0.62f, H * 0.075f);
    ts.buttonRects[CInput::TB_RELOAD] = makeBtn(W - H * 0.24f, H * 0.78f, H * 0.070f);
    ts.buttonRects[CInput::TB_USE]    = makeBtn(W - H * 0.36f, H * 0.84f, H * 0.070f);
    ts.buttonRects[CInput::TB_CROUCH] = makeBtn(W - H * 0.13f, H * 0.84f, H * 0.070f);

    // Top-center utility buttons (safe from camera notch and minimap)
    ts.buttonRects[CInput::TB_INV]    = makeBtn(W * 0.5f - H * 0.22f, H * 0.07f, H * 0.045f);
    ts.buttonRects[CInput::TB_PDA]    = makeBtn(W * 0.5f - H * 0.08f, H * 0.07f, H * 0.045f);
    ts.buttonRects[CInput::TB_MENU]   = makeBtn(W * 0.5f + H * 0.08f, H * 0.07f, H * 0.045f);

    // Quick slots
    ts.buttonRects[CInput::TB_QUICK1] = makeBtn(W * 0.5f + H * 0.22f, H * 0.07f, H * 0.040f);
    ts.buttonRects[CInput::TB_QUICK2] = makeBtn(W * 0.5f + H * 0.31f, H * 0.07f, H * 0.040f);
    ts.buttonRects[CInput::TB_QUICK3] = makeBtn(W * 0.5f + H * 0.40f, H * 0.07f, H * 0.040f);
    ts.buttonRects[CInput::TB_QUICK4] = makeBtn(W * 0.5f + H * 0.49f, H * 0.07f, H * 0.040f);
}

static bool PointInButton(float nx, float ny, const SDL_FRect& r, float W, float H)
{
    const float bcx = (r.x + r.w * 0.5f) * W;
    const float bcy = (r.y + r.h * 0.5f) * H;
    const float br  = r.w * 0.5f * W;
    const float dx  = nx * W - bcx;
    const float dy  = ny * H - bcy;
    return (dx * dx + dy * dy) <= (br * br);
}

void CInput::TouchUpdate()
{
    ZoneScoped;

    if (!psTouchEnable)
        return;

    if (cbStack.empty())
        return;

    // In menus or loading screens, touch is handled naturally by SDL's touch-to-mouse translation
    if (!g_pGameLevel || !g_pGameLevel->bReady)
    {
        touchState.Reset();
        return;
    }

    IInputReceiver* ir = cbStack.back();

    RebuildTouchButtonLayout(touchState);

    SDL_Event events[MAX_TOUCH_EVENTS];
    const auto count = SDL_PeepEvents(events, (int)MAX_TOUCH_EVENTS,
        SDL_GETEVENT, SDL_FINGERDOWN, SDL_FINGERUP);

    if (count > 0)
        SetCurrentInputType(KeyboardMouse);

    const float W = static_cast<float>(Device.dwWidth);
    const float H = static_cast<float>(Device.dwHeight);
    const float stickMaxRadius = H * 0.17f * psTouchSens;
    constexpr float STICK_LEFT_ZONE = 0.40f;

    auto updateStickDeflection = [&](float px, float py)
    {
        touchState.stickCurrent.set(px, py);
        Fvector2 delta;
        delta.sub(touchState.stickCurrent, touchState.stickOrigin);

        float magnitude = delta.magnitude();
        if (magnitude > stickMaxRadius)
        {
            delta.div(magnitude);
            delta.mul(stickMaxRadius);
            magnitude = stickMaxRadius;
        }

        const float normMag = (stickMaxRadius > 0.f) ? (magnitude / stickMaxRadius) : 0.f;
        Fvector2 normDelta = delta;
        if (magnitude > 0.f)
            normDelta.div(magnitude);
        normDelta.mul(normMag);

        touchState.stickDelta = normDelta;
        touchState.stickMagnitude = normMag;
    };

    for (int i = 0; i < count; ++i)
    {
        const SDL_TouchFingerEvent& tf = events[i].tfinger;
        const float nx = tf.x;
        const float ny = tf.y;
        const float px = nx * W;
        const float py = ny * H;

        if (tf.type == SDL_FINGERDOWN)
        {
            // 1. Check if landing on any action button
            bool onButton = false;
            for (int b = 0; b < TB_COUNT; ++b)
            {
                if (!touchState.buttonDown[b] && PointInButton(nx, ny, touchState.buttonRects[b], W, H))
                {
                    touchState.buttonDown[b]   = true;
                    touchState.buttonFinger[b] = tf.fingerId;
                    onButton = true;

                    const int key = TouchButtonKey[b];
                    if (key == MOUSE_1)
                        mouseState[0] = true;
                    else if (key == MOUSE_2)
                        mouseState[1] = true;
                    else if (key < COUNT_KB_BUTTONS)
                        keyboardState[key] = true;

                    ir->IR_OnKeyboardPress(key);
                    break;
                }
            }

            if (onButton)
                continue;

            // 2. Left zone: virtual movement stick
            if (nx < STICK_LEFT_ZONE && !touchState.stickFingerActive)
            {
                touchState.stickFingerActive = true;
                touchState.stickFingerId     = tf.fingerId;
                touchState.stickOrigin.set(px, py);
                touchState.stickCurrent.set(px, py);
                touchState.stickDelta = {};
                touchState.stickMagnitude = 0.f;
                continue;
            }

            // 3. Right zone (or remaining area): look swipe
            if (nx >= STICK_LEFT_ZONE && !touchState.lookFingerActive)
            {
                touchState.lookFingerActive = true;
                touchState.lookFingerId     = tf.fingerId;
                touchState.lookPrev.set(px, py);
            }
        }
        else if (tf.type == SDL_FINGERUP)
        {
            // Release action buttons
            for (int b = 0; b < TB_COUNT; ++b)
            {
                if (touchState.buttonDown[b] && touchState.buttonFinger[b] == tf.fingerId)
                {
                    touchState.buttonDown[b] = false;

                    const int key = TouchButtonKey[b];
                    if (key == MOUSE_1)
                        mouseState[0] = false;
                    else if (key == MOUSE_2)
                        mouseState[1] = false;
                    else if (key < COUNT_KB_BUTTONS)
                        keyboardState[key] = false;

                    ir->IR_OnKeyboardRelease(key);
                }
            }

            // Release stick
            if (touchState.stickFingerActive && tf.fingerId == touchState.stickFingerId)
            {
                touchState.stickFingerActive = false;
                touchState.stickMagnitude    = 0.f;
                touchState.stickDelta        = {};
                keyboardState[SDL_SCANCODE_LSHIFT] = false;
                ir->IR_OnControllerRelease(XR_CONTROLLER_AXIS_LEFT, ControllerAxisState{});
                ir->IR_OnKeyboardRelease(SDL_SCANCODE_LSHIFT);
            }

            // Release look swipe
            if (touchState.lookFingerActive && tf.fingerId == touchState.lookFingerId)
            {
                touchState.lookFingerActive = false;
            }
        }
    }

    // Process FINGERMOTION events
    const auto motionCount = SDL_PeepEvents(events, (int)MAX_TOUCH_EVENTS,
        SDL_GETEVENT, SDL_FINGERMOTION, SDL_FINGERMOTION);

    for (int i = 0; i < motionCount; ++i)
    {
        const SDL_TouchFingerEvent& tf = events[i].tfinger;
        const float nx = tf.x;
        const float ny = tf.y;
        const float px = nx * W;
        const float py = ny * H;

        // Stick motion
        if (touchState.stickFingerActive && tf.fingerId == touchState.stickFingerId)
        {
            updateStickDeflection(px, py);
        }

        // Look swipe motion
        if (touchState.lookFingerActive && tf.fingerId == touchState.lookFingerId)
        {
            const float dpx = (px - touchState.lookPrev.x) * psTouchSens;
            const float dpy = (py - touchState.lookPrev.y) * psTouchSens;
            touchState.lookPrev.set(px, py);

            if (!fis_zero(dpx) || !fis_zero(dpy))
                ir->IR_OnMouseMove(static_cast<int>(dpx), static_cast<int>(dpy));
        }
    }

    // Continuous hold: emit virtual stick hold event every frame while stick is deflected
    if (touchState.stickFingerActive && !fis_zero(touchState.stickMagnitude))
    {
        ControllerAxisState axisState{ touchState.stickDelta, touchState.stickMagnitude };
        ir->IR_OnControllerHold(XR_CONTROLLER_AXIS_LEFT, axisState);

        // Auto-sprint: push stick forward >= 95% to trigger sprint (Shift)
        if (touchState.stickMagnitude >= 0.95f && touchState.stickDelta.y < -0.5f)
        {
            keyboardState[SDL_SCANCODE_LSHIFT] = true;
            ir->IR_OnKeyboardHold(SDL_SCANCODE_LSHIFT);
        }
        else
        {
            keyboardState[SDL_SCANCODE_LSHIFT] = false;
        }
    }

    // Continuous hold: emit hold events for all currently pressed action buttons
    for (int b = 0; b < TB_COUNT; ++b)
    {
        if (touchState.buttonDown[b])
            ir->IR_OnKeyboardHold(TouchButtonKey[b]);
    }
}

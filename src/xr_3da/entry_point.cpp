#include "stdafx.h"

#include "xrEngine/x_ray.h"
#include "xrGame/xrGame.h"
#include "Include/xrRender/xrRender.h"

#if !defined(XR_PLATFORM_WINDOWS)
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#endif

// Always request high performance GPU
extern "C"
{
// https://docs.nvidia.com/gameworks/content/technologies/desktop/optimus.htm
XR_EXPORT u32 NvOptimusEnablement = 0x00000001; // NVIDIA Optimus

// https://gpuopen.com/amdpowerxpressrequesthighperformance/
XR_EXPORT u32 AmdPowerXpressRequestHighPerformance = 0x00000001; // PowerXpress or Hybrid Graphics
}

std::array<RendererModule*, 2> s_render_modules =
{
#ifdef XR_PLATFORM_WINDOWS
    xray::render::render_r4::GetRendererModule(),
#endif
    xray::render::render_gl::GetRendererModule(),
};

struct tracy_raii
{
    ~tracy_raii()
    {
#ifdef TRACY_ENABLE
        tracy::GetProfiler().RequestShutdown();
        while (!tracy::GetProfiler().HasShutdownFinished())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
#endif
    }
};

static bool HasCommandParam(pcstr commandLine, pcstr param)
{
    if (!commandLine || !param)
        return false;
    const size_t paramLen = xr_strlen(param);
    pcstr p = strstr(commandLine, param);
    while (p)
    {
        const char next = p[paramLen];
        if (next == ' ' || next == '\0' || next == '\t' || next == '\r' || next == '\n')
            return true;
        p = strstr(p + paramLen, param);
    }
    return false;
}

int entry_point(pcstr commandLine)
{
    tracy_raii raii;
    auto* game = HasCommandParam(commandLine, "-nogame") ? nullptr : &xrGame;

    CApplication app{ commandLine, game, s_render_modules };

    return app.Run();
}

#if defined(XR_PLATFORM_WINDOWS)
int StackoverflowFilter(const int exceptionCode)
{
    if (exceptionCode == EXCEPTION_STACK_OVERFLOW)
        return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_SEARCH;
}

int APIENTRY WinMain(HINSTANCE inst, HINSTANCE prevInst, char* commandLine, int cmdShow)
{
    int result = 0;
    // BugTrap can't handle stack overflow exception, so handle it here
    __try
    {
        result = entry_point(commandLine);
    }
    __except (StackoverflowFilter(GetExceptionCode()))
    {
        _resetstkoflw();
        FATAL("stack overflow");
    }

    return result;
}
#elif defined(__ANDROID__)
#include <android/log.h>
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, "OpenXRay", __VA_ARGS__)

extern "C" XR_EXPORT int SDL_main(int argc, char *argv[])
{
    int result = EXIT_FAILURE;

    try
    {
        std::string cmd;
        for (int i = 1; i < argc; ++i)
        {
            if (argv[i])
            {
                cmd += argv[i];
                cmd += " ";
            }
        }

        // On Android, if -fsltx is not explicitly provided, search standard locations
        if (cmd.find("-fsltx") == std::string::npos)
        {
            const char* candidatePaths[] = {
                "/sdcard/Android/data/org.openxray/files/fsgame.ltx",
                "/sdcard/OpenXRay/fsgame.ltx",
                "/storage/emulated/0/Android/data/org.openxray/files/fsgame.ltx",
                "/storage/emulated/0/OpenXRay/fsgame.ltx"
            };
            for (const char* path : candidatePaths)
            {
                if (access(path, F_OK) == 0)
                {
                    ALOGI("Discovered fsgame.ltx at: %s", path);
                    cmd += "-fsltx ";
                    cmd += path;
                    cmd += " ";

                    std::string dir = path;
                    const size_t slash = dir.find_last_of('/');
                    if (slash != std::string::npos)
                    {
                        const std::string dirPath = dir.substr(0, slash);
                        chdir(dirPath.c_str());
                        ALOGI("Changed working directory to: %s", dirPath.c_str());
                    }
                    break;
                }
            }
        }

        ALOGI("Starting OpenXRay with commandline: %s", cmd.c_str());
        result = entry_point(cmd.c_str());
    }
    catch (const std::overflow_error& e)
    {
        _resetstkoflw();
        FATAL_F("stack overflow: %s", e.what());
    }
    catch (const std::runtime_error& e)
    {
        FATAL_F("runtime error: %s", e.what());
    }
    catch (const std::exception& e)
    {
        FATAL_F("exception: %s", e.what());
    }
    catch (...)
    {
    }

    return result;
}

extern "C" XR_EXPORT int main(int argc, char *argv[])
{
    return SDL_main(argc, argv);
}
#else
int main(int argc, char *argv[])
{
    int result = EXIT_FAILURE;

    try
    {
        char* commandLine = nullptr;
        int i;
        if(argc > 1)
        {
            size_t sum = 1;
            for(i = 1; i < argc; ++i)
                sum += strlen(argv[i]) + 1;

            commandLine = (char*)xr_malloc(sum);
            ZeroMemory(commandLine, sum);

            for(i = 1; i < argc; ++i)
            {
                strcat(commandLine, argv[i]);
                strcat(commandLine, " ");
            }

            result = entry_point(commandLine);

            xr_free(commandLine);
        }
        else
            result = entry_point("");
    }
    catch (const std::overflow_error& e)
    {
        _resetstkoflw();
        FATAL_F("stack overflow: %s", e.what());
    }
    catch (const std::runtime_error& e)
    {
        FATAL_F("runtime error: %s", e.what());
    }
    catch (const std::exception& e)
    {
        FATAL_F("exception: %s", e.what());
    }
    catch (...)
    {
    // this executes if f() throws std::string or int or any other unrelated type
    }

    return result;
}
#endif

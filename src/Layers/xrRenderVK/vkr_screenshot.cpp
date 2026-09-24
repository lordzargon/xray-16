#include "stdafx.h"

#include "xrCore/Media/Image.hpp"
#include "xrEngine/xrImage_Resampler.h"

namespace xray::render::RENDER_NAMESPACE
{
using namespace XRay::Media;

void CRender::Screenshot(ScreenshotMode mode /*= SM_NORMAL*/, pcstr name /*= nullptr*/)
{
    switch (mode)
    {
    case SM_NORMAL:
    {
        // TODO: VK: Implement reading back from swapchain/staging image
        break;
    }

    case SM_FOR_GAMESAVE:
        break;

    default:
        break;
    }
}
} // namespace xray::render::RENDER_NAMESPACE

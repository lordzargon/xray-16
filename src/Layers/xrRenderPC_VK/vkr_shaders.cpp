#include "stdafx.h"
#include "Layers/xrRender/SH_Atomic.h"
#include "Layers/xrRender/ResourceManager.h"

namespace xray::render::RENDER_NAMESPACE
{
HRESULT CRender::shader_compile(pcstr name, IReader* fs, pcstr pFunctionName,
    pcstr pTarget, u32 Flags, void*& result)
{
    UNUSED(name);
    UNUSED(fs);
    UNUSED(pFunctionName);
    UNUSED(pTarget);
    UNUSED(Flags);
    UNUSED(result);

    // Initial Vulkan bring-up: return S_OK
    return S_OK;
}
} // namespace xray::render::RENDER_NAMESPACE

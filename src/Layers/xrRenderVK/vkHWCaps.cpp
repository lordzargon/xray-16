#include "stdafx.h"
#pragma hdrstop

#include "Layers/xrRender/HWCaps.h"
#include "vkHW.h"

namespace xray::render::RENDER_NAMESPACE
{
void CHWCaps::Update()
{
    // Geometry caps
    geometry_major = 4;
    geometry_minor = 0;
    geometry_profile = "vs_4_0";
    geometry.bSoftware = FALSE;
    geometry.bPointSprites = FALSE;
    geometry.bNPatches = FALSE;
    geometry.dwRegisters = 256;
    geometry.dwInstructions = 256;
    geometry.dwClipPlanes = 6;
    geometry.bVTF = TRUE;
    geometry.dwVertexCache = 24;

    // Raster caps
    raster_major = 4;
    raster_minor = 0;
    raster_profile = "ps_4_0";
    raster.dwStages = 15;
    raster.bNonPow2 = TRUE;
    raster.bCubemap = TRUE;
    raster.dwMRT_count = 4;
    raster.b_MRT_mixdepth = TRUE;
    raster.dwInstructions = 256;

    bTableFog = FALSE;
    bStencil = TRUE;
    bScissor = TRUE;

    bForceGPU_REF = false;
    bForceGPU_SW = false;
    bForceGPU_NonPure = false;
    SceneMode = FALSE;
    iGPUNum = 1;
}
} // namespace xray::render::RENDER_NAMESPACE

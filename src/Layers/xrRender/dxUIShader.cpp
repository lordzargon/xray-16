#include "stdafx.h"
#include "dxUIShader.h"

namespace xray::render::RENDER_NAMESPACE
{
void dxUIShader::Copy(IUIShader& _in) { *this = *((dxUIShader*)&_in); }
void dxUIShader::create(LPCSTR sh, LPCSTR tex) { hShader.create(sh, tex); }
void dxUIShader::destroy() { hShader.destroy(); }

bool dxUIShader::operator==(const IUIShader& other) const
{
    return hShader == static_cast<const dxUIShader&>(other).hShader;
}

CTexture* dxUIShader::GetBaseTexture() const
{
    if (!hShader || !hShader->E[0])
        return nullptr;

    if (hShader->E[0]->passes.empty())
        return nullptr;

    const SPass* pass = hShader->E[0]->passes[0]._get();
    if (!pass || !pass->T)
        return nullptr;

    const STextureList& textures = *pass->T;
    if (textures.empty())
        return nullptr;

    if (pass->constants)
    {
        const R_constant* sbase = pass->constants->get(baseTexture)._get();
        if (sbase)
        {
            for (const auto& [stage, texture] : textures)
            {
                if (stage == sbase->samp.index && texture)
                    return texture._get();
            }
            if (sbase->samp.index < textures.size() && textures[sbase->samp.index].second)
                return textures[sbase->samp.index].second._get();
        }
    }

    for (const auto& [stage, texture] : textures)
    {
        if (texture)
            return texture._get();
    }

    return nullptr;
}

xrImTextureData dxUIShader::GetImGuiTextureId()
{
    const auto texture = GetBaseTexture();
    if (!texture)
        return {};

    if (!texture->flags.bLoaded)
        texture->Load();

    return
    {
        texture->GetImTextureID(),
        {
            (float)texture->get_Width(),
            (float)texture->get_Height()
        }
    };
}

bool dxUIShader::GetBaseTextureResolution(Fvector2& res)
{
    const auto texture = GetBaseTexture();
    if (!texture)
    {
        res = {};
        return false;
    }

    if (!texture->flags.bLoaded)
        texture->Load();

    res = { float(texture->get_Width()), float(texture->get_Height()) };
    return true;
}
} // namespace xray::render::RENDER_NAMESPACE

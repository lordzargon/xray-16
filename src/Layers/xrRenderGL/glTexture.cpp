// Texture.cpp: implementation of the CTexture class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <gli/gli.hpp>
#include <gli/convert.hpp>

namespace xray::render::RENDER_NAMESPACE
{
void fix_texture_name(pstr fn)
{
    pstr _ext = strext(fn);
    if (_ext &&
        (0 == xr_stricmp(_ext, ".tga") ||
            0 == xr_stricmp(_ext, ".dds") ||
            0 == xr_stricmp(_ext, ".bmp") ||
            0 == xr_stricmp(_ext, ".ogm")))
        *_ext = 0;
}

int get_texture_load_lod(LPCSTR fn)
{
    CInifile::Sect& sect = pSettings->r_section("reduce_lod_texture_list");

    for (const auto& item : sect.Data)
    {
        if (strstr(fn, item.first.c_str()))
        {
            if (psTextureLOD < 1)
                return 0;
            if (psTextureLOD < 3)
                return 1;
            return 2;
        }
    }

    if (psTextureLOD < 2)
        return 0;
    if (psTextureLOD < 4)
        return 1;
    return 2;
}

u32 calc_texture_size(int lod, u32 mip_cnt, size_t orig_size)
{
    if (1 == mip_cnt)
        return orig_size;

    int _lod = lod;
    float res = float(orig_size);

    while (_lod > 0)
    {
        --_lod;
        res -= res / 1.333f;
    }
    return iFloor(res);
}

GLuint CRender::texture_load(LPCSTR fRName, u32& ret_msize, GLenum& ret_desc)
{
    ret_msize = 0;
    R_ASSERT1_CURE(fRName && fRName[0], { return 0; });

    GLuint pTexture = 0;
    string_path fn;
    {
        // make file name
        string_path fname;
        xr_strcpy(fname, fRName);
        fix_texture_name(fname);

        // Call to FS.exist WRITES to fn !

        if (!FS.exist(fn, "$game_textures$", fname, ".dds") && strstr(fname, "_bump"))
        {
            Msg("! Fallback to default bump map: %s", fname);
            if (strstr(fname, "_bump#"))
                R_ASSERT1_CURE(FS.exist(fn, "$game_textures$", "ed\\ed_dummy_bump#", ".dds"), return 0);
            else
                R_ASSERT1_CURE(FS.exist(fn, "$game_textures$", "ed\\ed_dummy_bump", ".dds"), return 0);
        }
        else
        {
            bool exist = false;

            for (cpcstr folder : { "$level$", "$game_saves$", "$game_textures$" })
            {
                exist = FS.exist(fn, folder, fname, ".dds");
                if (exist)
                    break;
            }

            if (!exist)
            {
                Msg("! Can't find texture '%s'", fname);
                if (!FS.exist(fn, "$game_textures$", "ed\\ed_not_existing_texture", ".dds"))
                    return 0;
            }
        }
    }

    // Load and get header
    IReader* S = FS.r_open(fn);
    R_ASSERT2_CURE(S, fn, { return 0; });
    size_t img_size = S->length();
#ifdef DEBUG
    Msg("* Loaded: %s[%d]b", fn, img_size);
#endif // DEBUG
    gli::texture texture = gli::load((char*)S->pointer(), img_size);
    R_ASSERT2(!texture.empty(), fn);

    xr_strlwr(fn);
    int img_loaded_lod = gli::is_target_cube(texture.target()) ? 0 : get_texture_load_lod(fn);
#if defined(__ANDROID__)
    // On Android, downscale world textures (> 512) by at least 1 mip level to prevent memory exhaustion / LMK kill
    if (!gli::is_target_cube(texture.target()) && img_loaded_lod == 0)
    {
        if (texture.extent().x > 512 || texture.extent().y > 512)
            img_loaded_lod = 1;
    }
#endif

    if (img_loaded_lod > 0 && texture.levels() > 1)
    {
        size_t base_level = std::min(static_cast<size_t>(img_loaded_lod), texture.levels() - 1);
        size_t max_level = texture.levels() - 1;
        texture = gli::texture(texture,
                               texture.target(),
                               texture.format(),
                               texture.base_layer(), texture.max_layer(),
                               texture.base_face(), texture.max_face(),
                               base_level, max_level);
    }

#if defined(__ANDROID__)
    if (gli::is_compressed(texture.format()))
    {
        if (texture.target() == gli::TARGET_2D)
        {
            gli::texture2d tex2d(texture);
            texture = gli::convert(tex2d, gli::FORMAT_RGBA8_UNORM_PACK8);
        }
        else if (texture.target() == gli::TARGET_CUBE)
        {
            gli::texture_cube texCube(texture);
            texture = gli::convert(texCube, gli::FORMAT_RGBA8_UNORM_PACK8);
        }
    }
#endif

    u32 mip_cnt = u32(-1); // XXX: write to it when reading with GLI!

    gli::gl GL(gli::gl::PROFILE_GL33);

    gli::gl::format const format = GL.translate(texture.format(), texture.swizzles());
    GLenum target = GL.translate(texture.target());

    glGenTextures(1, &pTexture);
    glBindTexture(target, pTexture);

    glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(texture.levels() - 1));

    if (gli::gl::EXTERNAL_RED != format.External) // skip for proper greyscale-alpha font textures
    {
#if defined(__ANDROID__)
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_R, format.Swizzles[gli::SWIZZLE_RED]);
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_G, format.Swizzles[gli::SWIZZLE_GREEN]);
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_B, format.Swizzles[gli::SWIZZLE_BLUE]);
        glTexParameteri(target, GL_TEXTURE_SWIZZLE_A, format.Swizzles[gli::SWIZZLE_ALPHA]);
#else
        glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, &format.Swizzles[gli::SWIZZLE_RED]);
#endif
    }

    glm::tvec3<GLsizei> const tex_extent(texture.extent());

    GLenum err;
    while (glGetError() != GL_NO_ERROR); // clear any previous errors

    bool storage_allocated = false;
    switch (texture.target())
    {
    case gli::TARGET_2D:
    case gli::TARGET_CUBE:
        glTexStorage2D(target, static_cast<GLint>(texture.levels()), format.Internal,
                       tex_extent.x, tex_extent.y);
        err = glGetError();
        if (err == GL_NO_ERROR)
        {
            storage_allocated = true;
        }
        else
        {
            Msg("! OpenGL: 0x%x: glTexStorage2D failed: '%s' (internal=0x%x w=%d h=%d levels=%d), fallback to glTexImage2D",
                err, fn, format.Internal, tex_extent.x, tex_extent.y, static_cast<int>(texture.levels()));
        }
        break;
    case gli::TARGET_3D:
    case gli::TARGET_CUBE_ARRAY:
        glTexStorage3D(target, static_cast<GLint>(texture.levels()), format.Internal,
                       tex_extent.x, tex_extent.y, tex_extent.z);
        err = glGetError();
        if (err == GL_NO_ERROR)
        {
            storage_allocated = true;
        }
        else
        {
            Msg("! OpenGL: 0x%x: glTexStorage3D failed: '%s' (internal=0x%x w=%d h=%d levels=%d), fallback to glTexImage3D",
                err, fn, format.Internal, tex_extent.x, tex_extent.y, static_cast<int>(texture.levels()));
        }
        break;
    default:
        NODEFAULT;
        break;
    }

    for (size_t layer = 0; layer < texture.layers(); ++layer)
    {
        for (size_t face = 0; face < texture.faces(); ++face)
        {
            for (size_t level = 0; level < texture.levels(); ++level)
            {
                glm::tvec3<GLsizei> const tex_level_extent(texture.extent(level));
                GLenum sub_target = gli::is_target_cube(texture.target())
                         ? static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face)
                         : target;

                switch (texture.target())
                {
                case gli::TARGET_2D:
                case gli::TARGET_CUBE:
                {
                    if (gli::is_compressed(texture.format()))
                    {
                        if (storage_allocated)
                        {
                            glCompressedTexSubImage2D(sub_target, static_cast<GLint>(level),
                                        0, 0, tex_level_extent.x, tex_level_extent.y,
                                        format.Internal, static_cast<GLsizei>(texture.size(level)),
                                        texture.data(layer, face, level));
                        }
                        else
                        {
                            glCompressedTexImage2D(sub_target, static_cast<GLint>(level),
                                        format.Internal,
                                        tex_level_extent.x, tex_level_extent.y, 0,
                                        static_cast<GLsizei>(texture.size(level)),
                                        texture.data(layer, face, level));
                        }
                        err = glGetError();
                        if (err != GL_NO_ERROR)
                        {
                            VERIFY(err == GL_NO_ERROR);
                            Msg("! OpenGL: 0x%x: Invalid 2D compressed subtexture: '%s'", err, fn);
                        }
                    }
                    else
                    {
                        if (storage_allocated)
                        {
                            glTexSubImage2D(sub_target, static_cast<GLint>(level),
                                        0, 0, tex_level_extent.x, tex_level_extent.y,
                                        format.External, format.Type,
                                        texture.data(layer, face, level));
                        }
                        else
                        {
#if defined(__ANDROID__)
                            GLint const internal_fmt = (format.External == GL_BGRA_EXT) ? GL_BGRA_EXT : format.External;
#else
                            GLint const internal_fmt = format.Internal;
#endif
                            glTexImage2D(sub_target, static_cast<GLint>(level),
                                        internal_fmt,
                                        tex_level_extent.x, tex_level_extent.y, 0,
                                        format.External, format.Type,
                                        texture.data(layer, face, level));
                        }
                        err = glGetError();
                        if (err != GL_NO_ERROR)
                        {
                            VERIFY(err == GL_NO_ERROR);
                            Msg("! OpenGL: 0x%x: Invalid 2D subtexture: '%s'", err, fn);
                        }

                    }
                    break;
                }
                case gli::TARGET_3D:
                case gli::TARGET_CUBE_ARRAY:
                {
                    if (gli::is_compressed(texture.format()))
                    {
                        if (storage_allocated)
                        {
                            glCompressedTexSubImage3D(target, static_cast<GLint>(level),
                                        0, 0, 0, tex_level_extent.x, tex_level_extent.y, tex_level_extent.z,
                                        format.Internal, static_cast<GLsizei>(texture.size(level)),
                                        texture.data(layer, face, level));
                        }
                        else
                        {
                            glCompressedTexImage3D(target, static_cast<GLint>(level),
                                        format.Internal,
                                        tex_level_extent.x, tex_level_extent.y, tex_level_extent.z, 0,
                                        static_cast<GLsizei>(texture.size(level)),
                                        texture.data(layer, face, level));
                        }
                        err = glGetError();
                        if (err != GL_NO_ERROR)
                        {
                            VERIFY(err == GL_NO_ERROR);
                            Msg("! OpenGL: 0x%x: Invalid compressed 3D subtexture: '%s'", err, fn);
                        }
                    }
                    else
                    {
                        if (storage_allocated)
                        {
                            glTexSubImage3D(target, static_cast<GLint>(level),
                                        0, 0, 0, tex_level_extent.x, tex_level_extent.y, tex_level_extent.z,
                                        format.External, format.Type,
                                        texture.data(layer, face, level));
                        }
                        else
                        {
#if defined(__ANDROID__)
                            GLint const internal_fmt = (format.External == GL_BGRA_EXT) ? GL_BGRA_EXT : format.External;
#else
                            GLint const internal_fmt = format.Internal;
#endif
                            glTexImage3D(target, static_cast<GLint>(level),
                                        internal_fmt,
                                        tex_level_extent.x, tex_level_extent.y, tex_level_extent.z, 0,
                                        format.External, format.Type,
                                        texture.data(layer, face, level));
                        }
                        err = glGetError();
                        if (err != GL_NO_ERROR)
                        {
                            VERIFY(err == GL_NO_ERROR);
                            Msg("! OpenGL: 0x%x: Invalid 3D subtexture: '%s'", err, fn);
                        }
                    }
                    break;
                }
                default:
                    NODEFAULT;
                    break;
                }
            }
        }
    }

    FS.r_close(S);

    ret_desc = target;
    ret_msize = calc_texture_size(img_loaded_lod, mip_cnt, img_size);
    return pTexture;
}
} // namespace xray::render::RENDER_NAMESPACE

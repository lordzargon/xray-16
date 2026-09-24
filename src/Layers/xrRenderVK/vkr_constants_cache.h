#pragma once

namespace xray::render::RENDER_NAMESPACE
{
class ECORE_API R_constants
{
public:
    ICF void set(R_constant* C, const Fmatrix& A) {}
    ICF void set(R_constant* C, const Fvector4& A) {}
    ICF void set(R_constant* C, float x, float y, float z, float w) {}
    ICF void set(R_constant* C, float A) {}
    ICF void set(R_constant* C, int A) {}

    ICF void seta(R_constant* C, u32 e, const Fmatrix& A) {}
    ICF void seta(R_constant* C, u32 e, const Fvector4& A) {}
    ICF void seta(R_constant* C, u32 e, float x, float y, float z, float w) {}

    ICF void flush() {}
};
} // namespace xray::render::RENDER_NAMESPACE

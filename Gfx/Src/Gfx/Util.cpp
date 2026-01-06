#include "Gfx/Util.h"
#include "Core/Log.h"
#include "Core/Math.h"

namespace
{
    struct FormatMapping
    {
        st::rhi::Format nvrhiFormat;
        DXGI_FORMAT dxgiFormat;
        uint32_t bitsPerPixel;
    };

    const FormatMapping g_FormatMappings[] = {
        { st::rhi::Format::UNKNOWN,              DXGI_FORMAT_UNKNOWN,                0 },
        { st::rhi::Format::R8_UINT,              DXGI_FORMAT_R8_UINT,                8 },
        { st::rhi::Format::R8_SINT,              DXGI_FORMAT_R8_SINT,                8 },
        { st::rhi::Format::R8_UNORM,             DXGI_FORMAT_R8_UNORM,               8 },
        { st::rhi::Format::R8_SNORM,             DXGI_FORMAT_R8_SNORM,               8 },
        { st::rhi::Format::RG8_UINT,             DXGI_FORMAT_R8G8_UINT,              16 },
        { st::rhi::Format::RG8_SINT,             DXGI_FORMAT_R8G8_SINT,              16 },
        { st::rhi::Format::RG8_UNORM,            DXGI_FORMAT_R8G8_UNORM,             16 },
        { st::rhi::Format::RG8_SNORM,            DXGI_FORMAT_R8G8_SNORM,             16 },
        { st::rhi::Format::R16_UINT,             DXGI_FORMAT_R16_UINT,               16 },
        { st::rhi::Format::R16_SINT,             DXGI_FORMAT_R16_SINT,               16 },
        { st::rhi::Format::R16_UNORM,            DXGI_FORMAT_R16_UNORM,              16 },
        { st::rhi::Format::R16_SNORM,            DXGI_FORMAT_R16_SNORM,              16 },
        { st::rhi::Format::R16_FLOAT,            DXGI_FORMAT_R16_FLOAT,              16 },
        { st::rhi::Format::BGRA4_UNORM,          DXGI_FORMAT_B4G4R4A4_UNORM,         16 },
        { st::rhi::Format::B5G6R5_UNORM,         DXGI_FORMAT_B5G6R5_UNORM,           16 },
        { st::rhi::Format::B5G5R5A1_UNORM,       DXGI_FORMAT_B5G5R5A1_UNORM,         16 },
        { st::rhi::Format::RGBA8_UINT,           DXGI_FORMAT_R8G8B8A8_UINT,          32 },
        { st::rhi::Format::RGBA8_SINT,           DXGI_FORMAT_R8G8B8A8_SINT,          32 },
        { st::rhi::Format::RGBA8_UNORM,          DXGI_FORMAT_R8G8B8A8_UNORM,         32 },
        { st::rhi::Format::RGBA8_SNORM,          DXGI_FORMAT_R8G8B8A8_SNORM,         32 },
        { st::rhi::Format::BGRA8_UNORM,          DXGI_FORMAT_B8G8R8A8_UNORM,         32 },
        { st::rhi::Format::SRGBA8_UNORM,         DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,    32 },
        { st::rhi::Format::SBGRA8_UNORM,         DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,    32 },
        { st::rhi::Format::R10G10B10A2_UNORM,    DXGI_FORMAT_R10G10B10A2_UNORM,      32 },
        { st::rhi::Format::R11G11B10_FLOAT,      DXGI_FORMAT_R11G11B10_FLOAT,        32 },
        { st::rhi::Format::RG16_UINT,            DXGI_FORMAT_R16G16_UINT,            32 },
        { st::rhi::Format::RG16_SINT,            DXGI_FORMAT_R16G16_SINT,            32 },
        { st::rhi::Format::RG16_UNORM,           DXGI_FORMAT_R16G16_UNORM,           32 },
        { st::rhi::Format::RG16_SNORM,           DXGI_FORMAT_R16G16_SNORM,           32 },
        { st::rhi::Format::RG16_FLOAT,           DXGI_FORMAT_R16G16_FLOAT,           32 },
        { st::rhi::Format::R32_UINT,             DXGI_FORMAT_R32_UINT,               32 },
        { st::rhi::Format::R32_SINT,             DXGI_FORMAT_R32_SINT,               32 },
        { st::rhi::Format::R32_FLOAT,            DXGI_FORMAT_R32_FLOAT,              32 },
        { st::rhi::Format::RGBA16_UINT,          DXGI_FORMAT_R16G16B16A16_UINT,      64 },
        { st::rhi::Format::RGBA16_SINT,          DXGI_FORMAT_R16G16B16A16_SINT,      64 },
        { st::rhi::Format::RGBA16_FLOAT,         DXGI_FORMAT_R16G16B16A16_FLOAT,     64 },
        { st::rhi::Format::RGBA16_UNORM,         DXGI_FORMAT_R16G16B16A16_UNORM,     64 },
        { st::rhi::Format::RGBA16_SNORM,         DXGI_FORMAT_R16G16B16A16_SNORM,     64 },
        { st::rhi::Format::RG32_UINT,            DXGI_FORMAT_R32G32_UINT,            64 },
        { st::rhi::Format::RG32_SINT,            DXGI_FORMAT_R32G32_SINT,            64 },
        { st::rhi::Format::RG32_FLOAT,           DXGI_FORMAT_R32G32_FLOAT,           64 },
        { st::rhi::Format::RGB32_UINT,           DXGI_FORMAT_R32G32B32_UINT,         96 },
        { st::rhi::Format::RGB32_SINT,           DXGI_FORMAT_R32G32B32_SINT,         96 },
        { st::rhi::Format::RGB32_FLOAT,          DXGI_FORMAT_R32G32B32_FLOAT,        96 },
        { st::rhi::Format::RGBA32_UINT,          DXGI_FORMAT_R32G32B32A32_UINT,      128 },
        { st::rhi::Format::RGBA32_SINT,          DXGI_FORMAT_R32G32B32A32_SINT,      128 },
        { st::rhi::Format::RGBA32_FLOAT,         DXGI_FORMAT_R32G32B32A32_FLOAT,     128 },
        { st::rhi::Format::D16,                  DXGI_FORMAT_R16_UNORM,              16 },
        { st::rhi::Format::D24S8,                DXGI_FORMAT_R24_UNORM_X8_TYPELESS,  32 },
        { st::rhi::Format::X24G8_UINT,           DXGI_FORMAT_X24_TYPELESS_G8_UINT,   32 },
        { st::rhi::Format::D32,                  DXGI_FORMAT_R32_FLOAT,              32 },
        { st::rhi::Format::D32S8,                DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, 64 },
        { st::rhi::Format::X32G8_UINT,           DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,  64 },
        { st::rhi::Format::BC1_UNORM,            DXGI_FORMAT_BC1_UNORM,              4 },
        { st::rhi::Format::BC1_UNORM_SRGB,       DXGI_FORMAT_BC1_UNORM_SRGB,         4 },
        { st::rhi::Format::BC2_UNORM,            DXGI_FORMAT_BC2_UNORM,              8 },
        { st::rhi::Format::BC2_UNORM_SRGB,       DXGI_FORMAT_BC2_UNORM_SRGB,         8 },
        { st::rhi::Format::BC3_UNORM,            DXGI_FORMAT_BC3_UNORM,              8 },
        { st::rhi::Format::BC3_UNORM_SRGB,       DXGI_FORMAT_BC3_UNORM_SRGB,         8 },
        { st::rhi::Format::BC4_UNORM,            DXGI_FORMAT_BC4_UNORM,              4 },
        { st::rhi::Format::BC4_SNORM,            DXGI_FORMAT_BC4_SNORM,              4 },
        { st::rhi::Format::BC5_UNORM,            DXGI_FORMAT_BC5_UNORM,              8 },
        { st::rhi::Format::BC5_SNORM,            DXGI_FORMAT_BC5_SNORM,              8 },
        { st::rhi::Format::BC6H_UFLOAT,          DXGI_FORMAT_BC6H_UF16,              8 },
        { st::rhi::Format::BC6H_SFLOAT,          DXGI_FORMAT_BC6H_SF16,              8 },
        { st::rhi::Format::BC7_UNORM,            DXGI_FORMAT_BC7_UNORM,              8 },
        { st::rhi::Format::BC7_UNORM_SRGB,       DXGI_FORMAT_BC7_UNORM_SRGB,         8 },
    };
} // anonymous namespace

st::rhi::Format st::gfx::GetFormat(DXGI_FORMAT format)
{
    for (const FormatMapping mapping : g_FormatMappings)
    {
        if (mapping.dxgiFormat == format)
        {
            return mapping.nvrhiFormat;
        }
    }

    LOG_ERROR("Format unknown: [{:#x}]", (uint32_t)format);
    return st::rhi::Format::UNKNOWN;
};

uint32_t st::gfx::BitsPerPixel(st::rhi::Format format)
{
    const FormatMapping& mapping = g_FormatMappings[static_cast<uint32_t>(format)];
    assert(mapping.nvrhiFormat == format);

    return mapping.bitsPerPixel;
}

float4x4 st::gfx::BuildPersInvZ(float v_fov, float aspect, float n_near, float z_far)
{
    float yScale = 1.0f / tanf(0.5f * v_fov);
    float xScale = yScale / aspect;
    float n = n_near;
    float f = z_far;

    // Note that we are using GLM that uses column-major order for the arguments of its constructor.
    // That means the first 4 inputs to the constructor are actually the first column of the matrix.

    return float4x4 {
        { xScale,   0.0f,   0.0f,           0.0f },	    // first column
        { 0.0f,     yScale, 0.0f,           0.0f },	    // second column
        { 0.0f,     0.0f,   (n - f) / n,    f / n },	// third column,
        { 0.0f,     0.0f,   -1.0f,          0.0f } 	    // fourth column
    };
}

float4x4 st::gfx::BuildPersInvZInfFar(float v_fov, float aspect, float z_near)
{
    float yScale = 1.0f / tanf(0.5f * v_fov);
    float xScale = yScale / aspect;

    // WARNING!!! rows are columns!
    return float4x4 {
        { xScale,		0.f,		0.f,	0.f },	// first column
        { 0.f,			yScale,		0.f,	0.f },	// second column
        { 0.f,			0.f,		0.f,	-1.f },	// third column
        { 0.f,			0.f,		z_near,	0.f }	// fourth column
    };
}

float4x4 st::gfx::BuildOrthoInvZ(float left, float right, float bottom, float top, float z_near, float z_far)
{
    /*
    float inv_dx = 1.0f / (right - left);
    float inv_dy = 1.0f / (top - bottom);
    float inv_dz = 1.0f / (z_near - z_far); // reverse Z

    return float4x4 {
        {  2.0f * inv_dx,           0.0f,                       0.0f,               0.0f }, // column 0        
        {  0.0f,                    2.0f * inv_dy,              0.0f,               0.0f }, // column 1
        {  0.0f,                    0.0f,                       -inv_dz,            0.0f }, // column 2 (Z)
        { -(right + left) * inv_dx, -(top + bottom) * inv_dy,   z_far * inv_dz, 1.0f }  // column 3 (translation)
    };
*/
/*
    float inv_dx = 1.0f / (right - left);
    float inv_dy = 1.0f / (top - bottom);
    float inv_dz = 1.0f / (z_far - z_near);

    // NOTE: BUILD BY COLUMNS for glm::mat4 (Right-Handed, Z-Reverse)
    return float4x4 {
        { 2.0f * inv_dx,            0.0f,                       0.0f,               0.0f }, // column 0
        { 0.0f,                     2.0f * inv_dy,              0.0f,               0.0f }, // column 1
        { 0.0f,                     0.0f,                       -inv_dz,             0.0f }, // column 2
        { -(right + left) * inv_dx, -(top + bottom) * inv_dy,   z_near * inv_dz,         1.0f }  // column 3
    };
*/

    float inv_dx = 1.0f / (right - left);
    float inv_dy = 1.0f / (top - bottom);
    float inv_dz = 1.0f / (z_far - z_near);

    return float4x4{
        { 2.0f * inv_dx,            0.0f,                       0.0f,               0.0f }, // column 0
        { 0.0f,                     2.0f * inv_dy,              0.0f,               0.0f }, // column 1
        { 0.0f,                     0.0f,                       -inv_dz,            0.0f }, // column 2
        { -(right + left) * inv_dx, -(top + bottom) * inv_dy,   -z_near * inv_dz,   1.0f }  // column 3
    };
}

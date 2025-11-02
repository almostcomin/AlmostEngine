#include "Gfx/Util.h"
#include "Core/Log.h"
#include "Core/Math.h"

namespace
{
    struct FormatMapping
    {
        st::rapi::Format nvrhiFormat;
        DXGI_FORMAT dxgiFormat;
        uint32_t bitsPerPixel;
    };

    const FormatMapping g_FormatMappings[] = {
        { st::rapi::Format::UNKNOWN,              DXGI_FORMAT_UNKNOWN,                0 },
        { st::rapi::Format::R8_UINT,              DXGI_FORMAT_R8_UINT,                8 },
        { st::rapi::Format::R8_SINT,              DXGI_FORMAT_R8_SINT,                8 },
        { st::rapi::Format::R8_UNORM,             DXGI_FORMAT_R8_UNORM,               8 },
        { st::rapi::Format::R8_SNORM,             DXGI_FORMAT_R8_SNORM,               8 },
        { st::rapi::Format::RG8_UINT,             DXGI_FORMAT_R8G8_UINT,              16 },
        { st::rapi::Format::RG8_SINT,             DXGI_FORMAT_R8G8_SINT,              16 },
        { st::rapi::Format::RG8_UNORM,            DXGI_FORMAT_R8G8_UNORM,             16 },
        { st::rapi::Format::RG8_SNORM,            DXGI_FORMAT_R8G8_SNORM,             16 },
        { st::rapi::Format::R16_UINT,             DXGI_FORMAT_R16_UINT,               16 },
        { st::rapi::Format::R16_SINT,             DXGI_FORMAT_R16_SINT,               16 },
        { st::rapi::Format::R16_UNORM,            DXGI_FORMAT_R16_UNORM,              16 },
        { st::rapi::Format::R16_SNORM,            DXGI_FORMAT_R16_SNORM,              16 },
        { st::rapi::Format::R16_FLOAT,            DXGI_FORMAT_R16_FLOAT,              16 },
        { st::rapi::Format::BGRA4_UNORM,          DXGI_FORMAT_B4G4R4A4_UNORM,         16 },
        { st::rapi::Format::B5G6R5_UNORM,         DXGI_FORMAT_B5G6R5_UNORM,           16 },
        { st::rapi::Format::B5G5R5A1_UNORM,       DXGI_FORMAT_B5G5R5A1_UNORM,         16 },
        { st::rapi::Format::RGBA8_UINT,           DXGI_FORMAT_R8G8B8A8_UINT,          32 },
        { st::rapi::Format::RGBA8_SINT,           DXGI_FORMAT_R8G8B8A8_SINT,          32 },
        { st::rapi::Format::RGBA8_UNORM,          DXGI_FORMAT_R8G8B8A8_UNORM,         32 },
        { st::rapi::Format::RGBA8_SNORM,          DXGI_FORMAT_R8G8B8A8_SNORM,         32 },
        { st::rapi::Format::BGRA8_UNORM,          DXGI_FORMAT_B8G8R8A8_UNORM,         32 },
        { st::rapi::Format::SRGBA8_UNORM,         DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,    32 },
        { st::rapi::Format::SBGRA8_UNORM,         DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,    32 },
        { st::rapi::Format::R10G10B10A2_UNORM,    DXGI_FORMAT_R10G10B10A2_UNORM,      32 },
        { st::rapi::Format::R11G11B10_FLOAT,      DXGI_FORMAT_R11G11B10_FLOAT,        32 },
        { st::rapi::Format::RG16_UINT,            DXGI_FORMAT_R16G16_UINT,            32 },
        { st::rapi::Format::RG16_SINT,            DXGI_FORMAT_R16G16_SINT,            32 },
        { st::rapi::Format::RG16_UNORM,           DXGI_FORMAT_R16G16_UNORM,           32 },
        { st::rapi::Format::RG16_SNORM,           DXGI_FORMAT_R16G16_SNORM,           32 },
        { st::rapi::Format::RG16_FLOAT,           DXGI_FORMAT_R16G16_FLOAT,           32 },
        { st::rapi::Format::R32_UINT,             DXGI_FORMAT_R32_UINT,               32 },
        { st::rapi::Format::R32_SINT,             DXGI_FORMAT_R32_SINT,               32 },
        { st::rapi::Format::R32_FLOAT,            DXGI_FORMAT_R32_FLOAT,              32 },
        { st::rapi::Format::RGBA16_UINT,          DXGI_FORMAT_R16G16B16A16_UINT,      64 },
        { st::rapi::Format::RGBA16_SINT,          DXGI_FORMAT_R16G16B16A16_SINT,      64 },
        { st::rapi::Format::RGBA16_FLOAT,         DXGI_FORMAT_R16G16B16A16_FLOAT,     64 },
        { st::rapi::Format::RGBA16_UNORM,         DXGI_FORMAT_R16G16B16A16_UNORM,     64 },
        { st::rapi::Format::RGBA16_SNORM,         DXGI_FORMAT_R16G16B16A16_SNORM,     64 },
        { st::rapi::Format::RG32_UINT,            DXGI_FORMAT_R32G32_UINT,            64 },
        { st::rapi::Format::RG32_SINT,            DXGI_FORMAT_R32G32_SINT,            64 },
        { st::rapi::Format::RG32_FLOAT,           DXGI_FORMAT_R32G32_FLOAT,           64 },
        { st::rapi::Format::RGB32_UINT,           DXGI_FORMAT_R32G32B32_UINT,         96 },
        { st::rapi::Format::RGB32_SINT,           DXGI_FORMAT_R32G32B32_SINT,         96 },
        { st::rapi::Format::RGB32_FLOAT,          DXGI_FORMAT_R32G32B32_FLOAT,        96 },
        { st::rapi::Format::RGBA32_UINT,          DXGI_FORMAT_R32G32B32A32_UINT,      128 },
        { st::rapi::Format::RGBA32_SINT,          DXGI_FORMAT_R32G32B32A32_SINT,      128 },
        { st::rapi::Format::RGBA32_FLOAT,         DXGI_FORMAT_R32G32B32A32_FLOAT,     128 },
        { st::rapi::Format::D16,                  DXGI_FORMAT_R16_UNORM,              16 },
        { st::rapi::Format::D24S8,                DXGI_FORMAT_R24_UNORM_X8_TYPELESS,  32 },
        { st::rapi::Format::X24G8_UINT,           DXGI_FORMAT_X24_TYPELESS_G8_UINT,   32 },
        { st::rapi::Format::D32,                  DXGI_FORMAT_R32_FLOAT,              32 },
        { st::rapi::Format::D32S8,                DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, 64 },
        { st::rapi::Format::X32G8_UINT,           DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,  64 },
        { st::rapi::Format::BC1_UNORM,            DXGI_FORMAT_BC1_UNORM,              4 },
        { st::rapi::Format::BC1_UNORM_SRGB,       DXGI_FORMAT_BC1_UNORM_SRGB,         4 },
        { st::rapi::Format::BC2_UNORM,            DXGI_FORMAT_BC2_UNORM,              8 },
        { st::rapi::Format::BC2_UNORM_SRGB,       DXGI_FORMAT_BC2_UNORM_SRGB,         8 },
        { st::rapi::Format::BC3_UNORM,            DXGI_FORMAT_BC3_UNORM,              8 },
        { st::rapi::Format::BC3_UNORM_SRGB,       DXGI_FORMAT_BC3_UNORM_SRGB,         8 },
        { st::rapi::Format::BC4_UNORM,            DXGI_FORMAT_BC4_UNORM,              4 },
        { st::rapi::Format::BC4_SNORM,            DXGI_FORMAT_BC4_SNORM,              4 },
        { st::rapi::Format::BC5_UNORM,            DXGI_FORMAT_BC5_UNORM,              8 },
        { st::rapi::Format::BC5_SNORM,            DXGI_FORMAT_BC5_SNORM,              8 },
        { st::rapi::Format::BC6H_UFLOAT,          DXGI_FORMAT_BC6H_UF16,              8 },
        { st::rapi::Format::BC6H_SFLOAT,          DXGI_FORMAT_BC6H_SF16,              8 },
        { st::rapi::Format::BC7_UNORM,            DXGI_FORMAT_BC7_UNORM,              8 },
        { st::rapi::Format::BC7_UNORM_SRGB,       DXGI_FORMAT_BC7_UNORM_SRGB,         8 },
    };
} // anonymous namespace

st::rapi::Format st::gfx::GetFormat(DXGI_FORMAT format)
{
    for (const FormatMapping mapping : g_FormatMappings)
    {
        if (mapping.dxgiFormat == format)
        {
            return mapping.nvrhiFormat;
        }
    }

    LOG_ERROR("Format unknown: [{:#x}]", (uint32_t)format);
    return st::rapi::Format::UNKNOWN;
};

uint32_t st::gfx::BitsPerPixel(st::rapi::Format format)
{
    const FormatMapping& mapping = g_FormatMappings[static_cast<uint32_t>(format)];
    assert(mapping.nvrhiFormat == format);

    return mapping.bitsPerPixel;
}

#if 0
nvrhi::VertexAttributeDesc st::gfx::GetVertexAttributeDesc(VertexAttribute attribute, const char* name, uint32_t bufferIndex)
{
    nvrhi::VertexAttributeDesc result = {};
    result.name = name;
    result.bufferIndex = bufferIndex;
    result.arraySize = 1;

    switch (attribute)
    {
    case VertexAttribute::Position:
    case VertexAttribute::PrevPosition:
        result.format = nvrhi::Format::RGB32_FLOAT;
        result.elementStride = sizeof(float3);
        break;
    case VertexAttribute::TexCoord1:
    case VertexAttribute::TexCoord2:
        result.format = nvrhi::Format::RG32_FLOAT;
        result.elementStride = sizeof(float2);
        break;
    case VertexAttribute::Normal:
    case VertexAttribute::Tangent:
        result.format = nvrhi::Format::RGBA8_SNORM;
        result.elementStride = sizeof(uint32_t);
        break;
    case VertexAttribute::Transform:
        result.format = nvrhi::Format::RGBA32_FLOAT;
        result.arraySize = 3;
        result.offset = offsetof(InstanceData, transform);
        result.elementStride = sizeof(InstanceData);
        result.isInstanced = true;
        break;
    case VertexAttribute::PrevTransform:
        result.format = nvrhi::Format::RGBA32_FLOAT;
        result.arraySize = 3;
        result.offset = offsetof(InstanceData, prevTransform);
        result.elementStride = sizeof(InstanceData);
        result.isInstanced = true;
        break;

    default:
        assert(!"unknown attribute");
    }

    return result;
}
#endif
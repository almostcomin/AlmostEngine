#include "Gfx/GfxPCH.h"
#include "Gfx/ImageHeightmapSource.h"
#include "Gfx/TextureLoader.h"
#include "Gfx/EHdrImageLoader.h"
#include "Core/File.h"

bool alm::gfx::ImageHeightmapSource::Load(const std::string& path)
{
    uint2 dims;
    rhi::Format format;
    alm::Blob srcData;
    float cellSize = -1.f;

    if (GetExtensionFromPath(path) == "hdr")
    {
        auto loadResult = LoadEHdr(path);
        if (!loadResult)
        {
            LOG_ERROR(loadResult.error());
            return false;
        }

        const EHdrInfo& info = loadResult->first;
        dims.x = info.Width;
        dims.y = info.Height;
        format = rhi::Format::R32_FLOAT;
        if (info.HasCellSize)
        {
            cellSize = info.CellSize;
        }
        srcData = std::move(loadResult->second);
    }
    else
    {
        alm::fs::File file{ path };
        if (!file.IsOpen())
        {
            LOG_ERROR("File {} not found.", path);
            return false;
        }

        auto readResult = file.Read();
        assert(readResult);
        alm::Blob fileData = std::move(*readResult);

        auto loadImageResult = LoadImageTexture(alm::WeakBlob{ fileData }, false);
        if (!loadImageResult)
        {
            LOG_ERROR("Failed to load image from file {}. It is a valid image file?", path);
            return false;
        }

        const gfx::TextureInfo& info = loadImageResult->first;
        dims.x = info.width;
        dims.y = info.height;
        format = info.format;
        srcData = std::move(loadImageResult->second);
    }
    const size_t pixelCount = dims.x * dims.y;

    // If format is not R32_FLOAT we have to convert
    if (format != rhi::Format::R32_FLOAT)
    {
        m_Data.alloc(pixelCount * sizeof(float));
        m_DataView = std::span<float>{ (float*)m_Data.data(), pixelCount };

        switch (format)
        {
        case rhi::Format::R8_UNORM:
        {
            const uint8_t* src = srcData.data();
            for (uint32_t i = 0; i < pixelCount; ++i)
            {
                m_DataView[i] = src[i];
            }
        } break;

        case rhi::Format::R16_UNORM:
        {
            const uint16_t* src = (uint16_t*)srcData.data();
            for (uint32_t i = 0; i < pixelCount; ++i)
            {
                m_DataView[i] = src[i] / 65535.f;
            }
        } break;

        case rhi::Format::RGBA8_UNORM:
        {
            const uint8_t* src = srcData.data();
            for (uint32_t i = 0; i < pixelCount; ++i)
                m_DataView[i] = src[i * 4] / 255.0f; // R channel
        } break;

        default:
            LOG_ERROR("ImageHeightmapSource: unsupported format {}", (int)format);
            return false;
        }
    }
    else
    {
        m_Data = std::move(srcData);
        m_DataView = std::span<float>{ (float*)m_Data.data(), pixelCount };
    }

    m_Width = dims.x;
    m_Height = dims.y;
    m_CellSize = cellSize;

    ComputeHeightRange();
    //Normalize();

    m_Name = GetFilenameFromPath(path);
    return true;
}

float alm::gfx::ImageHeightmapSource::GetHeight(const uint2& p) const
{
    if (m_DataView.empty())
    {
        LOG_ERROR("ImageHeightmapSource: Data not initialized");
        return 0.f;
    }

    return m_DataView[std::clamp(p.y, 0u, m_Height - 1) * m_Width + std::clamp(p.x, 0u, m_Width - 1)];
}

float alm::gfx::ImageHeightmapSource::Sample(const float2& uv) const
{
    if (m_DataView.empty())
    {
        LOG_ERROR("ImageHeightmapSource: Data not initialized");
        return 0.f;
    }

    const float2 imagePos = uv * float2{ m_Width, m_Height } - 0.5f;
    const auto i2 = int2{ (int)floor(imagePos.x), (int)floor(imagePos.y) };
    const auto f2 = float2{ imagePos.x - i2.x, imagePos.y - i2.y };

    auto resolve = [&](int x, int size) -> int
    {
        switch (m_EdgeMode)
        {
        case EdgeMode::Wrap:
            return ((x % size) + size) % size;
        case EdgeMode::Clamp:
        default:
            return std::clamp(x, 0, size - 1);
        }
    };

    auto fetch = [&](int x, int y) -> float
    {
        return m_DataView[resolve(y, m_Height) * m_Width + resolve(x, m_Width)];
    };

    return fetch(i2.x,     i2.y)     * (1 - f2.x) * (1 - f2.y)
         + fetch(i2.x + 1, i2.y)     *      f2.x  * (1 - f2.y)
         + fetch(i2.x,     i2.y + 1) * (1 - f2.x) *      f2.y
         + fetch(i2.x + 1, i2.y + 1) *      f2.x  *      f2.y;
}

float2 alm::gfx::ImageHeightmapSource::GetNormalizedSize() const
{
    if (m_Data.empty())
    {
        LOG_ERROR("ImageHeightmapSource: Data not initialized");
        return float2{ 0.f, 0.f };
    }

    if (m_Width >= m_Height)
    {
        return float2{ 1.f, (float)m_Height / m_Width };
    }
    else
    {
        return float2{ (float)m_Width / m_Height, 1.f };
    }
}

float2 alm::gfx::ImageHeightmapSource::GetHeightRange() const
{
    return m_HeightRange;
}

void alm::gfx::ImageHeightmapSource::ComputeHeightRange()
{
    m_HeightRange.x = FLT_MAX;
    m_HeightRange.y = -FLT_MAX;
    for (float h : m_DataView)
    {
        m_HeightRange.x = std::min(m_HeightRange.x, h);
        m_HeightRange.y = std::max(m_HeightRange.y, h);
    }
}

void alm::gfx::ImageHeightmapSource::Normalize()
{
    for (float& h : m_DataView)
    {
        h = (h - m_HeightRange.x) / (m_HeightRange.y - m_HeightRange.x);
    }
    m_HeightRange.x = 0.f;
    m_HeightRange.y = 1.f;
}
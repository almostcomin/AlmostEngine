#include "Gfx/GfxPCH.h"
#include "Gfx/ImageHeightmapSource.h"
#include "Gfx/TextureLoader.h"
#include "Core/File.h"

bool alm::gfx::ImageHeightmapSource::Load(const std::string& path)
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

    std::string_view ext = GetExtensionFromPath(path);

    auto loadImageResult = LoadImageTexture(alm::WeakBlob{ fileData }, false);
    if (!loadImageResult)
    {
        LOG_ERROR("Failed to load image from file {}. It is a valid image file?", path);
        return false;
    }

    const TextureInfo& texInfo = loadImageResult->first;
    const alm::Blob& blob = loadImageResult->second;
    const uint32_t pixelCount = texInfo.width * texInfo.height;
    m_Data.resize(pixelCount);

    switch (texInfo.format)
    {
    case rhi::Format::R8_UNORM:
    {
        const uint8_t* src = blob.data();
        for (uint32_t i = 0; i < pixelCount; ++i)
        {
            m_Data[i] = src[i];
        }
    } break;

    case rhi::Format::R16_UNORM:
    {
        const uint16_t* src = (uint16_t*)blob.data();
        for (uint32_t i = 0; i < pixelCount; ++i)
        {
            m_Data[i] = src[i] / 65535.f;
        }
    } break;

    case rhi::Format::R32_FLOAT:
    {
        const float* src = (float*)blob.data();
        std::copy(src, src + pixelCount, m_Data.begin());
    } break;

    case rhi::Format::RGBA8_UNORM:
    {
        const uint8_t* src = blob.data();
        for (uint32_t i = 0; i < pixelCount; ++i)
            m_Data[i] = src[i * 4] / 255.0f; // R channel
    } break;

    default:
        LOG_ERROR("ImageHeightmapSource: unsupported format {}", (int)texInfo.format);
        return false;
    }

    m_Width = texInfo.width;
    m_Height = texInfo.height;

    ComputeHeightRange();

    return true;
}

float alm::gfx::ImageHeightmapSource::Sample(const float2& uv) const
{
    if (m_Data.empty())
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
        return m_Data[resolve(y, m_Height) * m_Width + resolve(x, m_Width)];
    };

    return fetch(i2.x,     i2.y)     * (1 - f2.x) * (1 - f2.y)
         + fetch(i2.x + 1, i2.y)     *      f2.x  * (1 - f2.y)
         + fetch(i2.x,     i2.y + 1) * (1 - f2.x) *      f2.y
         + fetch(i2.x + 1, i2.y + 1) *      f2.x  *      f2.y;
}

float2 alm::gfx::ImageHeightmapSource::GetNormalizedSize() const
{
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
    for (float h : m_Data)
    {
        m_HeightRange.x = std::min(m_HeightRange.x, h);
        m_HeightRange.y = std::max(m_HeightRange.y, h);
    }
}
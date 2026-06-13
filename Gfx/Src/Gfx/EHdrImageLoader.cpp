#include "Gfx/GfxPCH.h"
#include "Gfx/EHdrImageLoader.h"
#include "Core/File.h"
#include "Core/Common.h"

namespace
{
    // Parses an ESRI EHdr (.flt/.hdr) sidecar header.
    // Returns false if the header is malformed or missing required fields.
    struct EHdrInfo
    {
        uint32_t width = 0;
        uint32_t height = 0;
        bool bigEndian = false;     // BYTEORDER: 'I' (Intel/LSB) or 'M' (Motorola/MSB)
        bool hasNoData = false;
        float noDataValue = 0.f;
    };

    bool ParseEHdr(const std::string& hdrPath, EHdrInfo& outInfo)
    {
        alm::fs::File file{ hdrPath };
        if (!file.IsOpen())
        {
            LOG_ERROR("EHdr header {} not found.", hdrPath);
            return false;
        }

        bool hasWidth = false;
        bool hasHeight = false;

        std::fstream& stream = file.GetStream();
        std::string key;
        while (stream >> key)
        {
            // Keys are case-insensitive in the EHdr spec.
            std::transform(key.begin(), key.end(), key.begin(),
                [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

            if (key == "NCOLS")
            {
                stream >> outInfo.width;
                hasWidth = true;
            }
            else if (key == "NROWS")
            {
                stream >> outInfo.height;
                hasHeight = true;
            }
            else if (key == "BYTEORDER")
            {
                std::string value;
                stream >> value;
                outInfo.bigEndian = (!value.empty() && std::toupper(value[0]) == 'M');
            }
            else if (key == "NODATA" || key == "NODATA_VALUE")
            {
                stream >> outInfo.noDataValue;
                outInfo.hasNoData = true;
            }
            else
            {
                // Skip the value of any key we don't care about
                // (NBITS, PIXELTYPE, ULXMAP, CELLSIZE, etc.).
                std::string discard;
                std::getline(stream, discard);
            }
        }

        if (!hasWidth || !hasHeight)
        {
            LOG_ERROR("EHdr header {} missing NCOLS/NROWS.", hdrPath);
            return false;
        }
        return true;
    }

    // Derives the binary path from the header path by swapping the extension.
    // Tries .flt first (EHdr float convention), then .bil as a fallback.
    std::string FindEHdrBinary(const std::string& hdrPath)
    {
        const size_t dot = hdrPath.find_last_of('.');
        const std::string base = (dot == std::string::npos) ? hdrPath : hdrPath.substr(0, dot);

        for (const char* ext : { ".flt", ".bil", ".raw" })
        {
            std::string candidate = base + ext;
            if (alm::fs::File{ candidate }.IsOpen())
                return candidate;
        }
        return {};
    }

    float MaybeSwap(float value, bool bigEndian)
    {
        if (!bigEndian)
            return value;

        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        bits = (bits >> 24) | ((bits >> 8) & 0x0000FF00u) |
            ((bits << 8) & 0x00FF0000u) | (bits << 24);
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

} // anonymous namespace

std::expected<std::pair<alm::gfx::TextureInfo, alm::Blob>, std::string>
alm::gfx::LoadEHdr(const std::string& hdrPath)
{
    EHdrInfo info;
    if (!ParseEHdr(hdrPath, info))
        return std::unexpected(std::format("Error loading EHdr header image [{}]", hdrPath));

    const std::string binPath = FindEHdrBinary(hdrPath);
    if (binPath.empty())
        return std::unexpected(std::format("No binary (.flt/.bil/.raw) found next to header {}.", hdrPath));

    alm::fs::File file{ binPath };
    if (!file.IsOpen())
        return std::unexpected(std::format("File {} not found.", binPath));

    auto readResult = file.Read();
    assert(readResult);
    alm::Blob fileData = std::move(*readResult);

    const uint32_t pixelCount = info.width * info.height;
    const size_t expectedBytes = size_t(pixelCount) * sizeof(float);
    if (fileData.size() < expectedBytes)
    {
        return std::unexpected(std::format("EHdr binary {} too small: {} bytes, expected {}.",
            binPath, fileData.size(), expectedBytes));
    }

    float* heights = reinterpret_cast<float*>(fileData.data());

    // Fix endian
    if (info.bigEndian)
    {
        for (uint32_t i = 0; i < pixelCount; ++i)
        {
            uint32_t bits;
            std::memcpy(&bits, heights + i, sizeof(bits));
            bits = (bits >> 24) | ((bits >> 8) & 0x0000FF00u) |
                ((bits << 8) & 0x00FF0000u) | (bits << 24);
            std::memcpy(heights + i, &bits, sizeof(bits));
        }
    }

    // OpenTopography marks voids with a sentinel (often -9999 or -3.4e38).
    // Replace them with the lowest valid sample so ComputeHeightRange and the
    // terrain mesh don't get wrecked by spurious pits.
    if (info.hasNoData)
    {
        // First pass: find the minimum among valid samples.
        float minValid = std::numeric_limits<float>::max();
        for (uint32_t i = 0; i < pixelCount; ++i)
        {
            if (heights[i] != info.noDataValue && heights[i] > -1e30f)
                minValid = std::min(minValid, heights[i]);
        }
        if (minValid == std::numeric_limits<float>::max())
            minValid = 0.f; // entire tile is NoData; degenerate but safe

        for (uint32_t i = 0; i < pixelCount; ++i)
        {
            heights[i] = (heights[i] == info.noDataValue || heights[i] <= -1e30f) ? minValid : heights[i];
        }
    }

    alm::gfx::TextureInfo texInfo{
        .format = rhi::Format::R32_FLOAT,
        .width = info.width,
        .height = info.height,
        .depth = 1,
        .arraySize = 1,
        .mipLevels = 1,
        .dimension = rhi::TextureDimension::Texture2D,
        .debugName = std::string{ GetFilenameFromPath(hdrPath) }
    };

    return std::pair<alm::gfx::TextureInfo, alm::Blob> { texInfo, std::move(fileData) };
}
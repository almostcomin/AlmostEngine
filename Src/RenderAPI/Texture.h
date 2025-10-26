#pragma once

#include "RenderAPI/Resource.h"
#include "RenderAPI/Format.h"
#include "Core/Math.h"
#include <memory>
#include <string>

namespace st::rapi
{
    using MipLevel = uint32_t;
    using ArraySlice = uint32_t;

    enum class TextureDimension : uint8_t
    {
        Unknown,
        Texture1D,
        Texture1DArray,
        Texture2D,
        Texture2DArray,
        TextureCube,
        TextureCubeArray,
        Texture2DMS,
        Texture2DMSArray,
        Texture3D
    };

    struct TextureDesc
    {
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t arraySize = 1;
        uint32_t mipLevels = 1;
        uint32_t sampleCount = 1;
        uint32_t sampleQuality = 0;
        Format format = Format::UNKNOWN;
        TextureDimension dimension = TextureDimension::Texture2D;
        std::string debugName;

        bool isShaderResource = true; // Note: isShaderResource is initialized to 'true' for backward compatibility
        bool isRenderTarget = false;
        bool isUAV = false;
        bool isTypeless = false;
        bool isShadingRateSurface = false;

        // Indicates that the texture is created with no backing memory,
        // and memory is bound to the texture later using bindTextureMemory.
        // On DX12, the texture resource is created at the time of memory binding.
        bool isVirtual = false;
        bool isTiled = false;

        float4 clearValue;
        bool useClearValue = false;

        constexpr TextureDesc& setWidth(uint32_t value) { width = value; return *this; }
        constexpr TextureDesc& setHeight(uint32_t value) { height = value; return *this; }
        constexpr TextureDesc& setDepth(uint32_t value) { depth = value; return *this; }
        constexpr TextureDesc& setArraySize(uint32_t value) { arraySize = value; return *this; }
        constexpr TextureDesc& setMipLevels(uint32_t value) { mipLevels = value; return *this; }
        constexpr TextureDesc& setSampleCount(uint32_t value) { sampleCount = value; return *this; }
        constexpr TextureDesc& setSampleQuality(uint32_t value) { sampleQuality = value; return *this; }
        constexpr TextureDesc& setFormat(Format value) { format = value; return *this; }
        constexpr TextureDesc& setDimension(TextureDimension value) { dimension = value; return *this; }
        TextureDesc& setDebugName(const std::string& value) { debugName = value; return *this; }
        constexpr TextureDesc& setIsRenderTarget(bool value) { isRenderTarget = value; return *this; }
        constexpr TextureDesc& setIsUAV(bool value) { isUAV = value; return *this; }
        constexpr TextureDesc& setIsTypeless(bool value) { isTypeless = value; return *this; }
        constexpr TextureDesc& setIsVirtual(bool value) { isVirtual = value; return *this; }
        constexpr TextureDesc& setClearValue(const float4& value) { clearValue = value; useClearValue = true; return *this; }
        constexpr TextureDesc& setUseClearValue(bool value) { useClearValue = value; return *this; }
    };

    struct TextureSubresourceSet
    {
        static constexpr MipLevel AllMipLevels = MipLevel(-1);
        static constexpr ArraySlice AllArraySlices = ArraySlice(-1);

        MipLevel baseMipLevel = 0;
        MipLevel numMipLevels = 1;
        ArraySlice baseArraySlice = 0;
        ArraySlice numArraySlices = 1;

        TextureSubresourceSet() = default;

        TextureSubresourceSet(MipLevel _baseMipLevel, MipLevel _numMipLevels, ArraySlice _baseArraySlice, ArraySlice _numArraySlices)
            : baseMipLevel(_baseMipLevel)
            , numMipLevels(_numMipLevels)
            , baseArraySlice(_baseArraySlice)
            , numArraySlices(_numArraySlices)
        {}

        [[nodiscard]] TextureSubresourceSet resolve(const TextureDesc& desc, bool singleMipLevel) const;
        [[nodiscard]] bool isEntireTexture(const TextureDesc& desc) const;

        bool operator ==(const TextureSubresourceSet& other) const
        {
            return baseMipLevel == other.baseMipLevel &&
                numMipLevels == other.numMipLevels &&
                baseArraySlice == other.baseArraySlice &&
                numArraySlices == other.numArraySlices;
        }
        bool operator !=(const TextureSubresourceSet& other) const { return !(*this == other); }

        constexpr TextureSubresourceSet& setBaseMipLevel(MipLevel value) { baseMipLevel = value; return *this; }
        constexpr TextureSubresourceSet& setNumMipLevels(MipLevel value) { numMipLevels = value; return *this; }
        constexpr TextureSubresourceSet& setMipLevels(MipLevel base, MipLevel num) { baseMipLevel = base; numMipLevels = num; return *this; }
        constexpr TextureSubresourceSet& setBaseArraySlice(ArraySlice value) { baseArraySlice = value; return *this; }
        constexpr TextureSubresourceSet& setNumArraySlices(ArraySlice value) { numArraySlices = value; return *this; }
        constexpr TextureSubresourceSet& setArraySlices(ArraySlice base, ArraySlice num) { baseArraySlice = base; numArraySlices = num; return *this; }

        // see the bottom of this file for a specialization of std::hash<TextureSubresourceSet>
    };

	class ITexture : public IResource
	{
    public:

		[[nodiscard]] virtual const TextureDesc& GetDesc() const = 0;
	};

	using TextureHandle = std::shared_ptr<ITexture>;
}
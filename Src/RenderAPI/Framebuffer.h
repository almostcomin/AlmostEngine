#pragma once

#include "RenderAPI/Texture.h"
#include "RenderAPI/Constants.h"
#include "RenderAPI/Viewport.h"
#include "RenderAPI/Resource.h"
#include "Core/static_vector.h"
#include "Core/Memory.h"

namespace st::rapi
{

struct FramebufferAttachment
{
    ITexture* texture = nullptr;
    TextureSubresourceSet subresources = TextureSubresourceSet{ 0, 1, 0, 1 };
    Format format = Format::UNKNOWN;
    bool isReadOnly = false;

    constexpr FramebufferAttachment& SetTexture(ITexture* t) { texture = t; return *this; }
    constexpr FramebufferAttachment& SetSubresources(TextureSubresourceSet value) { subresources = value; return *this; }
    constexpr FramebufferAttachment& SetArraySlice(ArraySlice index) { subresources.baseArraySlice = index; subresources.numArraySlices = 1; return *this; }
    constexpr FramebufferAttachment& SetArraySliceRange(ArraySlice index, ArraySlice count) { subresources.baseArraySlice = index; subresources.numArraySlices = count; return *this; }
    constexpr FramebufferAttachment& SetMipLevel(MipLevel level) { subresources.baseMipLevel = level; subresources.numMipLevels = 1; return *this; }
    constexpr FramebufferAttachment& SetFormat(Format f) { format = f; return *this; }
    constexpr FramebufferAttachment& SetReadOnly(bool ro) { isReadOnly = ro; return *this; }

    bool Valid() const { return texture != nullptr; }
};

struct FramebufferDesc
{
    static_vector<FramebufferAttachment, c_MaxRenderTargets> ColorAttachments;
    FramebufferAttachment DepthAttachment;
    FramebufferAttachment ShadingRateAttachment;
    std::string DebugName = "{null}";

    FramebufferDesc& AddColorAttachment(const FramebufferAttachment& a) { ColorAttachments.push_back(a); return *this; }
    FramebufferDesc& AddColorAttachment(ITexture* texture) { ColorAttachments.push_back(FramebufferAttachment().SetTexture(texture)); return *this; }
    FramebufferDesc& AddColorAttachment(ITexture* texture, TextureSubresourceSet subresources) { ColorAttachments.push_back(FramebufferAttachment().SetTexture(texture).SetSubresources(subresources)); return *this; }
    FramebufferDesc& SetDepthAttachment(const FramebufferAttachment& d) { DepthAttachment = d; return *this; }
    FramebufferDesc& SetDepthAttachment(ITexture* texture) { DepthAttachment = FramebufferAttachment().SetTexture(texture); return *this; }
    FramebufferDesc& SetDepthAttachment(ITexture* texture, TextureSubresourceSet subresources) { DepthAttachment = FramebufferAttachment().SetTexture(texture).SetSubresources(subresources); return *this; }
    FramebufferDesc& SetShadingRateAttachment(const FramebufferAttachment& d) { ShadingRateAttachment = d; return *this; }
    FramebufferDesc& SetShadingRateAttachment(ITexture* texture) { ShadingRateAttachment = FramebufferAttachment().SetTexture(texture); return *this; }
    FramebufferDesc& SetShadingRateAttachment(ITexture* texture, TextureSubresourceSet subresources) { ShadingRateAttachment = FramebufferAttachment().SetTexture(texture).SetSubresources(subresources); return *this; }
    FramebufferDesc& SetDebugName(const std::string& debugName) { DebugName = debugName; return *this; }
};

// Describes the parameters of a framebuffer that can be used to determine if a given framebuffer
// is compatible with a certain graphics or meshlet pipeline object. All fields of FramebufferInfo
// must match between the framebuffer and the pipeline for them to be compatible.
struct FramebufferInfo
{
    uint32_t width = 0;
    uint32_t height = 0;
    static_vector<Format, c_MaxRenderTargets> colorFormats;
    Format depthFormat = Format::UNKNOWN;
    uint32_t sampleCount = 1;
    uint32_t sampleQuality = 0;

    FramebufferInfo() = default;
    FramebufferInfo(const FramebufferDesc& desc);

    [[nodiscard]] Viewport GetViewport(float minZ = 0.f, float maxZ = 1.f) const
    {
        return Viewport(0.f, float(width), 0.f, float(height), minZ, maxZ);
    }

    bool operator==(const FramebufferInfo& other) const
    {
        return FormatsEqual(colorFormats, other.colorFormats)
            && depthFormat == other.depthFormat
            && sampleCount == other.sampleCount
            && sampleQuality == other.sampleQuality;
    }
    bool operator!=(const FramebufferInfo& other) const { return !(*this == other); }

private:
    static bool FormatsEqual(const static_vector<Format, c_MaxRenderTargets>& a, const static_vector<Format, c_MaxRenderTargets>& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); i++) if (a[i] != b[i]) return false;
        return true;
    }
};

class IFramebuffer : public IResource
{
public:

	[[nodiscard]] virtual const FramebufferDesc& GetDesc() const = 0;
	[[nodiscard]] virtual const FramebufferInfo& GetFramebufferInfo() const = 0;
};

using FramebufferHandle = st::weak<IFramebuffer>;

} // namespace st::rapi
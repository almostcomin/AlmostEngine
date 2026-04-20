#pragma once

#include "RHI/Texture.h"
#include "RHI/Viewport.h"
#include "RHI/Common.h"
#include "RHI/TypeForwards.h"

namespace alm::rhi
{

struct FramebufferAttachment
{
    TextureHandle texture = nullptr;
    TextureSubresourceSet subresources = TextureSubresourceSet{ 0, 1, 0, 1 };
    Format format = Format::UNKNOWN;
    bool isReadOnly = false;

    FramebufferAttachment& SetTexture(TextureHandle t) { texture = t; return *this; }
    FramebufferAttachment& SetSubresources(TextureSubresourceSet value) { subresources = value; return *this; }
    FramebufferAttachment& SetArraySlice(ArraySlice index) { subresources.baseArraySlice = index; subresources.numArraySlices = 1; return *this; }
    FramebufferAttachment& SetArraySliceRange(ArraySlice index, ArraySlice count) { subresources.baseArraySlice = index; subresources.numArraySlices = count; return *this; }
    FramebufferAttachment& SetMipLevel(MipLevel level) { subresources.baseMipLevel = level; subresources.numMipLevels = 1; return *this; }
    FramebufferAttachment& SetFormat(Format f) { format = f; return *this; }
    FramebufferAttachment& SetReadOnly(bool ro) { isReadOnly = ro; return *this; }

    bool Valid() const { return texture != nullptr; }
};

struct FramebufferDesc
{
    static_vector<FramebufferAttachment, c_MaxRenderTargets> ColorAttachments;
    FramebufferAttachment DepthAttachment;
    FramebufferAttachment ShadingRateAttachment;
    std::string DebugName = "{null}";

    FramebufferDesc& AddColorAttachment(const FramebufferAttachment& a) { ColorAttachments.push_back(a); return *this; }
    FramebufferDesc& AddColorAttachment(TextureHandle texture) { ColorAttachments.push_back(FramebufferAttachment().SetTexture(texture)); return *this; }
    FramebufferDesc& AddColorAttachment(TextureHandle texture, TextureSubresourceSet subresources) { ColorAttachments.push_back(FramebufferAttachment().SetTexture(texture).SetSubresources(subresources)); return *this; }
    FramebufferDesc& SetDepthAttachment(const FramebufferAttachment& d) { DepthAttachment = d; return *this; }
    FramebufferDesc& SetDepthAttachment(TextureHandle texture) { DepthAttachment = FramebufferAttachment().SetTexture(texture); return *this; }
    FramebufferDesc& SetDepthAttachment(TextureHandle texture, TextureSubresourceSet subresources) { DepthAttachment = FramebufferAttachment().SetTexture(texture).SetSubresources(subresources); return *this; }
    FramebufferDesc& SetShadingRateAttachment(const FramebufferAttachment& d) { ShadingRateAttachment = d; return *this; }
    FramebufferDesc& SetShadingRateAttachment(TextureHandle texture) { ShadingRateAttachment = FramebufferAttachment().SetTexture(texture); return *this; }
    FramebufferDesc& SetShadingRateAttachment(TextureHandle texture, TextureSubresourceSet subresources) { ShadingRateAttachment = FramebufferAttachment().SetTexture(texture).SetSubresources(subresources); return *this; }
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

    Viewport GetViewport(float minZ = 0.f, float maxZ = 1.f) const
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

	virtual const FramebufferDesc& GetDesc() const = 0;
	virtual const FramebufferInfo& GetFramebufferInfo() const = 0;

    virtual TextureHandle GetBackBuffer(uint32_t idx) = 0;
    virtual TextureHandle GetDepthStencil() = 0;

    virtual TextureColorTargetView GetColorTargetView(uint32_t idx) const = 0;

protected:

    IFramebuffer(Device* device, const std::string& debugName) : IResource{ device, debugName } {};
};

} // namespace st::rhi
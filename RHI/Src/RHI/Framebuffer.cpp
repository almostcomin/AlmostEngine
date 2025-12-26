#include "RHI/FrameBuffer.h"

st::rhi::FramebufferInfo::FramebufferInfo(const FramebufferDesc& desc)
{
    for (size_t i = 0; i < desc.ColorAttachments.size(); i++)
    {
        const FramebufferAttachment& attachment = desc.ColorAttachments[i];
        colorFormats.push_back(attachment.format == Format::UNKNOWN && attachment.texture ? attachment.texture->GetDesc().format : attachment.format);
    }

    if (desc.DepthAttachment.Valid())
    {
        const TextureDesc& textureDesc = desc.DepthAttachment.texture->GetDesc();
        width = std::max(textureDesc.width >> desc.DepthAttachment.subresources.baseMipLevel, 1u);
        height = std::max(textureDesc.height >> desc.DepthAttachment.subresources.baseMipLevel, 1u);
        depthFormat = textureDesc.format;
        sampleCount = textureDesc.sampleCount;
        sampleQuality = textureDesc.sampleQuality;
    }
    else if (!desc.ColorAttachments.empty() && desc.ColorAttachments[0].Valid())
    {
        const TextureDesc& textureDesc = desc.ColorAttachments[0].texture->GetDesc();
        width = std::max(textureDesc.width >> desc.ColorAttachments[0].subresources.baseMipLevel, 1u);
        height = std::max(textureDesc.height >> desc.ColorAttachments[0].subresources.baseMipLevel, 1u);
        sampleCount = textureDesc.sampleCount;
        sampleQuality = textureDesc.sampleQuality;
    }
}
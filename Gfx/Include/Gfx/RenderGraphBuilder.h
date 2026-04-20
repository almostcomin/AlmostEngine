#pragma once

#include "RHI/Format.h"
#include "RHI/Buffer.h"
#include "RHI/ResourceState.h"
#include "Gfx/RenderGraph.h"

namespace alm::gfx
{
	class RenderStage;

	class RenderGraphBuilder
	{
	public:

		RenderGraphBuilder(RenderGraph* renderGraph, RenderStage* renderStage);

		RGTextureHandle CreateColorTarget(const std::string& id, int width, int height, int arraySize, rhi::Format format);
		RGTextureHandle CreateDepthTarget(const std::string& id, int width, int height, int arraySize, rhi::Format format);
		RGTextureHandle CreateTexture(const std::string& id, RenderGraph::TextureResourceType type, int width, int height, int arraySize,
			rhi::Format format, bool needsUAV);

		RGBufferHandle CreateBuffer(const std::string& id, const rhi::BufferDesc& desc);

		RGTextureHandle GetTextureHandle(const std::string& id);
		RGBufferHandle GetBufferHandle(const std::string& id);

		RGFramebufferHandle RequestFramebuffer(const std::vector<RGTextureHandle>& colorTargets, RGTextureHandle depthTarget = {});

		void AddTextureDependency(RGTextureHandle textureHandle, RenderGraph::AccessMode accessMode, rhi::ResourceState inputState, rhi::ResourceState outputState);
		void AddBufferDependency(RGBufferHandle bufferHandle, RenderGraph::AccessMode accessMode, rhi::ResourceState inputState, rhi::ResourceState outputState);
		void AddRenderTargetWriteDependency();

		const std::vector<RenderGraph::TextureDependency>& GetTextureReadDependencies() const { return m_TextureReadDependencies; }
		const std::vector<RenderGraph::TextureDependency>& GetTextureWriteDependencies() const { return m_TextureWriteDependencies; }

		const std::vector<RenderGraph::BufferDependency>& GetBufferReadDependencies() const { return m_BufferReadDependencies; }
		const std::vector<RenderGraph::BufferDependency>& GetBufferWriteDependencies() const { return m_BufferWriteDependencies; }

	private:

		RenderGraph* m_RenderGraph;
		RenderStage* m_RenderStage;

		std::vector<RGTextureHandle> m_CreatedTextures;
		std::vector<RGBufferHandle> m_CreatedBuffers;

		std::vector<RenderGraph::TextureDependency> m_TextureReadDependencies;
		std::vector<RenderGraph::TextureDependency> m_TextureWriteDependencies;

		std::vector<RenderGraph::BufferDependency> m_BufferReadDependencies;
		std::vector<RenderGraph::BufferDependency> m_BufferWriteDependencies;

		bool m_WritesToRenderTarget;
	};

} // namespace st::gfx
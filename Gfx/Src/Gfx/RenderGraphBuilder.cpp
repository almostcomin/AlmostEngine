#include "Gfx/RenderGraphBuilder.h"

st::gfx::RenderGraphBuilder::RenderGraphBuilder(st::gfx::RenderGraph* renderGraph, st::gfx::RenderStage* renderStage) :
	m_RenderGraph{ renderGraph }, m_RenderStage{ renderStage }, m_WritesToRenderTarget{ false }
{}

st::gfx::RGTextureHandle st::gfx::RenderGraphBuilder::CreateColorTarget(const std::string& id, int width, int height, int arraySize, rhi::Format format)
{
	auto handle = m_RenderGraph->CreateTexture(m_RenderStage, id, RenderGraph::TextureResourceType::RenderTarget, width, height, arraySize, format, false);
	m_CreatedTextures.push_back(handle);
	return handle;
}

st::gfx::RGTextureHandle st::gfx::RenderGraphBuilder::CreateDepthTarget(const std::string& id, int width, int height, int arraySize, rhi::Format format)
{
	auto handle = m_RenderGraph->CreateTexture(m_RenderStage, id, RenderGraph::TextureResourceType::DepthStencil, width, height, arraySize, format, false);
	m_CreatedTextures.push_back(handle);
	return handle;
}

st::gfx::RGTextureHandle st::gfx::RenderGraphBuilder::CreateTexture(const std::string& id, RenderGraph::TextureResourceType type, int width, int height,
																	int arraySize, rhi::Format format, bool needsUAV)
{
	auto handle = m_RenderGraph->CreateTexture(m_RenderStage, id, type, width, height, arraySize, format, needsUAV);
	m_CreatedTextures.push_back(handle);
	return handle;
}

st::gfx::RGBufferHandle st::gfx::RenderGraphBuilder::CreateBuffer(const std::string& id, const rhi::BufferDesc& desc)
{
	auto handle = m_RenderGraph->CreateBuffer(m_RenderStage, id, desc);
	m_CreatedBuffers.push_back(handle);
	return handle;
}

st::gfx::RGTextureHandle st::gfx::RenderGraphBuilder::GetTextureHandle(const std::string& id)
{
	return m_RenderGraph->GetTextureHandle(id);
}

st::gfx::RGBufferHandle st::gfx::RenderGraphBuilder::GetBufferHandle(const std::string& id)
{
	return m_RenderGraph->GetBufferHandle(id);
}

void st::gfx::RenderGraphBuilder::AddTextureDependency(RGTextureHandle textureHandle, RenderGraph::AccessMode accessMode,
													   rhi::ResourceState inputState, rhi::ResourceState outputState)
{
	switch (accessMode)
	{
	case RenderGraph::AccessMode::Read:
		m_TextureReadDependencies.push_back({ textureHandle, inputState, outputState });
		break;

	case RenderGraph::AccessMode::Write:
		m_TextureWriteDependencies.push_back({ textureHandle, inputState, outputState });
		break;

	default:
		assert(0);
	}
}

void st::gfx::RenderGraphBuilder::AddBufferDependency(RGBufferHandle bufferHandle, RenderGraph::AccessMode accessMode,
													  rhi::ResourceState inputState, rhi::ResourceState outputState)
{
	switch (accessMode)
	{
	case RenderGraph::AccessMode::Read:
		m_BufferReadDependencies.push_back({ bufferHandle, inputState, outputState });
		break;

	case RenderGraph::AccessMode::Write:
		m_BufferWriteDependencies.push_back({ bufferHandle, inputState, outputState });
		break;

	default:
		assert(0);
	}
}

void st::gfx::RenderGraphBuilder::AddRenderTargetWriteDependency()
{
	m_WritesToRenderTarget = true;
}
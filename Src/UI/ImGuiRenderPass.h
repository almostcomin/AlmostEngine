#pragma once

#include "Graphics/RenderPass.h"
#include <nvrhi/nvrhi.h>
#include <imgui/imgui.h>
#include <unordered_map>

struct SDL_Window;

namespace st::gfx
{
	class DeviceManager;
	class ShaderFactory;
}

namespace st::ui
{

class ImGuiRenderPass : public st::gfx::RenderPass
{
public:

	virtual void BuildUI() = 0;

	bool Render(nvrhi::IFramebuffer* frameBuffer) override;

	void OnBackbufferResize(const glm::ivec2& size) override;

protected:

	bool Init(SDL_Window* window, st::gfx::DeviceManager* deviceManager, st::gfx::ShaderFactory* shaderFactory);

private:

	bool UpdateFontTexture();
	bool UpdateGeometry();

	bool ReallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, const bool indexBuffer);
	nvrhi::GraphicsPipelineHandle GetPSO(nvrhi::IFramebuffer* frameBuffer);
	nvrhi::IBindingSet* GetBindingSet(nvrhi::ITexture* texture);

protected:

	void BeginFullScreenWindow();
	void EndFullScreenWindow();
	void DrawCenteredText(const char* text);

private:

	nvrhi::ShaderHandle m_VS;
	nvrhi::ShaderHandle m_PS;

	nvrhi::InputLayoutHandle m_ShaderAttribLayout;
	nvrhi::BindingLayoutHandle m_BindingLayout;
	nvrhi::GraphicsPipelineDesc m_BasePSODesc;

	nvrhi::TextureHandle m_FontTexture;
	nvrhi::SamplerHandle m_FontSampler;

	nvrhi::BufferHandle m_VertexBuffer;
	nvrhi::BufferHandle m_IndexBuffer;
	std::vector<ImDrawVert> m_VertexData;
	std::vector<ImDrawIdx> m_IndexData;

	nvrhi::GraphicsPipelineHandle m_PSO;
	nvrhi::CommandListHandle m_CommandList;

	std::unordered_map<nvrhi::ITexture*, nvrhi::BindingSetHandle> m_BindingsCache;

	st::gfx::DeviceManager* m_DeviceManager;
};

}
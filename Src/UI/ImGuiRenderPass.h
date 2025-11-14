#pragma once

#include "Gfx/RenderPass.h"
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

enum class MouseButton
{
	LEFT,
	RIGHT,
	MIDDLE
};

enum class KeyAction
{
	PRESS,
	RELEASE,
	REPEAT
};

class ImGuiRenderPass : public st::gfx::RenderPass
{
public:

	virtual void BuildUI() = 0;

	bool Render(st::rapi::IFramebuffer* frameBuffer) override;

	void OnBackbufferResize(const glm::ivec2& size) override;

	bool OnMouseMove(float posX, float posY);
	bool OnMouseButtonUpdate(MouseButton button, KeyAction action);

protected:

private:

	bool Init();

	bool UpdateFontTexture();
	bool UpdateGeometry();

	bool ReallocateBuffer(rapi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, const bool indexBuffer);
	nvrhi::GraphicsPipelineHandle GetPSO(nvrhi::IFramebuffer* frameBuffer);
	nvrhi::IBindingSet* GetBindingSet(nvrhi::ITexture* texture);

	void ReconcileInputState();

	void OnAttached() override;
	void OnDetached() override;

protected:

	void BeginFullScreenWindow();
	void EndFullScreenWindow();
	void DrawCenteredText(const char* text);

private:

	std::array<KeyAction, 5> m_CachedMouseButtons;

	nvrhi::ShaderHandle m_VS;
	nvrhi::ShaderHandle m_PS;

	nvrhi::InputLayoutHandle m_ShaderAttribLayout;
	nvrhi::BindingLayoutHandle m_BindingLayout;
	nvrhi::GraphicsPipelineDesc m_BasePSODesc;

	nvrhi::TextureHandle m_FontTexture;
	nvrhi::SamplerHandle m_FontSampler;

	rapi::BufferHandle m_VertexBuffer;
	rapi::BufferHandle m_IndexBuffer;
	std::vector<ImDrawVert> m_VertexData;
	std::vector<ImDrawIdx> m_IndexData;

	nvrhi::GraphicsPipelineHandle m_PSO;
	nvrhi::CommandListHandle m_CommandList;

	std::unordered_map<nvrhi::ITexture*, nvrhi::BindingSetHandle> m_BindingsCache;
};

}
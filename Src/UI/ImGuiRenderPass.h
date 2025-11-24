#pragma once

#include "Gfx/RenderPass.h"
#include "RenderAPI/Buffer.h"
#include "RenderAPI/PipelineState.h"
#include "RenderAPI/CommandList.h"
#include <imgui/imgui.h>
#include <unordered_map>
#include <array>

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
	rapi::GraphicsPipelineStateHandle GetPSO(rapi::IFramebuffer* frameBuffer);

	void ReconcileInputState();

	void OnAttached() override;
	void OnDetached() override;

protected:

	void BeginFullScreenWindow();
	void EndFullScreenWindow();
	void DrawCenteredText(const char* text);
	rapi::BufferHandle& GetCurrentVB();
	rapi::BufferHandle& GetCurrentIB();

private:

	std::array<KeyAction, 5> m_CachedMouseButtons;

	rapi::ShaderHandle m_VS;
	rapi::ShaderHandle m_PS;

	//nvrhi::InputLayoutHandle m_ShaderAttribLayout;
	//nvrhi::BindingLayoutHandle m_BindingLayout;
	rapi::GraphicsPipelineStateDesc m_BasePSODesc;

	rapi::TextureHandle m_FontTexture;
	//rapi::SamplerHandle m_FontSampler;

	rapi::BufferHandle m_VertexBuffer[3];
	rapi::BufferHandle m_IndexBuffer[3];

	rapi::GraphicsPipelineStateHandle m_PSO;
	rapi::CommandListHandle m_CommandList;

	//std::unordered_map<nvrhi::ITexture*, nvrhi::BindingSetHandle> m_BindingsCache;

	rapi::BufferHandle m_TextureUploadBuffer;
};

}
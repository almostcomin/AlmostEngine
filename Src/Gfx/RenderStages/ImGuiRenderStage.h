#pragma once

#include "Gfx/RenderStage.h"
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

namespace st::gfx
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

class ImGuiRenderStage : public RenderStage
{
public:

	virtual void BuildUI() = 0;

	bool Render() override;

	void OnBackbufferResize(const glm::ivec2& size) override;

	bool OnMouseMove(float posX, float posY);
	bool OnMouseButtonUpdate(MouseButton button, KeyAction action);

	const char* GetDebugName() const override { return "ImGuiRenderStage"; }

protected:

private:

	bool Init();
	void Release();

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

	rapi::GraphicsPipelineStateDesc m_BasePSODesc;

	rapi::TextureHandle m_FontTexture;

	rapi::BufferHandle m_VertexBuffer[3];
	rapi::BufferHandle m_IndexBuffer[3];

	rapi::GraphicsPipelineStateHandle m_PSO;
};

}
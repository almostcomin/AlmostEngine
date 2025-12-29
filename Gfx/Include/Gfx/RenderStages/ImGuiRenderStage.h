#pragma once

#include "Gfx/RenderStage.h"
#include "RHI/Buffer.h"
#include "RHI/PipelineState.h"
#include "RHI/CommandList.h"
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

	bool ReallocateBuffer(rhi::BufferOwner& buffer, size_t requiredSize, size_t reallocateSize, const bool indexBuffer);
	rhi::GraphicsPipelineStateHandle GetPSO(rhi::IFramebuffer* frameBuffer);

	void ReconcileInputState();

	void OnAttached() override;
	void OnDetached() override;

protected:

	void BeginFullScreenWindow();
	void EndFullScreenWindow();
	void DrawCenteredText(const char* text);
	rhi::BufferOwner& GetCurrentVB();
	rhi::BufferOwner& GetCurrentIB();

private:

	std::array<KeyAction, 5> m_CachedMouseButtons;

	rhi::ShaderOwner m_VS;
	rhi::ShaderOwner m_PS;

	rhi::GraphicsPipelineStateDesc m_BasePSODesc;

	rhi::TextureOwner m_FontTexture;

	rhi::BufferOwner m_VertexBuffer[3];
	rhi::BufferOwner m_IndexBuffer[3];

	rhi::GraphicsPipelineStateOwner m_PSO;
};

}
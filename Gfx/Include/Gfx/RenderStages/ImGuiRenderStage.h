#pragma once

#include "Gfx/RenderStage.h"
#include "RHI/Buffer.h"
#include "RHI/PipelineState.h"
#include "RHI/CommandList.h"
#include "Gfx/RenderGraphTypes.h"
#include "Gfx/RenderStageFactory.h"
#include <imgui/imgui.h>
#include <unordered_map>
#include <array>

struct SDL_Window;

namespace alm::gfx
{
	class DeviceManager;
	class ShaderFactory;
}

namespace alm::gfx
{

class ImGuiRenderStage : public RenderStage
{
	REGISTER_RENDER_STAGE(ImGuiRenderStage)

public:

	using ImGuiTexFlags = uint32_t;
	static constexpr ImGuiTexFlags ImGuiTexFlags_None = 0;
	static constexpr ImGuiTexFlags ImGuiTexFlags_IgnoreAlpha = 1 << 0;
	static constexpr ImGuiTexFlags ImGuiTexFlags_HideRedChannel = 1 << 1;
	static constexpr ImGuiTexFlags ImGuiTexFlags_HideGreenChannel = 1 << 2;
	static constexpr ImGuiTexFlags ImGuiTexFlags_HideBlueChannel = 1 << 3;
	static constexpr ImGuiTexFlags ImGuiTexFlags_ShowAlphaChannel = 1 << 4;

public:

	struct GeometryBuffers
	{
		rhi::BufferOwner VertexBuffer[3];
		rhi::BufferOwner IndexBuffer[3];
	};

public:

	virtual void BuildUI() {};

	void SetRenderStats(float fps, float cpuTime, float gpuTime);
	void ShowBottomBar(bool b) { m_ShowBottomBar = b; }

	void RenderDrawData(ImDrawData* drawData, GeometryBuffers& geometryBuffers, rhi::ICommandList* commandList);

protected:

	void Setup(RenderGraphBuilder& builder) override;
	void Render(alm::rhi::CommandListHandle commandList) override;
	void OnAttached() override;
	void OnDetached() override;
	void OnBackbufferResize() override;

	void BeginFullScreenWindow();
	void EndFullScreenWindow();

	void DrawCenteredText(const char* text);

	void ShowImage(rhi::TextureHandle tex, const float2& size, const float2& uv0, const float2& uv1, ImGuiTexFlags flags = ImGuiTexFlags_None);

	bool ShowToggleButton(const char* label, bool* v);

	rhi::BufferOwner& GetCurrentVB(GeometryBuffers& geometryBuffers);
	rhi::BufferOwner& GetCurrentIB(GeometryBuffers& geometryBuffers);

private:

	bool Init();
	void Release();

	bool UpdateFontTexture(rhi::ICommandList* commandList);
	bool UpdateGeometry(ImDrawData* drawData, GeometryBuffers& geometryBuffers);

	bool ReallocateBuffer(rhi::BufferOwner& buffer, size_t requiredSize, size_t reallocateSize, const bool indexBuffer);

	void BuildBottomBar();

private:

	struct ImGuiTexture
	{
		rhi::TextureHandle tex;
		ImGuiTexFlags flags;
	};

private:

	RGTextureHandle m_ImGuiTexture;

	rhi::ShaderOwner m_VS;
	rhi::ShaderOwner m_PS;

	rhi::FramebufferOwner m_FB;

	rhi::GraphicsPipelineStateDesc m_BasePSODesc;
	rhi::GraphicsPipelineStateOwner m_PSO;

	rhi::TextureOwner m_FontTexture;
	alm::unique<ImGuiTexture> m_GuiFontTexture;

	GeometryBuffers m_GeometryBuffers;
	std::vector<alm::unique<ImGuiTexture>> m_CurrentTextures;

	float m_FPS = 0.f;
	float m_CPUTime = 0.f;
	float m_GPUTime = 0.f;
	bool m_ShowBottomBar = true;
};

}
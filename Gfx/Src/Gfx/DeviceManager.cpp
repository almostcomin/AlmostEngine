#include "Gfx/GfxPCH.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/Backend/dx12/DeviceManager.h"
#include "Core/Log.h"
#include <SDL3/SDL_video.h>
#include "Gfx/DataUploader.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/TextureCache.h"
#include "Gfx/CommonResources.h"
#include "Gfx/UploadBuffer.h"
#include "Gfx/GpuSceneBuffers.h"
#include "Gfx/RenderView.h"
#include "RHI/Device.h"
#include "RHI/TimerQuery.h"
#include <imgui/imgui.h>

alm::gfx::DeviceManager::DeviceManager() = default;

alm::gfx::DeviceManager::~DeviceManager() = default;

alm::gfx::DeviceManager* alm::gfx::DeviceManager::Create(alm::gfx::GraphicsAPI api)
{
	switch(api)
	{
	case alm::gfx::GraphicsAPI::D3D12:
		return new alm::gfx::dx12::DeviceManager;
		break;
	//default:
		// TODO: assert
	}

	return nullptr;
}

bool alm::gfx::DeviceManager::Init(const DeviceParams& params)
{
	bool ok = InternalInit(params);
	if (ok)
	{
		m_ShaderFactory = std::make_unique<alm::gfx::ShaderFactory>(params.ShadersDebug, m_Device.get());
		m_DataUploader = std::make_unique<alm::gfx::DataUploader>(m_ShaderFactory.get(), m_Device.get());
		m_TextureCache = std::make_unique<alm::gfx::TextureCache>(m_DataUploader.get(), m_Device.get());
		m_CommonResources = std::make_unique<alm::gfx::CommonResources>(m_ShaderFactory.get(), m_Device.get());
		m_UploadBuffer = std::make_unique<alm::gfx::UploadBuffer>(m_FrameIndex, MiB(8), m_Device.get());		
		m_GpuSceneBuffers = std::make_unique<alm::gfx::GpuSceneBuffers>(m_Device.get());

		m_BeginCommandLists.resize(params.SwapChainBufferCount);
		m_EndCommandLists.resize(params.SwapChainBufferCount);
		for (uint32_t i = 0; i < params.SwapChainBufferCount; ++i)
		{
			m_BeginCommandLists[i] = GetDevice()->CreateCommandList({ rhi::QueueType::Graphics }, "DeviceManager Begin");
			m_EndCommandLists[i] = GetDevice()->CreateCommandList({ rhi::QueueType::Graphics }, "DeviceManager End");
		}
	}

	return ok;
}

void alm::gfx::DeviceManager::Shutdown()
{
	m_Device->WaitForIdle();
	m_UploadBuffer->OnFrameCompleted(m_FrameIndex);

	for (auto& fb : m_SwapChainFramebuffers)
	{
		m_Device->ReleaseImmediately(std::move(fb));
	}
	m_SwapChainFramebuffers.clear();

	for (auto& commandList : m_BeginCommandLists)
	{
		m_Device->ReleaseImmediately(std::move(commandList));
	}
	for (auto& commandList : m_EndCommandLists)
	{
		m_Device->ReleaseImmediately(std::move(commandList));
	}

	m_GpuSceneBuffers.reset();
	m_UploadBuffer.reset();
	m_CommonResources.reset();
	m_TextureCache.reset();
	m_DataUploader.reset();
	m_ShaderFactory.reset();

	InternalShutdown();
}

void alm::gfx::DeviceManager::Update()
{
	m_TextureCache->Update();
}

alm::rhi::FramebufferHandle alm::gfx::DeviceManager::GetCurrentFramebuffer()
{
	return m_SwapChainFramebuffers[GetCurrentBackBufferIndex()].get_weak();
}

bool alm::gfx::DeviceManager::UpdateWindowSize()
{
	int width, height;
	if (SDL_GetWindowSize(m_DeviceParams.WindowHandle, (int*)&width, (int*)&height) == false)
	{
		LOG_ERROR("SDL_GetWindowSize failed");
		return false;
	}

	if (width == 0 || height == 0)
	{
		// window is minimized
		m_WindowVisible = false;
		return false;
	}
	m_WindowVisible = true;

	if (int(m_BackBufferWidth) != width || int(m_BackBufferHeight) != height)
	{
		for (auto& fb : m_SwapChainFramebuffers)
		{
			m_Device->ReleaseQueued(std::move(fb));
		}
		m_SwapChainFramebuffers.clear();

		m_BackBufferWidth = width;
		m_BackBufferHeight = height;

		ResizeSwapChain();

		uint32_t backBufferCount = m_DeviceParams.SwapChainBufferCount;
		m_SwapChainFramebuffers.resize(backBufferCount);
		for (uint32_t index = 0; index < backBufferCount; index++)
		{
			m_SwapChainFramebuffers[index] = m_Device->CreateFramebuffer(alm::rhi::FramebufferDesc()
				.AddColorAttachment(GetBackBuffer(index)), std::format("Swapchain FrameBuffer[{}]", index));
		}

		return true;
	}

	return false;
}

alm::gfx::DeviceManager::RenderResult alm::gfx::DeviceManager::Render(float totalSec, float elapsedSec, gfx::MouseState mouseState)
{
	RenderResult result;
	result.cpuIdleMs = 0.f;

	uint32_t beginFrameIdleUSec = 0;
	uint32_t presentIdleUSec = 0;

	if (IsWindowVisible())
	{
		uint64_t completedFrameIdx = BeginFrame(&beginFrameIdleUSec);
		if (completedFrameIdx != 0) // FrameIndex 0 is a fake frame
		{
			m_UploadBuffer->OnFrameCompleted(completedFrameIdx);
		}

		// Begin actions
		{
			auto& commandList = m_BeginCommandLists[GetFrameModuleIndex()];
			commandList->Open();
			
			// Update gpu buffers. TODO: Use a dedicated upload commandlist?
			m_GpuSceneBuffers->UpdateGpuBuffers(commandList.get());

			// Done
			commandList->Close();
			GetDevice()->ExecuteCommandList(commandList.get(), alm::rhi::QueueType::Graphics);
		}

		// Render callback
		for (auto& renderView : m_RenderViews)
		{
			renderView->Render(totalSec, elapsedSec, mouseState);
		}

		// Update and Render of additional ImGui viewports
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		// End actions
		{
			auto& commandList = m_EndCommandLists[GetFrameModuleIndex()];
			commandList->Open();

			// Done
			commandList->Close();
			GetDevice()->ExecuteCommandList(commandList.get(), alm::rhi::QueueType::Graphics);
		}

		// FPS cap
		if(m_DeviceParams.FPSCap > 0)
		{
			const auto frameDuration = std::chrono::microseconds(1000000 / m_DeviceParams.FPSCap);
			static auto nextFrameTime = std::chrono::steady_clock::now();

			auto now = std::chrono::steady_clock::now();
			if (nextFrameTime < now)
			{
				nextFrameTime = now + frameDuration;
			}
			else
			{
				const auto t0 = std::chrono::steady_clock::now();
				std::this_thread::sleep_until(nextFrameTime);
				const auto t1 = std::chrono::steady_clock::now();

				nextFrameTime += frameDuration;
				result.cpuIdleMs += (float)std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 1000.f;
			}
		}

		bool presentOk = Present(&presentIdleUSec);
		assert(presentOk);

		m_UploadBuffer->OnNextFrame(m_FrameIndex);
	}

	m_TextureCache->Update();

	result.cpuIdleMs += (float)(beginFrameIdleUSec + presentIdleUSec) / 1000.f;

	return result;
}

void alm::gfx::DeviceManager::RegisterRenderView(alm::weak<alm::gfx::RenderView> renderView)
{
	m_RenderViews.insert(renderView);
}

void alm::gfx::DeviceManager::UnregisterRenderView(alm::weak<alm::gfx::RenderView> renderView)
{
	m_RenderViews.fast_erase(renderView);
}

uint32_t alm::gfx::DeviceManager::GetFrameModuleIndex() const
{
	return m_FrameIndex % m_SwapChainFramebuffers.size();
}

float alm::gfx::DeviceManager::GetGPUFrameTime()
{
	float result = 0.f;

	for (const auto& it : m_RenderViews)
	{
		result += it->GetGpuFrameTime();
	}

	return result;
}
#include "Gfx/DeviceManager.h"
#include "Gfx/Backend/dx12/DeviceManager.h"
#include "Core/Log.h"
#include <SDL3/SDL_video.h>
#include "Gfx/DataUploader.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/TextureCache.h"
#include "Gfx/CommonResources.h"
#include "Gfx/UploadBuffer.h"
#include "RHI/Device.h"
#include "RHI/TimerQuery.h"

st::gfx::DeviceManager::~DeviceManager()
{}

st::gfx::DeviceManager* st::gfx::DeviceManager::Create(st::gfx::GraphicsAPI api)
{
	switch(api)
	{
	case st::gfx::GraphicsAPI::D3D12:
		return new st::gfx::dx12::DeviceManager;
		break;
	//default:
		// TODO: assert
	}

	return nullptr;
}

bool st::gfx::DeviceManager::Init(const DeviceParams& params)
{
	bool ok = InternalInit(params);
	if (ok)
	{
		m_ShaderFactory = std::make_unique<st::gfx::ShaderFactory>(m_Device.get());
		m_DataUploader = std::make_unique<st::gfx::DataUploader>(m_Device.get());
		m_TextureCache = std::make_unique<st::gfx::TextureCache>(m_Device.get(), m_DataUploader.get());
		m_CommonResources = std::make_unique<st::gfx::CommonResources>(m_ShaderFactory.get(), m_Device.get());
		m_UploadBuffer = std::make_unique<st::gfx::UploadBuffer>(m_FrameIndex, 1024 * 1024, m_Device.get());

		for (uint32_t i = 0; i < QueuedFramesCount; ++i)
		{
			m_FrameTimers[i] = GetDevice()->CreateTimerQuery("FrameTimerQuery");
		}
		m_NextTimerToUse = 0;

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

void st::gfx::DeviceManager::Shutdown()
{
	m_Device->WaitForIdle();

	m_CommonResources.reset();
	m_TextureCache.reset();
	m_DataUploader.reset();
	m_ShaderFactory.reset();

	for (auto& fb : m_SwapChainFramebuffers)
	{
		m_Device->ReleaseQueued(fb);
	}
	m_SwapChainFramebuffers.clear();

	InternalShutdown();
}

void st::gfx::DeviceManager::Update()
{
	m_TextureCache->Update();
}

st::rhi::FramebufferHandle st::gfx::DeviceManager::GetCurrentFramebuffer()
{
	return m_SwapChainFramebuffers[GetCurrentBackBufferIndex()].get_weak();
}

bool st::gfx::DeviceManager::UpdateWindowSize()
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
			m_Device->ReleaseQueued(fb);
		}
		m_SwapChainFramebuffers.clear();

		m_BackBufferWidth = width;
		m_BackBufferHeight = height;

		ResizeSwapChain();

		uint32_t backBufferCount = m_DeviceParams.SwapChainBufferCount;
		m_SwapChainFramebuffers.resize(backBufferCount);
		for (uint32_t index = 0; index < backBufferCount; index++)
		{
			m_SwapChainFramebuffers[index] = m_Device->CreateFramebuffer(st::rhi::FramebufferDesc()
				.AddColorAttachment(GetBackBuffer(index)), std::format("Swapchain FrameBuffer[{}]", index));
		}

		return true;
	}

	return false;
}

void st::gfx::DeviceManager::Render(std::function<void(void)> cb)
{
	// Check window resize
	if (UpdateWindowSize())
	{
		//renderPassManager->OnBackbufferResize(deviceManager->GetWindowDimensions());
	}

	if (IsWindowVisible())
	{
		uint64_t completedFrameIdx = BeginFrame();
		if (completedFrameIdx != 0) // FrameIndex 0 is a fake frame
		{
			m_UploadBuffer->OnFrameCompleted(completedFrameIdx);
		}

		// Begin time query
		{
			m_FrameTimers[m_NextTimerToUse]->Reset();

			auto& commandList = m_BeginCommandLists[GetFrameModuleIndex()];
			
			commandList->Open();
			commandList->BeginTimerQuery(m_FrameTimers[m_NextTimerToUse].get());
			commandList->Close();

			GetDevice()->ExecuteCommandList(commandList.get(), st::rhi::QueueType::Graphics);
		}

		// Render callback
		cb();

		// End time query
		{
			auto& commandList = m_EndCommandLists[GetFrameModuleIndex()];

			commandList->Open();
			commandList->EndTimerQuery(m_FrameTimers[m_NextTimerToUse].get());
			commandList->Close();

			GetDevice()->ExecuteCommandList(commandList.get(), st::rhi::QueueType::Graphics);
			
			m_NextTimerToUse = (m_NextTimerToUse + 1) % QueuedFramesCount;
		}

		bool presentOk = Present();
		assert(presentOk);

		m_UploadBuffer->OnNextFrame(m_FrameIndex);
	}

	m_TextureCache->Update(); 
}

uint32_t st::gfx::DeviceManager::GetFrameModuleIndex() const
{
	return m_FrameIndex % m_SwapChainFramebuffers.size();
}

float st::gfx::DeviceManager::GetGPUFrameTime()
{
	for (int i = m_NextTimerToUse - 1; i >= 0; i--)
	{
		if (m_FrameTimers[i]->Poll())
			return m_FrameTimers[i]->GetQueryTime() * 1000.f;
	}

	for (int i = QueuedFramesCount - 1; i > m_NextTimerToUse; i--)
	{
		if (m_FrameTimers[i]->Poll())
			return m_FrameTimers[i]->GetQueryTime() * 1000.f;
	}
	return -1.0f;
}
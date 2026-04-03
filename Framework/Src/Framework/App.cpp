#include "Framework/FrameworkPCH.h"
#include "Framework/App.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/Scene.h"
#include "Gfx/Camera.h"
#include "Gfx/RenderView.h"
#include "Gfx/RenderStages/CompositeRenderStage.h"
#include "Gfx/RenderStages/DebugRenderStage.h"
#include "Gfx/RenderStages/DepthPrepassRenderStage.h"
#include "Gfx/RenderStages/GBuffersRenderStage.h"
#include "Gfx/RenderStages/DeferredLightingRenderStage.h"
#include "Gfx/RenderStages/ShadowmapRenderStage.h"
#include "Gfx/RenderStages/ToneMappingRenderStage.h"
#include "Gfx/RenderStages/LinearizeDepthRenderStage.h"
#include "Gfx/RenderStages/SSAORenderStage.h"
#include "Gfx/RenderStages/WireframeRenderStage.h"
#include "Gfx/RenderStages/WBOITAccumRenderStage.h"
#include "Gfx/RenderStages/WBOITResolveRenderStage.h"
#include "Gfx/RenderStages/BloomRenderStage.h"
#include "Gfx/RenderStages/SkyRenderStage.h"
#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Gfx/ImGUIViewportsRenderer.h"
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <imgui/imgui_impl_sdl3.h>

namespace
{

alm::App::AppArgs ParseArgs(int argc, char* argv[])
{
	alm::App::AppArgs args;
	for (int i = 1; i < argc - 1; i++)
	{
		if (argv[i][0] == '-')
		{
			args[argv[i] + 1] = argv[i + 1];
			i++;
		}
	}
	return args;
}

bool GetBoolArg(const alm::App::AppArgs& args, const std::string& key, bool defaultValue = false)
{
	auto it = args.find(key);
	if (it == args.end())
		return defaultValue;
	else
		return it->second == "0" ? false : true;
}

} // anonymous namespace

int SDL_main(int argc, char* argv[])
{
	auto app = CreateApp();
	auto args = ParseArgs(argc, argv);
	app->Run(args);

	return 0;
}

alm::App::App(const std::string& windowTitle) : m_WindowTitle{ windowTitle }
{}

alm::App::~App()
{}

void alm::App::Run(const AppArgs& params)
{
	InitInternal(params);
	Initialize(params);

	MainLoop();

	Shutdown();
	ShutdownInternal();
}

bool alm::App::InitInternal(const AppArgs& args)
{
	// Initialize SDL
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		LOG_FATAL("Error initializing SDL.");
		return false; // Initialization failed
	}

	// Create a window
	m_Window = SDL_CreateWindow(m_WindowTitle.c_str(), 1920, 1080, SDL_WINDOW_RESIZABLE);
	if (!m_Window)
	{
		SDL_Quit();
		return false; // Window creation failed
	}

	// Init device manager
	const bool graphicsDebug = GetBoolArg(args, "gd", false);
	const bool vSync = GetBoolArg(args, "vsync", false);

	m_DeviceManager = std::unique_ptr<alm::gfx::DeviceManager>{ alm::gfx::DeviceManager::Create(alm::gfx::GraphicsAPI::D3D12) };
	alm::gfx::DeviceManager::DeviceParams initParams{
		.WindowHandle = m_Window,
		.DebugRuntime = graphicsDebug,
		.GPUValidation = graphicsDebug,
		.VSyncEnabled = vSync,
		.ForceSDR = false
	};
	m_DeviceManager->Init(initParams);

	// Reset window title
	std::string windowTitle = m_WindowTitle + " - " + m_DeviceManager->GetBackEndHWName();
	SDL_SetWindowTitle(m_Window, windowTitle.c_str());

	// Init ImGui
	{
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		//style.ScaleAllSizes(1.0f);			// Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
		//style.FontScaleDpi = 1.0f;			// Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)
		//io.ConfigDpiScaleFonts = true;		// [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
		//io.ConfigDpiScaleViewports = true;	// [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplSDL3_InitForOther(m_Window);
	}

	// Our scene
	m_Scene = alm::make_unique_with_weak<alm::gfx::Scene>(m_DeviceManager.get());

	// Or main camera
	m_MainCamera = std::make_shared<alm::gfx::Camera>();
	int windowWidth, windowHeight;
	SDL_GetWindowSize(m_Window, (int*)&windowWidth, (int*)&windowHeight);
	m_MainCamera->SetAspect((float)windowWidth / windowHeight);
	m_MainCamera->SetPosition({ 0.f, 0.f, 5.f });

	// Create main RenderView
	m_MainRenderView = alm::make_unique_with_weak<alm::gfx::RenderView>(m_DeviceManager.get(), "Main view");
	m_MainRenderView->SetScene(m_Scene.get_weak());
	m_MainRenderView->SetCamera(m_MainCamera);

	InitRenderStages();

	if (m_ImGuiRS)
	{
		alm::gfx::InitImGuiViewportsRenderer(m_ImGuiRS, m_DeviceManager.get());
	}

	return true;
}

void alm::App::ShutdownInternal()
{
	m_MainRenderView.reset();
	m_Scene.reset();
	m_MainCamera.reset();

	ImGui::DestroyPlatformWindows();
	alm::gfx::ReleaseImGuiViewportsRenderer();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	m_DeviceManager->Shutdown();

	SDL_DestroyWindow(m_Window);
	SDL_Quit();
}

void alm::App::InitRenderStages()
{
	std::vector<alm::gfx::RenderStage*> renderStages;

	auto shadowmapRS = gfx::RenderStageFactory::CreateShared<gfx::ShadowmapRenderStage>();
	auto depthPrepassRS = gfx::RenderStageFactory::CreateShared<gfx::DepthPrepassRenderStage>();
	auto linearizeDepthRS = gfx::RenderStageFactory::CreateShared<gfx::LinearizeDepthRenderStage>();
	auto SSAORS = gfx::RenderStageFactory::CreateShared<gfx::SSAORenderStage>();
	auto GBuffersRS = gfx::RenderStageFactory::CreateShared<gfx::GBuffersRenderStage>();
	auto deferredLightingRS = gfx::RenderStageFactory::CreateShared<gfx::DeferredLightingRenderStage>();
	auto simpleSkyRS = gfx::RenderStageFactory::CreateShared<gfx::SkyRenderStage>();
	auto WBOITAccumRS = gfx::RenderStageFactory::CreateShared<alm::gfx::WBOITAccumRenderStage>();
	auto WBOITResolveRS = gfx::RenderStageFactory::CreateShared<alm::gfx::WBOITResolveRenderStage>();
	auto bloomRS = gfx::RenderStageFactory::CreateShared<alm::gfx::BloomRenderStage>();
	auto toneMappingRS = gfx::RenderStageFactory::CreateShared<alm::gfx::ToneMappingRenderStage>();
	auto debugRS = gfx::RenderStageFactory::CreateShared<alm::gfx::DebugRenderStage>();
	auto wireframeRS = gfx::RenderStageFactory::CreateShared<alm::gfx::WireframeRenderStage>();
	auto ImGuiRS = gfx::RenderStageFactory::CreateShared(GetUIRenderStageType());
	auto compositeRS = gfx::RenderStageFactory::CreateShared<alm::gfx::CompositeRenderStage>();
	
	// Add stages to render graph.
	alm::gfx::RenderGraph* renderGraph = m_MainRenderView->GetRenderGraph().get();
	renderGraph->SetRenderStages({
		shadowmapRS,
		depthPrepassRS,
		linearizeDepthRS,
		GBuffersRS,
		SSAORS,
		deferredLightingRS,
		simpleSkyRS,
		WBOITAccumRS,
		WBOITResolveRS,
		bloomRS,
		toneMappingRS,
		debugRS,
		ImGuiRS,
		compositeRS,
		wireframeRS });

	// Define default render mode
	renderGraph->SetRenderMode("Default", { 
		shadowmapRS.get(),
		depthPrepassRS.get(),
		linearizeDepthRS.get(),
		GBuffersRS.get(),
		SSAORS.get(),
		deferredLightingRS.get(),
		simpleSkyRS.get(),
		WBOITAccumRS.get(),
		WBOITResolveRS.get(),
		bloomRS.get(),
		toneMappingRS.get(),
		debugRS.get(),
		ImGuiRS.get(),
		compositeRS.get() });

	// Define wireframe render mode
	renderGraph->SetRenderMode("Wireframe", { 
		depthPrepassRS.get(),
		wireframeRS.get(),
		debugRS.get(),
		ImGuiRS.get(),
		compositeRS.get() });

	m_ImGuiRS = std::dynamic_pointer_cast<alm::gfx::ImGuiRenderStage>(ImGuiRS);
}

void alm::App::MainLoop()
{
	bool requestQuit = false;
	auto lastTime = std::chrono::steady_clock::now();
	auto fpsLastTime = lastTime;
	uint32_t fpsFrameCount = 0;
	bool mouseMiddlePressed = false;

	while (!requestQuit)
	{
		const auto currentTime = std::chrono::steady_clock::now();
		const std::chrono::duration<float> elapsed = currentTime - lastTime;
		const float elapsedSec = elapsed.count();

		// Update timers
		{
			m_CPUTime = elapsedSec * 1000;
			m_GPUTime = (std::max)(m_DeviceManager->GetGPUFrameTime(), 0.f);

			std::chrono::duration<float> fpsElapsed = currentTime - fpsLastTime;
			if (fpsElapsed.count() > 1.f)
			{
				m_FPS = fpsFrameCount / fpsElapsed.count();
				fpsFrameCount = 0;
				fpsLastTime = currentTime;
			}
		}

		// Input
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL3_ProcessEvent(&event);
			bool forwardEvent = true;

			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				m_RequestQuit = true;
				forwardEvent = false;
				break;

			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				if (event.window.windowID == SDL_GetWindowID(m_Window))
				{
					m_RequestQuit = true;
				}
				forwardEvent = false;
				break;
			}

			const ImGuiIO& io = ImGui::GetIO();
			forwardEvent &= !io.WantCaptureMouse;

			if (forwardEvent)
			{
				OnSDLEvent(event);
			}
		}

		if (Update(elapsedSec) == false)
		{
			requestQuit = true;
		}

		// Scene update
		if (m_Scene)
		{
			// Update scene
			m_Scene->Update();
		}

		if (m_DeviceManager->UpdateWindowSize())
		{
			float2 newSize = m_DeviceManager->GetWindowDimensions();
			m_MainCamera->SetAspect(newSize.x / newSize.y);

			m_MainRenderView->OnWindowSizeChanged();
		}

		m_DeviceManager->Render([&]()
		{
			m_MainRenderView->Render(elapsedSec);
		});

		fpsFrameCount++;
		lastTime = currentTime;
		std::this_thread::yield();
	}
}

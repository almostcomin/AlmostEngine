#include "Framework/FrameworkPCH.h"
#include "Framework/App.h"
#include "Framework/UI/FrameworkUI.h"
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
#include "Gfx/RenderStages/SimpleSkyRenderStage.h"
#include "Gfx/RenderStages/ImGuiRenderStage.h"
#include "Gfx/ImGUIViewportsRenderer.h"
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <imgui/imgui_impl_sdl3.h>

namespace
{

alm::fw::App::AppArgs ParseArgs(int argc, char* argv[])
{
	alm::fw::App::AppArgs args;
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

bool GetBoolArg(const alm::fw::App::AppArgs& args, const std::string& key, bool defaultValue = false)
{
	auto it = args.find(key);
	if (it == args.end())
		return defaultValue;
	else
		return it->second == "0" ? false : true;
}

std::string GetStringArg(const alm::fw::App::AppArgs& args, const std::string& key, const std::string& defaultValue = {})
{
	auto it = args.find(key);
	if (it == args.end())
		return defaultValue;
	else
		return it->second;
}

} // anonymous namespace

int SDL_main(int argc, char* argv[])
{
	auto app = CreateApp();
	auto args = ParseArgs(argc, argv);
	app->Run(args);

	return 0;
}

alm::fw::App::App(const std::string& name, RenderStageSetMode renderStageSetMode) :
	m_FrameworkUI{ nullptr },
	m_Window{ nullptr },
	m_Name{ name },
	m_RenderStageSetMode{ renderStageSetMode }
{}

alm::fw::App::~App()
{}

void alm::fw::App::Run(const AppArgs& args)
{
	m_StartupArgs = args;

	InitInternal();
	Initialize();

	MainLoop();

	Shutdown();
	ShutdownInternal();
}

bool alm::fw::App::GetStartupArgBool(const std::string& key, bool defaultValue)
{
	return GetBoolArg(m_StartupArgs, key, defaultValue);
}

std::string alm::fw::App::GetStartupArgString(const std::string& key, const std::string& defaultValue)
{
	return GetStringArg(m_StartupArgs, key, defaultValue);
}

void alm::fw::App::RefreshUIData()
{
	alm::gfx::RenderGraph* renderGraph = m_MainRenderView->GetRenderGraph().get();
	auto shadowmapRS = renderGraph->GetRenderStage<alm::gfx::ShadowmapRenderStage>();
	auto simpleSkyRS = renderGraph->GetRenderStage<alm::gfx::SimpleSkyRenderStage>();
	auto skyRS = renderGraph->GetRenderStage<alm::gfx::SkyRenderStage>();
	auto cloudsRS = renderGraph->GetRenderStage<alm::gfx::CloudsRenderStage>();
	auto SSAORS = renderGraph->GetRenderStage<alm::gfx::SSAORenderStage>();
	auto bloomRS = renderGraph->GetRenderStage<alm::gfx::BloomRenderStage>();
	auto tonemappingRS = renderGraph->GetRenderStage<alm::gfx::ToneMappingRenderStage>();
	auto compositeRS = renderGraph->GetRenderStage<alm::gfx::CompositeRenderStage>();
	auto& data = m_FrameworkUI->FrameworkData;

	if (shadowmapRS)
	{
		data.ShadowmapSize = shadowmapRS->GetSize();
	}
	data.SunParams = m_Scene->GetSunParams();
	data.AmbientParams = m_Scene->GetAmbientParams();

	if (simpleSkyRS)
		data.SimpleSkyParams = simpleSkyRS->GetSkyParams();

	if (skyRS)
		data.SkyParams = skyRS->GetSkyParams();

	if (cloudsRS)
		data.CloudsParams = cloudsRS->GetCloudsParams();

	if (SSAORS)
	{
		data.SSAO.Radius = SSAORS->GetRadius();
		data.SSAO.Power = SSAORS->GetPower();
		data.SSAO.Bias = SSAORS->GetBias();
	}

	if (bloomRS)
	{
		data.Bloom.Radius = bloomRS->GetFilterRadius();
		data.Bloom.Strength = bloomRS->GetStrength();
		data.Bloom.MaxMip = bloomRS->GetMaxMipChainLenght();
	}

	if (tonemappingRS && compositeRS)
	{
		data.Tonemapping.MiddleGrayNits = tonemappingRS->GetSceneMiddleGray() * compositeRS->GetPaperWhiteNits();
		data.Tonemapping.PaperWhiteNits = compositeRS->GetPaperWhiteNits();
		data.Tonemapping.SdrExposureBias = tonemappingRS->GetSDRExposureBias();
		data.Tonemapping.MinLogLuminance = tonemappingRS->GetMinLogLuminance();
		data.Tonemapping.LogLuminanceRange = tonemappingRS->GetLogLuminanceRange();
		data.Tonemapping.AdaptationUpSpeed = tonemappingRS->GetAdaptationUpSpeed();
		data.Tonemapping.AdaptationDownSpeed = tonemappingRS->GetAdaptationDownSpeed();
	}
}

alm::gfx::RenderStageTypeID alm::fw::App::GetUIRenderStageType() const
{ 
	return alm::fw::FrameworkUI::StaticType(); 
}

bool alm::fw::App::InitInternal()
{
	// Initialize SDL
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		LOG_FATAL("Error initializing SDL.");
		return false; // Initialization failed
	}

	// Create a window
	m_Window = SDL_CreateWindow(m_Name.c_str(), 1920, 1080, SDL_WINDOW_RESIZABLE);
	if (!m_Window)
	{
		SDL_Quit();
		return false; // Window creation failed
	}

	const bool graphicsDebug = GetStartupArgBool("gd", false);
	const bool vSync = GetStartupArgBool("vsync", false);

	// Init device manager
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
	std::string windowTitle = m_Name + " - " + m_DeviceManager->GetBackEndHWName();
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
	m_Scene = alm::make_unique_with_weak<alm::gfx::Scene>("DefaultScene", m_DeviceManager.get());

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
		m_FrameworkUI = dynamic_cast<alm::fw::FrameworkUI*>(m_ImGuiRS.get());

		// UI initial values
		if(m_FrameworkUI)
		{
			RefreshUIData();
		}
	}

	return true;
}

void alm::fw::App::ShutdownInternal()
{
	m_MainCamera.reset();
	m_MainRenderView.reset();
	m_Scene.reset();

	ImGui::DestroyPlatformWindows();
	alm::gfx::ReleaseImGuiViewportsRenderer();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	m_DeviceManager->Shutdown();

	SDL_DestroyWindow(m_Window);
	SDL_Quit();
}

void alm::fw::App::InitRenderStages()
{
	if (m_RenderStageSetMode == RenderStageSetMode::None)
		return;

	if (m_RenderStageSetMode == RenderStageSetMode::User)
	{
		m_ImGuiRS = UserInitRenderStages();
		return;
	}

	// Lets create a default set of render stages
	{
		auto shadowmapRS = gfx::RenderStageFactory::CreateShared<gfx::ShadowmapRenderStage>();
		auto depthPrepassRS = gfx::RenderStageFactory::CreateShared<gfx::DepthPrepassRenderStage>();
		auto linearizeDepthRS = gfx::RenderStageFactory::CreateShared<gfx::LinearizeDepthRenderStage>();
		auto SSAORS = gfx::RenderStageFactory::CreateShared<gfx::SSAORenderStage>();
		auto GBuffersRS = gfx::RenderStageFactory::CreateShared<gfx::GBuffersRenderStage>();
		auto deferredLightingRS = gfx::RenderStageFactory::CreateShared<gfx::DeferredLightingRenderStage>();
		auto simpleSkyRS = gfx::RenderStageFactory::CreateShared<gfx::SimpleSkyRenderStage>();
		auto WBOITAccumRS = gfx::RenderStageFactory::CreateShared<alm::gfx::WBOITAccumRenderStage>();
		auto WBOITResolveRS = gfx::RenderStageFactory::CreateShared<alm::gfx::WBOITResolveRenderStage>();
		auto bloomRS = gfx::RenderStageFactory::CreateShared<alm::gfx::BloomRenderStage>();
		auto toneMappingRS = gfx::RenderStageFactory::CreateShared<alm::gfx::ToneMappingRenderStage>();
		auto debugRS = gfx::RenderStageFactory::CreateShared<alm::gfx::DebugRenderStage>();
		auto wireframeRS = gfx::RenderStageFactory::CreateShared<alm::gfx::WireframeRenderStage>();
		auto compositeRS = gfx::RenderStageFactory::CreateShared<alm::gfx::CompositeRenderStage>();

		std::shared_ptr<alm::gfx::ImGuiRenderStage> ImGuiRS;
		if (GetUIRenderStageType() != gfx::RenderStageType_None)
		{
			auto userUI = gfx::RenderStageFactory::CreateShared(GetUIRenderStageType());
			ImGuiRS = std::dynamic_pointer_cast<alm::gfx::ImGuiRenderStage>(std::move(userUI));
		}
		else
		{
			ImGuiRS = gfx::RenderStageFactory::CreateShared<alm::gfx::ImGuiRenderStage>();
		}

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
}

void alm::fw::App::MainLoop()
{
	bool requestQuit = false;
	auto appStartTime = std::chrono::steady_clock::now();
	auto lastTime = appStartTime;
	auto fpsLastTime = appStartTime;
	uint32_t fpsFrameCount = 0;
	bool mouseMiddlePressed = false;

	while (!requestQuit)
	{
		const auto currentTime = std::chrono::steady_clock::now();
		const std::chrono::duration<double> totalDuration = currentTime - appStartTime;
		const double totalSec = totalDuration.count();
		const std::chrono::duration<float> elapsedDuration = currentTime - lastTime;
		const float elapsedSec = elapsedDuration.count();
		lastTime = currentTime;

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
				requestQuit = true;
				forwardEvent = false;
				break;

			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				if (event.window.windowID == SDL_GetWindowID(m_Window))
				{
					requestQuit = true;
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
		m_Scene->Update();

		// Update UI
		if (m_FrameworkUI)
		{
			m_FrameworkUI->SetRenderStats(m_FPS, m_CPUTime, m_GPUTime);
		}

		if (m_DeviceManager->UpdateWindowSize())
		{
			float2 newSize = m_DeviceManager->GetWindowDimensions();
			m_MainCamera->SetAspect(newSize.x / newSize.y);

			m_MainRenderView->OnWindowSizeChanged();
		}

		m_DeviceManager->Render([&]()
		{
			m_MainRenderView->Render(totalSec, elapsedSec);
		});

		// Apply UI settings
		{
			alm::gfx::RenderGraph* renderGraph = m_MainRenderView->GetRenderGraph().get();
			auto debugRS = renderGraph->GetRenderStage<gfx::DebugRenderStage>();
			auto shadowmapRS = renderGraph->GetRenderStage<alm::gfx::ShadowmapRenderStage>();
			auto simpleSkyRS = renderGraph->GetRenderStage<alm::gfx::SimpleSkyRenderStage>();
			auto skyRS = renderGraph->GetRenderStage<alm::gfx::SkyRenderStage>();
			auto cloudsRS = renderGraph->GetRenderStage<alm::gfx::CloudsRenderStage>();
			auto lightingRS = renderGraph->GetRenderStage<alm::gfx::DeferredLightingRenderStage>();
			auto SSAORS = renderGraph->GetRenderStage<alm::gfx::SSAORenderStage>();
			auto bloomRS = renderGraph->GetRenderStage<alm::gfx::BloomRenderStage>();
			auto tonemappingRS = renderGraph->GetRenderStage<alm::gfx::ToneMappingRenderStage>();
			auto compositeRS = renderGraph->GetRenderStage<alm::gfx::CompositeRenderStage>();
			auto& data = m_FrameworkUI->FrameworkData;

			if (!data.RequestedRenderMode.empty())
			{
				renderGraph->SetActiveRenderMode(data.RequestedRenderMode);
				data.RequestedRenderMode.clear();
			}

			debugRS->ShowRenderBBoxes(alm::gfx::SceneContentType::Meshes, data.Debug.ShowMeshBBoxes);
			debugRS->ShowRenderBBoxes(alm::gfx::SceneContentType::SpotLights, data.Debug.ShowLightBBoxes);

			if (shadowmapRS->GetSize() != data.ShadowmapSize)
				shadowmapRS->SetSize(data.ShadowmapSize);

			if (data.SunParamsUpdated)
			{
				m_Scene->SetSunParams(data.SunParams);
				data.SunParamsUpdated = false;
			}
			if (data.AmbientParamsUpdated)
			{
				m_Scene->SetAmbientParams(data.AmbientParams);
				data.AmbientParamsUpdated = false;
			}

			if (simpleSkyRS)
				simpleSkyRS->SetSkyParams(data.SimpleSkyParams);

			if (skyRS)
				skyRS->SetSkyParams(data.SkyParams);

			if (cloudsRS)
				cloudsRS->SetCloudsParams(data.CloudsParams);

			lightingRS->SetMaterialChannel(data.MatChannel);

			if (SSAORS)
			{
				if(SSAORS->IsSSAOEnabled() != data.SSAO.Enabled)
					SSAORS->SetSSAOEnabled(data.SSAO.Enabled);
				lightingRS->ShowSSAO(data.SSAO.View);
				SSAORS->SetRadius(data.SSAO.Radius);
				SSAORS->SetPower(data.SSAO.Power);
				SSAORS->SetBias(data.SSAO.Bias);
			}

			if (bloomRS)
			{
				bloomRS->SetBloomEnabled(data.Bloom.Enabled);
				bloomRS->SetFilterRadius(data.Bloom.Radius);
				bloomRS->SetStrength(data.Bloom.Strength);
				bloomRS->SetMaxMipChainLenght(data.Bloom.MaxMip);
			}

			if (tonemappingRS && compositeRS)
			{
				tonemappingRS->SetTonemappingEnabled(data.Tonemapping.Enabled);
				// Scene middlegray is middle_gray_nits / paper_white_nits
				tonemappingRS->SetSceneMiddleGray(data.Tonemapping.MiddleGrayNits / data.Tonemapping.PaperWhiteNits);
				tonemappingRS->SetMinLogLuminance(data.Tonemapping.MinLogLuminance);
				tonemappingRS->SetLogLuminanceRange(data.Tonemapping.LogLuminanceRange);
				tonemappingRS->SetSDRExposureBias(data.Tonemapping.SdrExposureBias);
				tonemappingRS->SetAdaptationUpSpeed(data.Tonemapping.AdaptationUpSpeed);
				tonemappingRS->SetAdaptationDownSpeed(data.Tonemapping.AdaptationDownSpeed);
			}
		}

		fpsFrameCount++;
		std::this_thread::yield();
	}
}

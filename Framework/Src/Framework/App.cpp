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
#include "Gfx/GltfImporter.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/MeshInstance.h"
#include "Gfx/Mesh.h"
#include "Gfx/Material.h"
#include "RHI/Device.h"
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
	m_RenderStageSetMode{ renderStageSetMode },
	m_ArrowMeshScale{ 1.f }
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

std::optional<bool> alm::fw::App::GetStartupArgBool(const std::string& key)
{
	auto it = m_StartupArgs.find(key);
	if (it == m_StartupArgs.end())
		return std::nullopt;

	return it->second == "0" ? false : true;
}

std::optional<std::string> alm::fw::App::GetStartupArgString(const std::string& key)
{
	auto it = m_StartupArgs.find(key);
	if (it == m_StartupArgs.end())
		return std::nullopt;

	return it->second;
}

std::optional<int> alm::fw::App::GetStartupArgInt(const std::string& key)
{
	auto it = m_StartupArgs.find(key);
	if (it == m_StartupArgs.end())
		return std::nullopt;

	int value;
	auto [_, ec] = std::from_chars(it->second.data(), it->second.data() + it->second.size(), value);
	if (ec == std::errc())
	{
		return value;
	}
	return std::nullopt;
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

void alm::fw::App::ShowArrow(const gfx::Transform& transform)
{
	// Lazy load
	if (!m_ArrowMeshY)
	{
		auto importResult = alm::gfx::ImportGlTF("arrowY.gltf", m_DeviceManager.get());
		if (!importResult)
		{
			LOG_ERROR(importResult.error());
			return;
		}

		m_ArrowMeshY = m_Scene->GetSceneGraph()->GetRoot()->AddChild(std::move(*importResult));

		// Change material domain so it does not write de z-buffer
		for (auto walker = gfx::SceneGraph::Walker{ m_ArrowMeshY.get() }; walker; walker.Next())
		{
			auto node = *walker;
			if (node->GetLeaf() && node->GetLeaf()->GetType() == gfx::SceneGraphLeaf::Type::MeshInstance)
			{
				auto meshInstance = alm::checked_pointer_cast<gfx::MeshInstance>(node->GetLeaf());
				auto mat = meshInstance->GetMesh()->GetMaterial();
				mat->SetDomain(gfx::MaterialDomain::AlphaBlended);
			}
		}
	}

	for (auto walker = gfx::SceneGraph::Walker{ m_ArrowMeshY.get() }; walker; walker.Next())
	{
		auto node = *walker;
		if (node->GetLeaf())
		{
			node->GetLeaf()->SetVisible(true);
		}
	}

	m_ArrowMeshY->SetLocalTransform(transform);
}

void alm::fw::App::HideArrow()
{
	if (m_ArrowMeshY)
	{
		for (auto walker = gfx::SceneGraph::Walker{ m_ArrowMeshY.get() }; walker; walker.Next())
		{
			auto node = *walker;
			if (node->GetLeaf())
			{
				node->GetLeaf()->SetVisible(false);
			}
		}
	}
}

void alm::fw::App::ShowNormal(const uint2& screenPos)
{
	auto* renderGraph = m_MainRenderView->GetRenderGraph().get();
	auto gBuffer2TexHandle = renderGraph->GetTextureHandle("GBuffer2");

	if (!gBuffer2TexHandle)
	{
		LOG_ERROR("Could not find \"GBuffer2\" texture in the render graph.");
		return;
	}

	auto linearDepthTexHandle = renderGraph->GetTextureHandle("LinearDepth");
	if (!linearDepthTexHandle)
	{
		LOG_ERROR("Could not find \"LinearDepth\" texture in the render graph.");
		return;
	}

	const rhi::TextureDesc& gBuffer2TexDesc = renderGraph->GetTexture(gBuffer2TexHandle)->GetDesc();
	const rhi::TextureDesc& linearDepthTexDesc = renderGraph->GetTexture(linearDepthTexHandle)->GetDesc();
	
	assert(gBuffer2TexDesc.width == linearDepthTexDesc.width && gBuffer2TexDesc.height == linearDepthTexDesc.height);
	assert(gBuffer2TexDesc.format == rhi::Format::RGBA16_FLOAT);
	assert(linearDepthTexDesc.format == rhi::Format::R32_FLOAT);

	if (!m_GBuffer2ViewTicket)
	{
		m_GBuffer2ViewTicket = renderGraph->RequestBufferView(
			gfx::GBuffersRenderStage::StaticType(), gfx::RenderGraph::AccessMode::Write, gBuffer2TexHandle);
		if (!m_GBuffer2ViewTicket)
		{
			LOG_ERROR("Failed to request GBuffer2 view ticket");
			return;
		}
	}
	if (!m_LinearDepthViewTicket)
	{
		m_LinearDepthViewTicket = renderGraph->RequestBufferView(
			gfx::LinearizeDepthRenderStage::StaticType(), gfx::RenderGraph::AccessMode::Write, linearDepthTexHandle);
		if (!m_LinearDepthViewTicket)
		{
			LOG_ERROR("Failed to request LinearDepth view ticket");
			return;
		}
	}

	float3 normal = { 0.f, 0.f, 0.f };
	bool validNormal = false;
	float3 worldPos = { 0.f, 0.f, 0.f };
	bool validPos = false;

	alm::rhi::BufferHandle gBuffer2Buffer = renderGraph->GetBufferView(m_GBuffer2ViewTicket);
	alm::rhi::BufferHandle linearDepthBuffer = renderGraph->GetBufferView(m_LinearDepthViewTicket);
	if (gBuffer2Buffer && linearDepthBuffer)
	{
		// Get normal
		{
			alm::rhi::SubresourceCopyableRequirements copyReq = m_DeviceManager->GetDevice()->GetSubresourceCopyableRequirements(
				gBuffer2TexDesc, 0, 0);

			char* gBufferData = (char*)gBuffer2Buffer->Map();
			gBufferData += copyReq.offset;
			gBufferData += screenPos.y * copyReq.rowStride;
			gBufferData += screenPos.x * sizeof(short4);

			ushort4 pixelData = *(ushort4*)gBufferData;
			gBuffer2Buffer->Unmap();
			
			if (pixelData.x != 0 || pixelData.y != 0)
			{
				float2 encodedNormal = { HalfToFloat(pixelData.x), HalfToFloat(pixelData.y) };
				normal = DecodeNormal(encodedNormal);
				float4x4 invCamViewMatrix = glm::inverse(m_MainRenderView->GetCamera()->GetViewMatrix());
				normal = invCamViewMatrix * float4{ normal.x, normal.y, normal.z, 0.f };
				normal = glm::normalize(normal);

				validNormal = true;
			}
		}

		// Get world pos
		{
			alm::rhi::SubresourceCopyableRequirements copyReq = m_DeviceManager->GetDevice()->GetSubresourceCopyableRequirements(
				linearDepthTexDesc, 0, 0);

			char* linearDepthData = (char*)linearDepthBuffer->Map();
			linearDepthData += copyReq.offset;
			linearDepthData += screenPos.y * copyReq.rowStride;
			linearDepthData += screenPos.x * sizeof(float);
			linearDepthBuffer->Unmap();

			float depth = *(float*)linearDepthData;

			worldPos = m_MainRenderView->GetCamera()->ScreenToWorld(
				screenPos, depth, uint2{ linearDepthTexDesc.width, linearDepthTexDesc.height });

			validPos = true;
		}
	}

	if (validNormal && validPos)
	{
		gfx::Transform transform;
		transform.SetTranslation(worldPos);
		transform.SetRotation(glm::rotation(float3{ 0.f, 1.f, 0.f }, normal));
		transform.SetScale(float3{ m_ArrowMeshScale });

		ShowArrow(transform);
	}

	m_FrameworkUI->AddBottomBarText(std::format("Normal: {{{:1.1f}, {:1.1f}, {:1.1f}}}", normal.x, normal.y, normal.z));
	m_FrameworkUI->AddBottomBarText(std::format("Pos: {{{:1.1f}, {:1.1f}, {:1.1f}}}", worldPos.x, worldPos.y, worldPos.z));
}

void alm::fw::App::HideNormal()
{
	HideArrow();
}

alm::gfx::RenderStageTypeID alm::fw::App::GetUIRenderStageType() const
{ 
	return alm::fw::FrameworkUI::StaticType(); 
}

bool alm::fw::App::InitInternal()
{
	LOG_INFO("Init SDL...");
	{
		// Initialize SDL
		if (!SDL_Init(SDL_INIT_VIDEO))
		{
			LOG_FATAL("Error initializing SDL.");
			return false; // Initialization failed
		}
	}
	LOG_INFO("Init SDL: Done");

	LOG_INFO("Creating SDL Window...");
	{
		// Create a window
		m_Window = SDL_CreateWindow(m_Name.c_str(), 1920, 1080, SDL_WINDOW_RESIZABLE);
		if (!m_Window)
		{
			SDL_Quit();
			return false; // Window creation failed
		}
	}
	LOG_INFO("Creating SDL Window...");

	LOG_INFO("Parsing startup arguments: Done");

	const bool vSync = GetStartupArgBool("vsync").value_or(true);
	const int fpsCap = GetStartupArgInt("fps_cap").value_or(0);
	const bool graphicsDebug = GetStartupArgBool("gd").value_or(false);
	const bool shadersDebug = GetStartupArgBool("shaders_debug").value_or(false);

	LOG_INFO("Parsing startup arguments: Done");

	LOG_INFO("   vsync:         {}", vSync);
	LOG_INFO("   fps_cap:       {}", fpsCap);
	LOG_INFO("   gd:            {}", graphicsDebug);
	LOG_INFO("   shaders_debug: {}", shadersDebug);


	LOG_INFO("Init device manager...");
	{
		m_DeviceManager = std::unique_ptr<alm::gfx::DeviceManager>{ alm::gfx::DeviceManager::Create(alm::gfx::GraphicsAPI::D3D12) };
		alm::gfx::DeviceManager::DeviceParams initParams{
			.WindowHandle = m_Window,
			.DebugRuntime = graphicsDebug,
			.GPUValidation = graphicsDebug,
			.VSyncEnabled = vSync,
			.FPSCap = fpsCap,
			.ShadersDebug = shadersDebug,
			.ForceSDR = false
		};
		m_DeviceManager->Init(initParams);
	}
	LOG_INFO("Init device manager: Done");

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
	uint32_t cpuIdleUSec = 0;

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
			m_CPUTimeMilliSec = elapsedSec * 1000;
			m_CPUIdleTimeMilliSec = (float)cpuIdleUSec / 1000;

			m_GPUTimeMilliSec = (std::max)(m_DeviceManager->GetGPUFrameTime(), 0.f);

			std::chrono::duration<float> fpsElapsed = currentTime - fpsLastTime;
			if (fpsElapsed.count() > 1.0f)
			{
				m_FPS = fpsFrameCount / fpsElapsed.count();
				fpsFrameCount = 0;
				fpsLastTime = currentTime;
			}
		}

		// Input
		const bool* keyboardState = SDL_GetKeyboardState(nullptr);
		const bool ctrlPressed = keyboardState[SDL_SCANCODE_LCTRL] || keyboardState[SDL_SCANCODE_RCTRL];

		float mouseX, mouseY;
		SDL_MouseButtonFlags mouseButtonFlags = SDL_GetMouseState(&mouseX, &mouseY) & SDL_BUTTON_LEFT;
		const bool mouseLeftPressed = mouseButtonFlags & SDL_BUTTON_LEFT;
		const bool mouseRightPressed = mouseButtonFlags & SDL_BUTTON_RIGHT;

		const bool requestShowNormal = ctrlPressed && mouseLeftPressed;

		bool forwardEvent = true;
		forwardEvent &= !requestShowNormal;

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL3_ProcessEvent(&event);

			switch (event.type)
			{
			case SDL_EVENT_MOUSE_WHEEL:
				if (requestShowNormal)
				{
					float magnitude = powf(10.f, floorf(log10f(m_ArrowMeshScale)));
					m_ArrowMeshScale += event.wheel.y * magnitude;
					m_ArrowMeshScale = std::max(m_ArrowMeshScale, 0.01f);
				};
				break;

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

		if (requestShowNormal)
		{
			ShowNormal({ mouseX, mouseY });
		}
		else
		{
			HideNormal();
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
			m_FrameworkUI->SetRenderStats(m_FPS, m_CPUTimeMilliSec, m_CPUIdleTimeMilliSec, m_GPUTimeMilliSec);
		}

		if (m_DeviceManager->UpdateWindowSize())
		{
			float2 newSize = m_DeviceManager->GetWindowDimensions();
			m_MainCamera->SetAspect(newSize.x / newSize.y);

			m_MainRenderView->OnWindowSizeChanged();
		}

		m_DeviceManager->Render([&]()
		{
			float2 mousePos;
			SDL_MouseButtonFlags mouseButtonFlags = SDL_GetMouseState(&mousePos.x, &mousePos.y);
			gfx::RenderView::MouseState mouseState{
				.pos = mousePos,
				.leftButton = mouseButtonFlags & SDL_BUTTON_LEFT ? true : false,
				.rightButton = mouseButtonFlags & SDL_BUTTON_RIGHT ? true : false
			};

			m_MainRenderView->Render(totalSec, elapsedSec, mouseState);
		}, cpuIdleUSec);

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
			debugRS->ShowHeightmapsBBoxes(data.Debug.ShowHeightmapBBoxes);

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
			{
				skyRS->SetEnabled(data.SkyEnabled);
				skyRS->SetSkyParams(data.SkyParams);
			}

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

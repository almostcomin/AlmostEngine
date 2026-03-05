#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <imgui/imgui_impl_sdl3.h>
#include "Core/Core.h"
#include "Core/Log.h"
#include "Gfx/DeviceManager.h"
#include "Gfx/ShaderFactory.h"
#include "Gfx/RenderView.h"
#include "Gfx/GltfImporter.h"
#include "Gfx/DataUploader.h"
#include "Gfx/Scene.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/SceneGraphLeaf.h"
#include "Gfx/Camera.h"
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
#include "Gfx/ImGUIViewportsRenderer.h"
#include "UI/StructureUI.h"
#include <thread>
#include <sstream>
#include <iomanip>

namespace
{
void PrintSceneGraph(const st::weak<st::gfx::SceneGraphNode>& root)
{
    st::gfx::SceneGraph::Walker walker(root);
    int depth = 0;
    while(walker)
    {
        std::stringstream ss;
		ss << std::fixed << std::setprecision(2);

        for (int i = 0; i < depth; i++)
            ss << "   ";

        if (walker->GetName().empty())
            ss << "<Unnamed>";
        else
            ss << walker->GetName();

        if (walker->HasBounds())
        {
            const auto& bbox = walker->GetWorldBounds();
            ss << " [" << bbox.min.x << ", " << bbox.min.y << ", " << bbox.min.z << " .. "
                << bbox.max.x << ", " << bbox.max.y << ", " << bbox.max.z << "]";
        }

        if (walker->GetLeaf())
        {
			switch (walker->GetLeaf()->GetType())
			{
			case st::gfx::SceneGraphLeaf::Type::MeshInstance:
				ss << " : MESH_INSTANCE ";
				break;
			case st::gfx::SceneGraphLeaf::Type::Camera:
				ss << " : CAMERA ";
				break;
			default:
				ss << " : UNKNOWN_LEAF ";
			}
        }

		if (!ss.str().empty())
			st::log::Info("{}", ss.str());

        depth += walker.Next();
    }
}

} // anonymous namespace

int SDL_main(int argc, char* argv[]) 
{
	// Your SDL application code goes here
	// For example, initializing SDL and creating a window
	// Initialize SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) 
	{
		LOG_FATAL("Error initializing SDL.");
		return -1; // Initialization failed
	}

	// Create a window
	SDL_Window* window = SDL_CreateWindow("Structure", 1920, 1080, SDL_WINDOW_RESIZABLE);
	if (!window) 
	{
		SDL_Quit();
		return -1; // Window creation failed
	}

	// Init device manager
	std::unique_ptr<st::gfx::DeviceManager> deviceManager{ st::gfx::DeviceManager::Create(st::gfx::GraphicsAPI::D3D12) };
	st::gfx::DeviceManager::DeviceParams initParams{
		.WindowHandle = window,
		.DebugRuntime = true,
		.GPUValidation = true,
		.VSyncEnabled = false,
		.ForceSDR = false
	};
	deviceManager->Init(initParams);
	std::string windowTitle = "Structure";
	windowTitle = windowTitle + " - " + deviceManager->GetBackEndHWName();
	SDL_SetWindowTitle(window, windowTitle.c_str());

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
		//io.ConfigDpiScaleFonts = true;      // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
		//io.ConfigDpiScaleViewports = true;  // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplSDL3_InitForOther(window);
	}

	// Our scene
	auto scene = st::make_unique_with_weak<st::gfx::Scene>(deviceManager.get());

	// Create main RenderView
	auto mainRenderView = st::make_unique_with_weak<st::gfx::RenderView>(deviceManager.get(), "Main view");

	// Create shadowmap render stage
	std::shared_ptr<st::gfx::ShadowmapRenderStage> shadowmapRS{ new st::gfx::ShadowmapRenderStage{ 2048, 4, st::rhi::Format::D32 }};
	// Create depth prepass render stage
	std::shared_ptr<st::gfx::DepthPrepassRenderStage> depthPrepassRS{ new st::gfx::DepthPrepassRenderStage };
	// Create linearize-depth render stage
	std::shared_ptr<st::gfx::LinearizeDepthRenderStage> linearizeDepthRS{ new st::gfx::LinearizeDepthRenderStage };
	// Screen-space ambient occlusion
	std::shared_ptr<st::gfx::SSAORenderStage> SSAORS{ new st::gfx::SSAORenderStage };
	// Create deferred render stage
	std::shared_ptr<st::gfx::GBuffersRenderStage> gBufRS{ new st::gfx::GBuffersRenderStage };
	// Create lighting render stage
	std::shared_ptr<st::gfx::DeferredLightingRenderStage> lightingRS{ new st::gfx::DeferredLightingRenderStage };
	// Create TonemMapping
	std::shared_ptr<st::gfx::ToneMappingRenderStage> toneMappingRS{ new st::gfx::ToneMappingRenderStage };
	// Create debug render stage
	std::shared_ptr<st::gfx::DebugRenderStage> debugRS{ new st::gfx::DebugRenderStage };
	// Wireframe render stage
	std::shared_ptr<st::gfx::WireframeRenderStage> wireframeRS{ new st::gfx::WireframeRenderStage };

	// Create UI render stage
	std::string requestLoadFile;
	bool requestClose = false;
	bool requestQuit = false;
	std::shared_ptr<StructureUI> uiRS{ new StructureUI{ mainRenderView.get_weak(), window, shadowmapRS.get(), toneMappingRS.get(), deviceManager.get() }};
	uiRS->m_RequestLoadFile = [&requestLoadFile](const char* filename) { requestLoadFile = filename; };
	uiRS->m_RequestClose = [&requestClose] { requestClose = true; };
	uiRS->m_RequestQuit = [&requestQuit] { requestQuit = true; };

	st::gfx::InitImGuiViewportsRenderer(uiRS, deviceManager.get());

	// Create composite render stage
	std::shared_ptr<st::gfx::CompositeRenderStage> compositeRS{ new st::gfx::CompositeRenderStage };

	// Create camera
	auto camera = std::make_shared<st::gfx::Camera>();
	int windowWidth, windowHeight;
	SDL_GetWindowSize(window, (int*)&windowWidth, (int*)&windowHeight);
	camera->SetAspect((float)windowWidth / windowHeight);
	camera->SetPosition({ 0.f, 0.f, 5.f });

	// Add stages to render view
	mainRenderView->SetRenderStages({ shadowmapRS, depthPrepassRS, linearizeDepthRS, gBufRS, SSAORS, lightingRS, toneMappingRS, debugRS, uiRS, compositeRS, wireframeRS });
	// Define default render mode
	mainRenderView->SetRenderMode("Default", 
		{ shadowmapRS.get(), depthPrepassRS.get(), linearizeDepthRS.get(), gBufRS.get(), SSAORS.get(), lightingRS.get(), toneMappingRS.get(), 
		  debugRS.get(), uiRS.get(), compositeRS.get() });
	// Define wireframe render mode
	mainRenderView->SetRenderMode("Wireframe",
		{ depthPrepassRS.get(), wireframeRS.get(), debugRS.get(), uiRS.get(), compositeRS.get() });

	mainRenderView->SetCamera(camera);

	// Update UI data with initial render stages values
	uiRS->m_Data.ShadowmapSize = shadowmapRS->GetSize();
	uiRS->m_Data.AmbientParams = scene->GetAmbientParams();
	uiRS->m_Data.SunParams = scene->GetSunParams();

	uiRS->m_Data.ShadowmapDepthBias = shadowmapRS->GetDepthBias();
	uiRS->m_Data.ShadowmapSlopeScaledDepthBias = shadowmapRS->GetSlopeScaledDepthBias();

	uiRS->m_Data.SSAO_Radius = SSAORS->GetRadius();
	uiRS->m_Data.SSAO_Power = SSAORS->GetPower();
	uiRS->m_Data.SSAO_Bias = SSAORS->GetBias();

	uiRS->m_Data.middleGrayNits = toneMappingRS->GetSceneMiddleGray() * compositeRS->GetPaperWhiteNits();
	uiRS->m_Data.paperWhiteNits = compositeRS->GetPaperWhiteNits();
	uiRS->m_Data.sdrExposureBias = toneMappingRS->GetSDRExposureBias();
	uiRS->m_Data.minLogLuminance = toneMappingRS->GetMinLogLuminance();
	uiRS->m_Data.logLuminanceRange = toneMappingRS->GetLogLuminanceRange();
	uiRS->m_Data.adaptationUpSpeed = toneMappingRS->GetAdaptationUpSpeed();
	uiRS->m_Data.adaptationDownSpeed = toneMappingRS->GetAdaptationDownSpeed();

	// Main loop

	auto lastTime = std::chrono::steady_clock::now();
	auto fpsLastTime = lastTime;
	uint32_t fpsFrameCount = 0;
	float2 cameraSpeed{ 0.f };
	bool mouseMiddlePressed = false;
	while (!requestQuit)
	{
		const auto currentTime = std::chrono::steady_clock::now();
		const std::chrono::duration<float> elapsed = currentTime - lastTime;
		const float elapsedSec = elapsed.count();
		uiRS->m_Data.CPUTime = elapsedSec / 1000;
		uiRS->m_Data.GPUTime = (std::max)(deviceManager->GetGPUFrameTime(), 0.f);

		if (!requestLoadFile.empty())
		{
			auto importResult = st::gfx::ImportGlTF(requestLoadFile.c_str(), deviceManager.get());
			if (importResult)
			{
				scene->SetSceneGraph(std::move(*importResult));
				mainRenderView->SetScene(scene.get_weak());

				const st::math::aabox3f& bounds = scene->GetSceneGraph()->GetRoot()->GetWorldBounds();
				const float radius = glm::length(bounds.extents()) / 2.f;
				camera->SetZNear(radius * 0.05f);

				camera->SetPosition(float3{ -1000.f, 500.f, 1000.f });
				camera->Fit(bounds);

				uiRS->m_Data.CameraSpeed = radius * 1.f;

				PrintSceneGraph(scene->GetSceneGraph()->GetRoot());
			}
			else
			{
				LOG_ERROR("Error importing file '{}': {}", requestLoadFile, importResult.error());
			}
			requestLoadFile.clear();
		}
		if (requestClose)
		{
			if (scene)
			{
				scene.reset();
			}
			requestClose = false;
		}

		// Input
		SDL_Event event;
		while (SDL_PollEvent(&event)) 
		{
			ImGui_ImplSDL3_ProcessEvent(&event);
			const ImGuiIO& io = ImGui::GetIO();

			switch (event.type)
			{
			case SDL_EVENT_MOUSE_MOTION:
				// Pass event to ImGui
				if (!io.WantCaptureMouse)
				{
					if (mouseMiddlePressed)
					{
						int windowWidth, windowHeight;
						SDL_GetWindowSize(window, (int*)&windowWidth, (int*)&windowHeight);
						{
							float angleRad = event.motion.yrel * PI / windowHeight;
							glm::quat q = glm::angleAxis(-angleRad, camera->GetRight());
							float3 newFwd = q * camera->GetForward();
							camera->SetForward(newFwd);
						}
						{
							float angleRad = event.motion.xrel * PI / windowWidth;
							glm::quat q = glm::angleAxis(-angleRad, camera->GetUp());
							float3 newFwd = q * camera->GetForward();
							camera->SetForward(newFwd);
						}
					}
				}
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				if (!io.WantCaptureMouse)
				{
					switch (event.button.button)
					{
					case SDL_BUTTON_LEFT:
						break;
					case SDL_BUTTON_MIDDLE:
						mouseMiddlePressed = event.button.down;
						break;
					case SDL_BUTTON_RIGHT:
						break;
					}
				}
				break;
			case SDL_EVENT_KEY_DOWN:
				if (!io.WantCaptureKeyboard)
				{
					switch (event.key.key)
					{
					case SDLK_W: cameraSpeed.y = uiRS->m_Data.CameraSpeed; break;
					case SDLK_S: cameraSpeed.y = -uiRS->m_Data.CameraSpeed; break;
					case SDLK_A: cameraSpeed.x = uiRS->m_Data.CameraSpeed; break;
					case SDLK_D: cameraSpeed.x = -uiRS->m_Data.CameraSpeed; break;
					}
					break;

				case SDL_EVENT_KEY_UP:
					switch (event.key.key)
					{
					case SDLK_W: cameraSpeed.y = 0.f; break;
					case SDLK_S: cameraSpeed.y = 0.f; break;
					case SDLK_A: cameraSpeed.x = 0.f; break;
					case SDLK_D: cameraSpeed.x = 0.f; break;
					}
				}
				break;

			case SDL_EVENT_QUIT:
				requestQuit = true;
				break;

			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				if (event.window.windowID == SDL_GetWindowID(window))
				{
					requestQuit = true;
				}
			}
		}

		// Camera movement
		{
			const float3& camFwd = camera->GetForward();
			const float3& camRight = camera->GetRight();

			float3 newPos = camera->GetPosition();
			float2 cameraTurboSpeed = cameraSpeed;
			SDL_Keymod mod = SDL_GetModState();
			if (mod & SDL_KMOD_SHIFT)
				cameraTurboSpeed *= 2.f;
			newPos += camFwd * cameraTurboSpeed.y * elapsedSec;
			newPos += -camRight * cameraTurboSpeed.x * elapsedSec;

			camera->SetPosition(newPos);
		}

		// Scene graph update
		if (scene)
		{
			// Update scene
			scene->Update();
		}

		// Update UI values
		{
			if (!uiRS->m_Data.RenderMode.empty() && uiRS->m_Data.RenderMode != mainRenderView->GetCurrentRenderMode())
			{
				mainRenderView->SetCurrentRenderMode(uiRS->m_Data.RenderMode);
			}

			debugRS->SetRenderBBoxes(uiRS->m_Data.ShowBBoxes);

			lightingRS->ShowShadowmap(uiRS->m_Data.ShowShadowmap);
			if (uiRS->m_Data.ShadowmapDepthBias != shadowmapRS->GetDepthBias())
			{
				shadowmapRS->SetDepthBias(uiRS->m_Data.ShadowmapDepthBias);
			}
			if (uiRS->m_Data.ShadowmapSlopeScaledDepthBias != shadowmapRS->GetSlopeScaledDepthBias())
			{
				shadowmapRS->SetSlopeScaledDepthBias(uiRS->m_Data.ShadowmapSlopeScaledDepthBias);
			}
			if (uiRS->m_Data.ShadowmapSize != shadowmapRS->GetSize())
			{
				shadowmapRS->SetSize(uiRS->m_Data.ShadowmapSize);
			}
			if (uiRS->m_Data.ShadowmapEnabled != shadowmapRS->IsEnabled())
			{
				shadowmapRS->SetEnabled(uiRS->m_Data.ShadowmapEnabled);
			}

			if (uiRS->m_Data.AmbientParamsUpdated)
			{
				scene->SetAmbientParams(uiRS->m_Data.AmbientParams);
				uiRS->m_Data.AmbientParamsUpdated = false;
			}

			if (uiRS->m_Data.SunParamsUpdated)
			{
				scene->SetSunParams(uiRS->m_Data.SunParams);
				uiRS->m_Data.SunParamsUpdated = false;
			}

			lightingRS->SetMaterialChannel(uiRS->m_Data.MatChannel);

			if (uiRS->m_Data.SSAOEnabled != SSAORS->IsSSAOEnabled())
			{
				SSAORS->SetSSAOEnabled(uiRS->m_Data.SSAOEnabled);
			}
			lightingRS->ShowSSAO(uiRS->m_Data.ShowSSAO);
			SSAORS->SetRadius(uiRS->m_Data.SSAO_Radius);
			SSAORS->SetPower(uiRS->m_Data.SSAO_Power);
			SSAORS->SetBias(uiRS->m_Data.SSAO_Bias);

			toneMappingRS->SetTonemappingEnabled(uiRS->m_Data.tonemappingEnabled);
			// Scene middlegray is middle_gray_nits / paper_white_nits
			toneMappingRS->SetSceneMiddleGray(uiRS->m_Data.middleGrayNits / uiRS->m_Data.paperWhiteNits);
			toneMappingRS->SetMinLogLuminance(uiRS->m_Data.minLogLuminance);
			toneMappingRS->SetLogLuminanceRange(uiRS->m_Data.logLuminanceRange);
			toneMappingRS->SetSDRExposureBias(uiRS->m_Data.sdrExposureBias);
			toneMappingRS->SetAdaptationUpSpeed(uiRS->m_Data.adaptationUpSpeed);
			toneMappingRS->SetAdaptationDownSpeed(uiRS->m_Data.adaptationDownSpeed);
		}

		// Update FPS counter
		{
			std::chrono::duration<float> fpsElapsed = currentTime - fpsLastTime;
			if (fpsElapsed.count() > 1.f)
			{
				uiRS->m_Data.FPS = fpsFrameCount / fpsElapsed.count();
				fpsFrameCount = 0;
				fpsLastTime = currentTime;
			}
		}

		if (deviceManager->UpdateWindowSize())
		{
			float2 newSize = deviceManager->GetWindowDimensions();
			camera->SetAspect(newSize.x / newSize.y);

			mainRenderView->OnWindowSizeChanged();
		}

		deviceManager->Render([&mainRenderView, elapsedSec]()
		{
			mainRenderView->Render(elapsedSec);
		});

		fpsFrameCount++;
		lastTime = currentTime;
		std::this_thread::yield();
	}

	// Clean up
	camera.reset();
	mainRenderView.reset();
	uiRS.reset();
	compositeRS.reset();
	SSAORS.reset();
	linearizeDepthRS.reset();
	lightingRS.reset();
	gBufRS.reset();
	depthPrepassRS.reset();
	shadowmapRS.reset();
	wireframeRS.reset();
	debugRS.reset();
	toneMappingRS.reset();
	scene.reset();

	ImGui::DestroyPlatformWindows();
	st::gfx::ReleaseImGuiViewportsRenderer();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	deviceManager->Shutdown();

	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0; // Exit successfully
}
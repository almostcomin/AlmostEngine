#pragma once

#include "Core/Math/glm_config.h"
#include "Core/Memory.h"
#include <string>
#include "RHI/ResourceState.h"
#include "RHI/TypeForwards.h"
#include "Core/Math.h"

namespace alm::rhi
{
	class IFramebuffer;
}

namespace alm::gfx
{
	class RenderGraphBuilder;
	class Scene;
	class Camera;
	class RenderGraph;
	class DeviceManager;
}

namespace alm::gfx
{

class RenderStage
{
	friend class RenderView;
	friend class RenderGraph;

	struct TextureDeclaration
	{
		std::string id;
		rhi::ResourceState inputState;
		rhi::ResourceState outputState;
	};

public:

	RenderView* GetRenderView() const;
	Camera* GetCamera() const;
	Scene* GetScene() const;
	DeviceManager* GetDeviceManager() const;

	bool IsEnabled() const { return m_Enabled; }
	bool IsAttached() const { return m_RenderGraph; }

	void SetEnabled(bool b);

	virtual const char* GetDebugName() const = 0;

protected:

	alm::weak<RenderGraph> m_RenderGraph;

	virtual void Setup(RenderGraphBuilder& builder) = 0;

	virtual void Render(alm::rhi::CommandListHandle commandList) = 0;
	virtual void OnAttached() {};
	virtual void OnDetached() {};
	virtual void OnBackbufferResize() {};
	virtual void OnSceneChanged() {};
	virtual void OnEnabled() {};
	virtual void OnDisabled() {};

	void Attach(RenderGraph* renderGraph);
	void Detach();

private:

	bool m_Enabled = true;
};
}
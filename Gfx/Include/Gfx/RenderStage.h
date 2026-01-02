#pragma once

#include "Core/Math/glm_config.h"
#include "Core/Memory.h"
#include <string>
#include "RHI/ResourceState.h"
#include "Core/Math.h"

namespace st::rhi
{
	class IFramebuffer;
};

namespace st::gfx
{
class RenderStage
{
	friend class RenderView;

	struct TextureDeclaration
	{
		std::string id;
		rhi::ResourceState inputState;
		rhi::ResourceState outputState;
	};

public:

	virtual const char* GetDebugName() const = 0;

protected:

	st::weak<RenderView> m_RenderView;

private:

	virtual void Render() = 0;
	virtual void OnAttached() {};
	virtual void OnDetached() {};
	virtual void OnBackbufferResize() {};
	virtual void OnSceneChanged() {};

	void Attach(RenderView* renderView);
	void Detach();
};
}
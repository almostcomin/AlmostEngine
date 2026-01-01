#pragma once

#include "Core/Math/glm_config.h"
#include "Core/Memory.h"
#include <string>
#include "RHI/ResourceState.h"

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

	virtual bool Render() = 0;
	virtual void OnBackbufferResize(const glm::ivec2& size) = 0;

	virtual void OnSceneChanged() {};

	virtual const char* GetDebugName() const = 0;

protected:

	st::weak<RenderView> m_RenderView;

private:

	void Attach(RenderView* renderView);
	void Detach();

	virtual void OnAttached() {};
	virtual void OnDetached() {};
};
}
#pragma once

#include "Core/Math/glm_config.h"
#include "Core/Memory.h"
#include <string>
#include "RenderAPI/ResourceState.h"

namespace st::rapi
{
	class IFramebuffer;
};

namespace st::gfx
{
class RenderPass
{
	friend class RenderView;

	struct TextureDeclaration
	{
		std::string id;
		rapi::ResourceState inputState;
		rapi::ResourceState outputState;
	};

public:

	virtual bool Render() = 0;
	virtual void OnBackbufferResize(const glm::ivec2& size) = 0;

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
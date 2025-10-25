#pragma once

#include <nvrhi/nvrhi.h>

namespace st::gfx
{
	class ShaderFactory;
};

namespace st::gfx
{

class CommonResources
{
public:

	CommonResources(ShaderFactory* shaderFactory);
	~CommonResources();

	void Init();

private:

	ShaderFactory* m_ShaderFactory;
};

} // namespace st::gfx
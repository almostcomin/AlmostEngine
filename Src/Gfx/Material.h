#pragma once

#include <string>
#include <nvrhi/nvrhi.h>

namespace st::gfx
{

class Material
{
public:

	Material(const char* name = nullptr, const char* sourceFilename = nullptr) :
		m_Name{ name ? name : "<null>" },
		m_SourceFileName{ sourceFilename ? sourceFilename : "<null>" }
	{}

	void SetVertexShader(nvrhi::ShaderHandle vertexShader);
	void SetPixelShader(nvrhi::ShaderHandle pixelShader);

private:

	nvrhi::ShaderHandle m_VertexShader;
	nvrhi::ShaderHandle m_PixelShader;

	std::string m_Name;
	std::string m_SourceFileName; // where this material originated from, e.g. GLTF file name
};

}
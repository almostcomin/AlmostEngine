#pragma once

#include <string>

namespace st::gfx
{

class Material
{
public:

	Material(const char* name = nullptr, const char* sourceFilename = nullptr) :
		m_Name{ name ? name : "<null>" },
		m_SourceFileName{ sourceFilename ? sourceFilename : "<null>" }
	{}

private:

	std::string m_Name;
	std::string m_SourceFileName; // where this material originated from, e.g. GLTF file name
};

}
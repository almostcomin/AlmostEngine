#pragma once

namespace alm::gfx
{
	enum class MaterialDomain
	{
		Opaque,
		AlphaTested,
		AlphaBlended,
		Terrain,

		_Size
	};

	inline const char* GetMaterialDomainString(MaterialDomain domain)
	{
		static const char* debugStrings[] =
		{
			"Opaque",
			"AlphaTested",
			"AlphaBlended",
			"Terrain"
		};

		static_assert(std::size(debugStrings) == (int)MaterialDomain::_Size);

		return debugStrings[(int)domain];
	};

} // namespace st::gfx
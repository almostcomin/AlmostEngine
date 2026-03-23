#pragma once

namespace alm::gfx
{
	enum class MaterialDomain
	{
		Opaque,
		AlphaTested,
		AlphaBlended,

		_Size
	};

	inline const char* GetMaterialDomainString(MaterialDomain domain)
	{
		static const char* debugStrings[(int)MaterialDomain::_Size] =
		{
			"Opaque",
			"AlphaTested",
			"AlphaBlended"
		};

		return debugStrings[(int)domain];
	};

} // namespace st::gfx
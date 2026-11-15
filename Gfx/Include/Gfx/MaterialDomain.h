#pragma once

namespace alm::gfx
{
	enum class MaterialDomain
	{
		Opaque,
		AlphaTested,
		AlphaBlended,
		TransmissiveOpaque,
		TransmissiveAlphaTested,
		TransmissiveAlphaBlended,
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
			"TransmissiveOpaque",
			"TransmissiveAlphaTested",
			"TransmissiveAlphaBlended",
			"Terrain"
		};

		static_assert(std::size(debugStrings) == (int)MaterialDomain::_Size);

		return debugStrings[(int)domain];
	};

	constexpr bool IsBackToFrontSorted(MaterialDomain domain)
	{
		return
			domain == MaterialDomain::TransmissiveOpaque ||
			domain == MaterialDomain::TransmissiveAlphaTested;
			//domain == MaterialDomain::TransmissiveAlphaBlended; // TODO
	};

	constexpr bool RequiresStableSort(MaterialDomain domain)
	{
		return domain == MaterialDomain::Terrain;
	}

} // namespace st::gfx
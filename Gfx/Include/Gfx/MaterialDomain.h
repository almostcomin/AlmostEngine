#pragma once

namespace alm::gfx
{

struct MaterialDomain
{
	enum class Type
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

	static constexpr Type Opaque =					Type::Opaque;
	static constexpr Type AlphaTested =				Type::AlphaTested;
	static constexpr Type AlphaBlended =			Type::AlphaBlended;
	static constexpr Type TransmissiveOpaque =		Type::TransmissiveOpaque;
	static constexpr Type TransmissiveAlphaTested = Type::TransmissiveAlphaTested;
	static constexpr Type TransmissiveAlphaBlended = Type::TransmissiveAlphaBlended;
	static constexpr Type Terrain =					Type::Terrain;
	static constexpr Type _Size =					Type::_Size;

	static const char* GetString(MaterialDomain::Type domain)
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

		static_assert(std::size(debugStrings) == (int)Type::_Size);
		return debugStrings[(int)domain];
	}

	static constexpr bool IsBackToFrontSorted(MaterialDomain::Type domain)
	{
		return
			domain == MaterialDomain::Type::TransmissiveOpaque ||
			domain == MaterialDomain::Type::TransmissiveAlphaTested ||
			domain == MaterialDomain::Type::TransmissiveAlphaBlended;
	}

	static constexpr bool SoftwareCull(MaterialDomain::Type domain)
	{
		return
			domain == MaterialDomain::Type::TransmissiveOpaque ||
			domain == MaterialDomain::Type::TransmissiveAlphaTested ||
			domain == MaterialDomain::Type::TransmissiveAlphaBlended;
	}

	static constexpr bool IsTransmission(Type domain)
	{
		return
			domain == MaterialDomain::Type::TransmissiveOpaque ||
			domain == MaterialDomain::Type::TransmissiveAlphaTested ||
			domain == MaterialDomain::Type::TransmissiveAlphaBlended;
	}
};

} // namespace alm::gfx
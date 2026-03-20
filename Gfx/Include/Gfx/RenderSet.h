#pragma once

namespace st::gfx
{

class MeshInstance;

struct RenderSet : public st::noncopyable_nonmovable
{
	using CullSet = std::pair<rhi::CullMode, std::vector<const st::gfx::MeshInstance*>>;
	using MaterialDomainSet = std::pair<gfx::MaterialDomain, std::vector<CullSet>>;

	std::vector<MaterialDomainSet> Elements;

	auto AllInstances() const
	{
		return Elements | std::views::transform([](auto& domainSet)
		{
			return domainSet.second | std::views::transform([&domainSet](auto& cullSet) 
			{
				return cullSet.second | std::views::transform([&domainSet, &cullSet](auto* instance) 
				{
					return instance;
				});
			}) | std::views::join;
		}) | std::views::join;
	}

	auto AllInstancesWithContext() const
	{
		return Elements | std::views::transform([](auto& domainSet)
		{
			return domainSet.second | std::views::transform([&domainSet](auto& cullSet) 
			{
				return cullSet.second | std::views::transform([&domainSet, &cullSet](auto* instance) 
				{
					return std::tuple(domainSet.first, cullSet.first, instance);
				});
			}) | std::views::join;
		}) | std::views::join;
	}
};

} // namespace st::gfx
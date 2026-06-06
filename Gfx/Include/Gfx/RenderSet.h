#pragma once

#include "Gfx/Renderable.h"

namespace alm::gfx
{

struct RenderSet : public alm::noncopyable_nonmovable
{
	using CullSet = std::pair<rhi::CullMode, std::vector<RenderableDrawInfo>>;
	using MaterialDomainSet = std::pair<gfx::MaterialDomain, std::vector<CullSet>>;

	std::vector<MaterialDomainSet> Elements;

	auto AllInstances() const
	{
		return Elements | std::views::transform([](auto& domainSet)
		{
			return domainSet.second | std::views::transform([](auto& cullSet) 
			{
				return cullSet.second | std::views::transform([](const auto& drawInfo) 
				{
					return drawInfo;
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
				return cullSet.second | std::views::transform([&domainSet, &cullSet](const auto& drawInfo)
				{
					return std::tuple(domainSet.first, cullSet.first, drawInfo);
				});
			}) | std::views::join;
		}) | std::views::join;
	}
};

} // namespace st::gfx
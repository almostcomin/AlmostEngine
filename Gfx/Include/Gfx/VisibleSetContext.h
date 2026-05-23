#pragma once

namespace alm::gfx
{

class SceneHeightmap;
class HeightmapInstance;
class RenderView;

struct VisibleSetContext
{
    const std::unordered_map<const SceneHeightmap*, std::unique_ptr<HeightmapInstance>>* HeightmapInstances = nullptr;

    const RenderView* View = nullptr;
};

}
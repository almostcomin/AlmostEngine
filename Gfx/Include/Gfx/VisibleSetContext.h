#pragma once

namespace alm::gfx
{

class SceneHeightmap;
class HeightmapInstance;
class RenderView;

struct VisibleSetContext
{
    const std::unordered_map<const SceneHeightmap*, HeightmapInstance>* HeightmapInstances = nullptr;

    const RenderView* View = nullptr;
};

}
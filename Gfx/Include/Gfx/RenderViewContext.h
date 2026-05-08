#pragma once

namespace alm::gfx
{

class SceneHeightmap;
class HeightmapInstance;
class RenderView;

struct RenderViewContext
{
    const std::unordered_map<const SceneHeightmap*, HeightmapInstance>* heightmapInstances = nullptr;

    const RenderView* view = nullptr;
};

}
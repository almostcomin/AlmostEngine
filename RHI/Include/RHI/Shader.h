#pragma once

#include "RHI/Resource.h"
#include <string>
#include "Core/Common.h"
#include "Core/Blob.h"

namespace alm::rhi
{
    // Shader type mask. The values match ones used in Vulkan.
    enum class ShaderType : uint16_t
    {
        None = 0x0000,

        Compute = 0x0020,

        Vertex = 0x0001,
        Hull = 0x0002,
        Domain = 0x0004,
        Geometry = 0x0008,
        Pixel = 0x0010,
        Amplification = 0x0040,
        Mesh = 0x0080,
        AllGraphics = 0x00DF,

        RayGeneration = 0x0100,
        AnyHit = 0x0200,
        ClosestHit = 0x0400,
        Miss = 0x0800,
        Intersection = 0x1000,
        Callable = 0x2000,
        AllRayTracing = 0x3F00,

        All = 0x3FFF
    };
    ENUM_CLASS_FLAG_OPERATORS(ShaderType)

    inline const char* GetShaderTypeString(ShaderType type)
    {
        switch (type)
        {
        case ShaderType::Compute: return "Compute";
        case ShaderType::Vertex: return "Vertex";
        case ShaderType::Hull: return "Hull";
        case ShaderType::Domain: return "Domain";
        case ShaderType::Geometry: return "Geometry";
        case ShaderType::Pixel: return "Pixel";
        case ShaderType::Amplification: return "Amplification";
        case ShaderType::Mesh: return "Mesh";
        case ShaderType::RayGeneration: return "RayGeneration";
        case ShaderType::AnyHit: return "AnyHit";
        case ShaderType::ClosestHit: return "ClosestHit";
        case ShaderType::Miss: return "Miss";
        case ShaderType::Intersection: return "Intersection";
        case ShaderType::Callable: return "Callable";
        default:
            assert(0);
            return "<unknown>";
        }
    }

    struct ShaderDesc
    {
        ShaderType Type = ShaderType::None;
        std::string EntryPoint = "main";
    };

	class IShader : public IResource
	{
    public:

		virtual const ShaderDesc& GetDesc() const = 0;
		virtual const WeakBlob& GetBytecode() const = 0;

        ResourceType GetResourceType() const override { return ResourceType::Shader; }

    protected:

        IShader(Device* device, const std::string& debugName) : IResource{ device, debugName } {};
	};
}
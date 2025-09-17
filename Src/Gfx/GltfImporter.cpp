#include "Gfx/GltfImporter.h"
#include <fstream>
#include "Core/Log.h"
#include "Gfx/Material.h"
#include "Gfx/Mesh.h"
#include <glm/ext/vector_float3.hpp>
#include "Gfx/Math/aabox.h"
#include "Gfx/Math/Util.h"
#include <nvrhi/nvrhi.h>
#include "Gfx/DataUploader.h"
#include "Gfx/SceneGraph.h"
#include "Gfx/SceneGraphNode.h"
#include "Gfx/MeshInstance.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

namespace
{

struct FleContext
{
    std::vector<uint8_t> data;
};

struct IntermediateBuffers
{
    std::vector<uint32_t> indexData16;
    std::vector<uint32_t> indexData32;
    std::vector<glm::vec3> vertexPosData;
    std::vector<uint32_t> vertexNormalData; // signed normalized 8 bits per channel
    std::vector<uint32_t> vertexTangentData; // signed normalized 8 bits per channel
    std::vector<glm::vec2> vertexTexCoord1Data;
    std::vector<glm::vec2> vertexTexCoord2Data;
};

cgltf_result ReadFileCB(const struct cgltf_memory_options* memory_options,
    const struct cgltf_file_options* file_options, const char* path, cgltf_size* size, void** data)
{
    FleContext* context = (FleContext*)file_options->user_data;
    if (!context)
    {
        st::log::Warning("cgltf read: no context for file {} provided.", path);
        return cgltf_result_invalid_options;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        st::log::Warning("cgltf read: file {} not found.", path);
        return cgltf_result_file_not_found;
    }
    file.seekg(0, std::ios::end);
    uint64_t fsize = file.tellg();
    file.seekg(0, std::ios::beg);

    context->data.resize(fsize);
    file.read((char*)context->data.data(), fsize);

    if (size) *size = fsize;
    if (data) *data = context->data.data();

    return cgltf_result_success;
}

void ReleaseFileCB(const struct cgltf_memory_options*, const struct cgltf_file_options*, void*)
{
    // no-op
}

const char* ErrorToString(cgltf_result res)
{
    switch (res)
    {
    case cgltf_result_success:
        return "Success";
    case cgltf_result_data_too_short:
        return "Data is too short";
    case cgltf_result_unknown_format:
        return "Unknown format";
    case cgltf_result_invalid_json:
        return "Invalid JSON";
    case cgltf_result_invalid_gltf:
        return "Invalid glTF";
    case cgltf_result_invalid_options:
        return "Invalid options";
    case cgltf_result_file_not_found:
        return "File not found";
    case cgltf_result_io_error:
        return "I/O error";
    case cgltf_result_out_of_memory:
        return "Out of memory";
    case cgltf_result_legacy_gltf:
        return "Legacy glTF";
    default:
        return "Unknown error";
    }
}

std::pair<const uint8_t*, size_t> BufferIterator(const cgltf_accessor& accessor, size_t defaultStride)
{
    // TODO: sparse accessor support
    const cgltf_buffer_view* view = accessor.buffer_view;
    const uint8_t* data = (uint8_t*)view->buffer->data + view->offset + accessor.offset;
    const size_t stride = view->stride ? view->stride : defaultStride;
    return std::make_pair(data, stride);
}

std::unordered_map<const cgltf_material*, std::shared_ptr<st::gfx::Material>> GetMaterialsMap(const cgltf_data* objects, const char* filename)
{
    std::unordered_map<const cgltf_material*, std::shared_ptr<st::gfx::Material>> matMap;

    for (size_t mat_idx = 0; mat_idx < objects->materials_count; mat_idx++)
    {
        const cgltf_material& srcMat = objects->materials[mat_idx];
        std::shared_ptr<st::gfx::Material> mat =
            std::make_shared<st::gfx::Material>(srcMat.name, filename);
        // TODO

        matMap[&srcMat] = mat;
    }

    return matMap;
}

void GetIndexVertexCount(const cgltf_data* objects, size_t& out_totalIndices, size_t& out_totalVertices, size_t& out_morphTargetTotalVertices, bool& out_hasJoints)
{
    out_totalIndices = 0;
    out_totalVertices = 0;
    out_morphTargetTotalVertices = 0;
    out_hasJoints = false;

    for (size_t mesh_idx = 0; mesh_idx < objects->meshes_count; mesh_idx++)
    {
        const cgltf_mesh& mesh = objects->meshes[mesh_idx];

        for (size_t prim_idx = 0; prim_idx < mesh.primitives_count; prim_idx++)
        {
            const cgltf_primitive& prim = mesh.primitives[prim_idx];

            if ((prim.type != cgltf_primitive_type_triangles &&
                prim.type != cgltf_primitive_type_line_strip &&
                prim.type != cgltf_primitive_type_lines) ||
                prim.attributes_count == 0)
            {
                continue;
            }

            if (prim.indices)
                out_totalIndices += prim.indices->count;
            else
                out_totalIndices += prim.attributes->data->count;
            out_totalVertices += prim.attributes->data->count;

            if (!out_hasJoints)
            {
                // Detect if the primitive has joints or weights attributes.
                for (size_t attr_idx = 0; attr_idx < prim.attributes_count; attr_idx++)
                {
                    const cgltf_attribute& attr = prim.attributes[attr_idx];
                    if (attr.type == cgltf_attribute_type_joints || attr.type == cgltf_attribute_type_weights)
                    {
                        out_hasJoints = true;
                        break;
                    }
                }
            }
        }
    }
}

template<typename T>
void CollectPrimitiveIndices(const cgltf_primitive& prim, const cgltf_accessor& positions, std::vector<uint8_t>& out_indexData)
{
    static_assert(std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>, "Only 16 or 32 bit indices are allowed");

    size_t indexCount = 0;
    if (prim.indices)
    {
        out_indexData.resize(prim.indices->count * sizeof(T));

        // copy the indices
        auto [indexSrc, indexStride] = BufferIterator(*prim.indices, 0);
        T* indexDst = (T*)out_indexData.data();

        switch (prim.indices->component_type)
        {
        case cgltf_component_type_r_8u:
            if (!indexStride) indexStride = sizeof(uint8_t);
            for (size_t i_idx = 0; i_idx < prim.indices->count; i_idx++)
            {
                *indexDst = T(*(const uint8_t*)indexSrc);
                indexSrc += indexStride;
                indexDst++;
            }
            break;

        case cgltf_component_type_r_16u:
            if (!indexStride) indexStride = sizeof(uint16_t);
            for (size_t i_idx = 0; i_idx < prim.indices->count; i_idx++)
            {
                *indexDst = T(*(const uint16_t*)indexSrc);
                indexSrc += indexStride;
                indexDst++;
            }
            break;

        case cgltf_component_type_r_32u:
            if (!indexStride) indexStride = sizeof(uint32_t);
            for (size_t i_idx = 0; i_idx < prim.indices->count; i_idx++)
            {
                *indexDst = T(*(const uint32_t*)indexSrc);
                indexSrc += indexStride;
                indexDst++;
            }
            break;
        default:
            assert(false);
        }
    }
    else
    {
        // generate the indices
        T* indexDst = (T*)out_indexData.data();
        for (size_t i_idx = 0; i_idx < prim.indices->count; i_idx++)
        {
            *indexDst = (T)i_idx;
            indexDst++;
        }
    }
}

void CollectPrimitivePositions(const cgltf_accessor& positions, std::vector<glm::vec3>& out_vertexPosData, st::math::aabox3f& out_bounds)
{
    out_bounds.Reset();
    out_vertexPosData.resize(positions.count);

    auto [posSrc, posStride] = BufferIterator(positions, sizeof(float) * 3);
    glm::vec3* posDst = out_vertexPosData.data();

    for (size_t v_idx = 0; v_idx < positions.count; v_idx++)
    {
        *posDst = *(const glm::vec3*)posSrc;
        out_bounds.Merge(*posDst);

        posSrc += posStride;
        ++posDst;
    }
}

void CollectPrimitiveNormals(const cgltf_accessor& normals, std::vector<uint32_t>& out_vertexNormalData)
{
    out_vertexNormalData.resize(normals.count);

    auto [normalSrc, normalStride] = BufferIterator(normals, sizeof(float) * 3);
    uint32_t* normalDst = out_vertexNormalData.data();

    for (size_t v_idx = 0; v_idx < normals.count; v_idx++)
    {
        const glm::vec3* normal = (const glm::vec3*)normalSrc;
        *normalDst = st::math::VectorToSnorm8(*normal);

        normalSrc += normalStride;
        ++normalDst;
    }
}

void CollectPrimitiveTangents(const cgltf_accessor& tangents, std::vector<uint32_t>& out_tangentNormalData)
{
    out_tangentNormalData.resize(tangents.count);

    auto [tangentSrc, tangentStride] = BufferIterator(tangents, sizeof(float) * 3);
    uint32_t* tangentDst = out_tangentNormalData.data();

    for (size_t v_idx = 0; v_idx < tangents.count; v_idx++)
    {
        const glm::vec3* tangent = (const glm::vec3*)tangentSrc;
        *tangentDst = st::math::VectorToSnorm8(*tangent);

        tangentSrc += tangentStride;
        ++tangentDst;
    }
}

void CollectPrimitiveTexcoords(const cgltf_accessor& texcoords, std::vector<glm::vec2>& out_vertexTexCoordData)
{
    out_vertexTexCoordData.resize(texcoords.count);

    auto [texcoordSrc, texcoordStride] = BufferIterator(texcoords, sizeof(float) * 2);
    glm::vec2* texcoordDst = out_vertexTexCoordData.data();

    for (size_t v_idx = 0; v_idx < texcoords.count; v_idx++)
    {
        *texcoordDst = *(const glm::vec2*)texcoordSrc;

        texcoordSrc += texcoordStride;
        ++texcoordDst;
    }
}

std::expected<std::pair<nvrhi::BufferHandle, nvrhi::EventQueryHandle>, std::string>
CreateIndexBuffer(std::vector<uint8_t>&& indexData, bool idx32bits, const char* debugName, st::gfx::DataUploader* dataUploader, nvrhi::IDevice* device)
{
    nvrhi::BufferDesc bufferDesc;
    bufferDesc.isIndexBuffer = true;
    bufferDesc.byteSize = indexData.size();
    bufferDesc.debugName = "IndexBuffer";
    bufferDesc.canHaveTypedViews = true;
    bufferDesc.canHaveRawViews = true;
    bufferDesc.format = idx32bits ? nvrhi::Format::R32_UINT : nvrhi::Format::R16_UINT;
    bufferDesc.isAccelStructBuildInput = false; // TODO

    nvrhi::BufferHandle indexBuffer = device->createBuffer(bufferDesc);
    auto uploadResult = dataUploader->UploadData(
        indexBuffer,
        nvrhi::ResourceStates::Common,
        nvrhi::ResourceStates::IndexBuffer | nvrhi::ResourceStates::ShaderResource,
        std::move(indexData),
        0,
        indexData.size(),
        debugName);

    if (uploadResult)
    {
        return std::make_pair(indexBuffer, *uploadResult);
    }
    else
    {
        return std::unexpected("Failed upload data to vertex buffer");
    }
}

std::expected<std::pair<nvrhi::BufferHandle, nvrhi::EventQueryHandle>, std::string>
CreateVertexBuffer(std::vector<uint8_t>&& vertexData, const char* debugName, st::gfx::DataUploader* dataUploader, nvrhi::IDevice* device)
{
    nvrhi::BufferDesc bufferDesc;
    bufferDesc.isVertexBuffer = true;
    bufferDesc.byteSize = vertexData.size();
    bufferDesc.debugName = "VertexBuffer";
    bufferDesc.canHaveTypedViews = true;
    bufferDesc.canHaveRawViews = true;
    bufferDesc.isAccelStructBuildInput = false; // TODO

    auto vertexBuffer = device->createBuffer(bufferDesc);
    auto uploadResult = dataUploader->UploadData(
        vertexBuffer,
        nvrhi::ResourceStates::Common,
        nvrhi::ResourceStates::VertexBuffer | nvrhi::ResourceStates::ShaderResource,
        std::move(vertexData),
        0,
        vertexData.size(),
        debugName);

    if (uploadResult)
    {
        return std::make_pair(vertexBuffer, *uploadResult);
    }
    else
    {
        return std::unexpected("Failed upload data to vertex buffer");
    }
}

std::expected<std::unordered_map<const cgltf_mesh*, std::shared_ptr<st::gfx::Mesh>>, std::string>
LoadMeshes(const cgltf_data* objects, std::unordered_map<const cgltf_material*, std::shared_ptr<st::gfx::Material>>& matMap, 
    const char* filename, st::gfx::DataUploader* dataUploader, nvrhi::IDevice* device, std::vector<nvrhi::EventQueryHandle>& out_handlesToWait)
{
    std::unordered_map<const cgltf_mesh*, std::shared_ptr<st::gfx::Mesh>> meshMap;

    for (size_t mesh_idx = 0; mesh_idx < objects->meshes_count; mesh_idx++)
    {
        const cgltf_mesh& srcMesh = objects->meshes[mesh_idx];
        for (size_t prim_idx = 0; prim_idx < srcMesh.primitives_count; prim_idx++)
        {
            const cgltf_primitive& prim = srcMesh.primitives[prim_idx];

            // Only triangle list for the moment
            if (prim.type != cgltf_primitive_type_triangles ||
                prim.attributes_count == 0)
            {
                continue;
            }

            if (prim.indices)
            {
                assert(prim.indices->component_type == cgltf_component_type_r_32u ||
                    prim.indices->component_type == cgltf_component_type_r_16u ||
                    prim.indices->component_type == cgltf_component_type_r_8u);
                assert(prim.indices->type == cgltf_type_scalar);
            }

            const cgltf_accessor* positions = nullptr;
            const cgltf_accessor* normals = nullptr;
            const cgltf_accessor* tangents = nullptr;
            const cgltf_accessor* texcoords = nullptr;
            const cgltf_accessor* joint_weights = nullptr;
            const cgltf_accessor* joint_indices = nullptr;
            const cgltf_accessor* radius = nullptr;

            for (size_t attr_idx = 0; attr_idx < prim.attributes_count; attr_idx++)
            {
                const cgltf_attribute& attr = prim.attributes[attr_idx];

                switch (attr.type)
                {
                case cgltf_attribute_type_position:
                    assert(attr.data->type == cgltf_type_vec3);
                    assert(attr.data->component_type == cgltf_component_type_r_32f);
                    positions = attr.data;
                    break;
                case cgltf_attribute_type_normal:
                    assert(attr.data->type == cgltf_type_vec3);
                    assert(attr.data->component_type == cgltf_component_type_r_32f);
                    normals = attr.data;
                    break;
                case cgltf_attribute_type_tangent:
                    assert(attr.data->type == cgltf_type_vec4);
                    assert(attr.data->component_type == cgltf_component_type_r_32f);
                    tangents = attr.data;
                    break;
                case cgltf_attribute_type_texcoord:
                    assert(attr.data->type == cgltf_type_vec2);
                    assert(attr.data->component_type == cgltf_component_type_r_32f);
                    if (attr.index == 0)
                        texcoords = attr.data;
                    break;
                case cgltf_attribute_type_joints:
                    assert(attr.data->type == cgltf_type_vec4);
                    assert(attr.data->component_type == cgltf_component_type_r_8u || attr.data->component_type == cgltf_component_type_r_16u);
                    joint_indices = attr.data;
                    break;
                case cgltf_attribute_type_weights:
                    assert(attr.data->type == cgltf_type_vec4);
                    assert(attr.data->component_type == cgltf_component_type_r_8u || attr.data->component_type == cgltf_component_type_r_16u || attr.data->component_type == cgltf_component_type_r_32f);
                    joint_weights = attr.data;
                    break;
                case cgltf_attribute_type_custom:
                    if (strncmp(attr.name, "_RADIUS", 7) == 0)
                    {
                        assert(attr.data->type == cgltf_type_scalar);
                        assert(attr.data->component_type == cgltf_component_type_r_32f);
                        radius = attr.data;
                    }
                    break;
                default:
                    break;
                }
            }
            // Positions at least
            assert(positions);

            // Indices
            bool idx32bits = false;
            if (prim.indices)
            {
                idx32bits = (prim.indices->component_type == cgltf_component_type_r_32u);
            }
            else
            {
                idx32bits = positions->count > std::numeric_limits<uint16_t>::max();
            }
            std::vector<uint8_t> indexData;
            if (idx32bits)
            {
                CollectPrimitiveIndices<uint32_t>(prim, *positions, indexData);
            }
            else
            {
                CollectPrimitiveIndices<uint16_t>(prim, *positions, indexData);
            }

            // Positions and bounds
            std::vector<glm::vec3> vertexPosData;
            st::math::aabox3f bounds;
            CollectPrimitivePositions(*positions, vertexPosData, bounds);

            // TODO: radius

            std::vector<uint32_t> vertexNormalData;
            if (normals)
            {
                assert(normals->count == positions->count);
                CollectPrimitiveNormals(*normals, vertexNormalData);
            }

            std::vector<uint32_t> vertexTangentData;
            if (tangents)
            {
                assert(tangents->count == positions->count);
                CollectPrimitiveTangents(*tangents, vertexTangentData);
            }

            std::vector<glm::vec2> vertexTexCoordData;
            if (texcoords)
            {
                assert(texcoords->count == positions->count);
                CollectPrimitiveTexcoords(*texcoords, vertexTexCoordData);
            }

            // TODO: joint_indices
            // TODO: joint_weights

            // Create mesh
            auto mesh = std::make_shared<st::gfx::Mesh>(srcMesh.name, filename);

            // Assign material
            if (prim.material)
            {
                mesh->SetMaterial(matMap[prim.material]);
            }
            else
            {
                st::log::Warning("Geometry {} for mesh {} doesn't have a material.", prim_idx, srcMesh.name);
            }

            // Index buffer
            auto indexBufferResult = CreateIndexBuffer(std::move(indexData), idx32bits, srcMesh.name, dataUploader, device);
            if (indexBufferResult)
            {
                mesh->SetIndexBuffer(indexBufferResult->first);
                out_handlesToWait.push_back(indexBufferResult->second);
            }

            // Vertex buffer
            int vertexStride = 0;
            constexpr int posElemSize = sizeof(decltype(vertexPosData)::value_type);
            if (!vertexPosData.empty())
                vertexStride += posElemSize;
            constexpr int normalElemSize = sizeof(decltype(vertexNormalData)::value_type);
            if (!vertexNormalData.empty())
                vertexStride += normalElemSize;
            constexpr int tangentElemSize = sizeof(decltype(vertexTangentData)::value_type);
            if (!vertexTangentData.empty())
                vertexStride += tangentElemSize;
            constexpr int texCoordElemSize = sizeof(decltype(vertexTexCoordData)::value_type);
            if (!vertexTexCoordData.empty())
                vertexStride += texCoordElemSize;

            std::vector<uint8_t> vertexData;
            vertexData.resize(vertexStride * vertexPosData.size());

            // Interleave
            int vertexOffset = 0;
            // Positions
            if (!vertexPosData.empty())
            {
                uint8_t* dstData = vertexData.data() + vertexOffset;
                for (size_t v_idx = 0; v_idx < vertexPosData.size(); v_idx++)
                {
                    *(decltype(vertexPosData)::value_type*)dstData = vertexPosData[v_idx];
                    dstData += vertexStride;
                }
                vertexOffset += posElemSize;
            }
            // Normals
            if (!vertexNormalData.empty())
            {
                uint8_t* dstData = vertexData.data() + vertexOffset;
                for (size_t v_idx = 0; v_idx < vertexNormalData.size(); v_idx++)
                {
                    *(decltype(vertexNormalData)::value_type*)dstData = vertexNormalData[v_idx];
                    dstData += vertexStride;
                }
                vertexOffset += normalElemSize;
            }
            // Tangents
            if (!vertexTangentData.empty())
            {
                uint8_t* dstData = vertexData.data() + vertexOffset;
                for (size_t v_idx = 0; v_idx < vertexTangentData.size(); v_idx++)
                {
                    *(decltype(vertexTangentData)::value_type*)dstData = vertexTangentData[v_idx];
                    dstData += vertexStride;
                }
                vertexOffset += tangentElemSize;
            }
            // TexCoords
            if (!vertexTexCoordData.empty())
            {
                uint8_t* dstData = vertexData.data() + vertexOffset;
                for (size_t v_idx = 0; v_idx < vertexTexCoordData.size(); v_idx++)
                {
                    *(decltype(vertexTexCoordData)::value_type*)dstData = vertexTexCoordData[v_idx];
                    dstData += vertexStride;
                }
                vertexOffset += texCoordElemSize;
            }

            // Create vertex buffer
            auto vertexBufferResult = CreateVertexBuffer(std::move(vertexData), srcMesh.name, dataUploader, device);
            if (vertexBufferResult)
            {
                mesh->SetVertexBuffer(vertexBufferResult->first);
                out_handlesToWait.push_back(vertexBufferResult->second);
            }

            meshMap[&srcMesh] = mesh;
        }
    }

    return meshMap;
}

} // anonymous namespace

// ---------------------------------------------------------------------------------------------------------------------------------------

namespace st::gfx
{

std::expected<std::unique_ptr<st::gfx::SceneGraph>, std::string> 
ImportGlTF(const char* path, st::gfx::DataUploader* dataUploader, nvrhi::IDevice* device)
{
    FleContext fileContext;

    cgltf_options options{};
    options.file.read = &ReadFileCB;
    options.file.release = &ReleaseFileCB;
    options.file.user_data = &fileContext;

    cgltf_data* objects = nullptr;
    cgltf_result res = cgltf_parse_file(&options, path, &objects);
    if (res != cgltf_result_success)
    {
        st::log::Error("Couldn't load glTF file '{}': {}", path, ErrorToString(res));
        return std::unexpected(ErrorToString(res));
    }

    res = cgltf_load_buffers(&options, objects, path);
    if (res != cgltf_result_success)
    {
        st::log::Error("Failed to load buffers for glTF file '%s': ", path, ErrorToString(res));
        return std::unexpected(ErrorToString(res));
    }

    // Materials
    auto matMap = GetMaterialsMap(objects, path);

    // Meshes
    std::vector<nvrhi::EventQueryHandle> handlesToWait;
    auto loadMeshesResult = LoadMeshes(objects, matMap, path, dataUploader, device, handlesToWait);
    auto meshMap = *loadMeshesResult;

    // Wait uploads
    for (const auto& ev : handlesToWait)
    {
        device->waitEventQuery(ev);
    }
    
    // Build scene
    assert(objects->scenes_count == 1); // only 1 scene allowed

    std::vector<std::pair<const cgltf_node*, std::shared_ptr<st::gfx::SceneGraphNode>>> stack;
    std::shared_ptr<st::gfx::SceneGraphNode> root;

    const cgltf_node* srcNode = *objects->scene->nodes;
    while (srcNode)
    {
        auto dstNode = std::make_shared<SceneGraphNode>();

        if (srcNode->has_matrix)
        {
            dstNode->SetTransform(st::gfx::Transform{ glm::make_mat4(srcNode->matrix) }); // TODO check if transpose is needed (glm::transpose)
        }
        else
        {
            st::gfx::Transform t;
            if (srcNode->has_scale)
                t.SetScale(*(glm::vec3*)srcNode->scale);
            if (srcNode->has_rotation)
                t.SetRotation(glm::quat{ srcNode->rotation[3], srcNode->rotation[0], srcNode->rotation[1], srcNode->rotation[2] });
            if (srcNode->translation)
                t.SetTranslation(*(glm::vec3*)srcNode->translation);
            dstNode->SetTransform(t);
        }

        if (srcNode->name)
            dstNode->SetName(srcNode->name);

        if (srcNode->mesh)
        {
            auto found = meshMap.find(srcNode->mesh);
            if (found != meshMap.end())
            {
                auto leaf = std::make_shared<st::gfx::MeshInstance>(found->second);
                dstNode->SetLeaf(leaf);
            }
            else
            {
                st::log::Error("Could not find mesh for node '{}', from mesh '{}', from file '{}'",
                    srcNode->name ? srcNode->name : "<noname>",
                    srcNode->mesh->name ? srcNode->mesh->name : "<noname>",
                    path);
            }
        }

        // Do we have parent? Then bind.
        if (!stack.empty())
        {
            stack.back().second->AddChild(dstNode);
            dstNode->SetParent(stack.back().second);
        }
        // Else, we are the root
        else
        {
            assert(!root);
            root = dstNode;
        }

        // Do we have children? Then push ourshelve to the stack
        // and the first child is the next node to process
        if (srcNode->children_count)
        {
            dstNode->ReserveChildCount(srcNode->children_count);
            stack.emplace_back(srcNode, dstNode);
            srcNode = srcNode->children[0];
        }
        else
        {
            const cgltf_node* parentNode = stack.empty() ? nullptr : stack.back().first;
            if (parentNode && parentNode->children[parentNode->children_count - 1] != srcNode)
            {
                srcNode++; // next sibling;
            }
            else
            {
                // If we are the last child, go up the stack finding any node still to process
                while (parentNode && parentNode->children[parentNode->children_count - 1] == srcNode)
                {
                    srcNode = parentNode;
                    stack.pop_back();
                    parentNode = stack.empty() ? nullptr : stack.back().first;
                }
                if (stack.empty())
                {
                    srcNode = nullptr; // finished
                }
                else
                {
                    srcNode++; // next sibling;
                }
            }
        }        
    }

    auto* graph = new st::gfx::SceneGraph(root);
    return std::unique_ptr<st::gfx::SceneGraph>{graph};
}

} // namespace st::gfx
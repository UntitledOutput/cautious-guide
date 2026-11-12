#include "graphics/model.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include "core/file.hpp"

using namespace graphics;

namespace
{
    GLenum gl_format(TextureFormat f)
    {
        switch (f)
        {
            case TextureFormat::R8:
                return GL_RED;
            case TextureFormat::RG8:
                return GL_RG;
            case TextureFormat::RGB8:
                return GL_RGB;
            case TextureFormat::RGBA8:
                return GL_RGBA;
            default:
                throw_error("compressed textures aren't supported yet");
        }
    }

    void generate_normals(std::vector<Vertex>& verts, std::span<const u32> idx)
    {
        for (size_t i = 0; i + 2 < idx.size(); i += 3)
        {
            Vertex& a = verts[idx[i]];
            Vertex& b = verts[idx[i + 1]];
            Vertex& c = verts[idx[i + 2]];
            math::vec3 n = cross(b.position - a.position, c.position - a.position); // length = area weight
            a.normal += n;
            b.normal += n;
            c.normal += n;
        }
        for (Vertex& v : verts)
            v.normal = dot(v.normal, v.normal) > 0.0f ? normalize(v.normal) : math::vec3{0.0f, 1.0f, 0.0f};
    }

    MeshHandle upload_mesh(RenderMgr& r, const MeshData& m)
    {
        const size_t n = m.positions.size();
        for (u32 i : m.indices)
            if (i >= n)
                throw_error("mesh '" + m.name + "' has an index past its vertex count");

        std::vector<Vertex> verts(n);
        for (size_t i = 0; i < n; ++i)
            verts[i].position = m.positions[i];

        if (m.normals.size() == n)
            for (size_t i = 0; i < n; ++i)
                verts[i].normal = m.normals[i];
        else
            generate_normals(verts, m.indices);

        VertexFormat fmt = VertexFormat::Position | VertexFormat::Normal;
        if (m.uvs[0].size() == n)
        {
            fmt |= VertexFormat::UV0;
            for (size_t i = 0; i < n; ++i)
                verts[i].uv0 = m.uvs[0][i];
        }

        if (m.joints.size() == n)
        {
            fmt |= VertexFormat::Joints;
            for (size_t i = 0; i < n; ++i)
                std::copy(std::begin(m.joints[i]), std::end(m.joints[i]), std::begin(verts[i].joints));
        }

        if (m.weights.size() == n)
        {
            fmt |= VertexFormat::Weights;
            for (size_t i = 0; i < n; ++i)
                verts[i].weights = m.weights[i];
        }

        return r.create_mesh({fmt, verts, m.indices});
    }
} // namespace

ModelHandle ModelCache::load(const std::string& path)
{
    const std::string key = std::filesystem::path(path).lexically_normal().generic_string();
    if (auto it = byPath.find(key); it != byPath.end())
        return it->second;

    if (!resolver)
        throw_error("set a material resolver before loading models");

    assets.push_back(upload(read(key)));
    const ModelHandle h = ModelHandle(assets.size() - 1);
    byPath.emplace(key, h);
    return h;
}

ModelHandle ModelCache::generate(std::string key,const ModelData& data)
{
    if (!resolver)
        throw_error("set a material resolver before loading models");

    assets.push_back(upload(data));
    const ModelHandle h = ModelHandle(assets.size() - 1);
    byPath.emplace(key, h);
    return h;
}

ModelData ModelCache::read(const std::string& path)
{
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });

    for (const auto& loader : loaders)
    {
        if (!loader->handles(ext))
            continue;

        LoadContext ctx;
        ctx.baseDir = std::filesystem::path(path).parent_path().string();
        ctx.read = [this](const std::string& p) { return fs.read_file(p); };
        return loader->load(fs.read_file(path), ctx);
    }
    throw_error("no model loader for " + path);
}

ModelAsset ModelCache::upload(const ModelData& data)
{
    for (size_t i = 0; i < data.nodes.size(); ++i)
    {
        if (data.nodes[i].parent >= int(i))
            throw_error("model nodes aren't parent-first");
        for (u32 mi : data.nodes[i].meshes)
            if (mi >= data.meshes.size())
                throw_error("node references a missing mesh");
    }
    for (const MeshData& m : data.meshes)
        if (m.material >= data.materials.size())
            throw_error("mesh references a missing material");

    std::vector<TextureHandle> textures;
    textures.reserve(data.textures.size());
    for (const TextureData& t : data.textures)
    {
        if (t.mips.empty() || t.width == 0 || t.height == 0)
        {
            textures.push_back(renderer.white_texture());
            continue;
        }
        textures.push_back(
            renderer.create_texture({int(t.width), int(t.height), t.mips[0], gl_format(t.format), t.srgb}));
    }

    ModelAsset asset;
    for (const MaterialData& m : data.materials)
        asset.materials.push_back(resolver(m, textures));

    std::vector<MeshHandle> meshes;
    meshes.reserve(data.meshes.size());
    for (const MeshData& m : data.meshes)
        meshes.push_back(upload_mesh(renderer, m));

    for (u32 ni = 0; ni < data.nodes.size(); ++ni)
        for (u32 mi : data.nodes[ni].meshes)
            asset.parts.push_back({meshes[mi], data.meshes[mi].material, ni,data.nodes[ni].skin});

    for (const SkinData& s : data.skins)
    {
        if (s.inverseBind.size() != s.joints.size())
            throw_error("skin has mismatched inverse bind count");
        for (u32 j : s.joints)
            if (j >= data.nodes.size())
                throw_error("skin references a missing joint node");
    }
    for (const NodeData& n : data.nodes)
        if (n.skin >= int(data.skins.size()))
            throw_error("node references a missing skin");
    for (const auto& a : data.animations)
        for (const auto& c : a.channels)
            if (c.node >= data.nodes.size())
                throw_error("animation targets a missing node");

    asset.nodes = data.nodes;
    asset.animations = data.animations;
    asset.skins = data.skins;
    asset.restWorld.resize(asset.nodes.size());
    compute_node_world(asset.nodes, asset.restWorld);
    return asset;
}

void graphics::compute_node_world(std::span<const NodeData> nodes, std::span<math::mat4> out)
{
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const NodeData& n = nodes[i];
        math::mat4 local = math::translate(n.translation) * math::to_mat4(n.rotation) * math::scale(n.scale);
        out[i] = n.parent < 0 ? local : out[size_t(n.parent)] * local;
    }
}

void graphics::submit_model(RenderMgr& r, const ModelAsset& model, const math::mat4& world,
                            std::span<const MaterialHandle> overrides, std::span<const math::mat4> pose)
{
    const std::span<const math::mat4> nodes = pose.empty() ? std::span<const math::mat4>(model.restWorld) : pose;

    for (const auto& part : model.parts)
    {
        MaterialHandle mat = model.materials[part.materialSlot];
        if (part.materialSlot < overrides.size() && overrides[part.materialSlot] != invalid_material)
            mat = overrides[part.materialSlot];

        DrawCall dc{};
        dc.mesh = part.mesh;
        dc.material = mat;
        if (part.skin >= 0)
        {
            const SkinData& skin = model.skins[size_t(part.skin)];
            std::vector<math::mat4> joints(skin.joints.size());
            for (size_t i = 0; i < joints.size(); ++i)
                joints[i] = nodes[skin.joints[i]] * skin.inverseBind[i];

            dc.jointOffset = r.push_joints(joints);
            dc.jointCount = u32(joints.size());
            dc.model = world;
        }
        else
        {
            dc.model = world * nodes[part.node];
        }

        r.submit_draw_call(dc);
    }
}

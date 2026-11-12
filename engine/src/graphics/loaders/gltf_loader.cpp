#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <cstring>
#include <numeric>
#include <unordered_map>
#include "graphics/image.hpp"
#include "graphics/model.hpp"

using namespace graphics;

namespace
{
    bool load_image(tinygltf::Image* image, const int, std::string*, std::string* warn, int, int,
                    const unsigned char* bytes, int size, void*)
    {
        DecodedImage img;
        if (!decode_image({bytes, size_t(size)}, img))
        {
            // leave it empty, the cache swaps in white
            if (warn)
                *warn += "couldn't decode image '" + image->name + "'\n";
            return true;
        }

        image->width = int(img.width);
        image->height = int(img.height);
        image->component = 4;
        image->bits = 8;
        image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
        image->image = std::move(img.rgba);
        return true;
    }

    float read_component(const byte* p, int type, bool normalized)
    {
        switch (type)
        {
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                {
                    float v;
                    std::memcpy(&v, p, 4);
                    return v;
                }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                return normalized ? *p / 255.0f : float(*p);
            case TINYGLTF_COMPONENT_TYPE_BYTE:
                {
                    int8_t v;
                    std::memcpy(&v, p, 1);
                    return normalized ? std::max(v / 127.0f, -1.0f) : float(v);
                }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                {
                    u16 v;
                    std::memcpy(&v, p, 2);
                    return normalized ? v / 65535.0f : float(v);
                }
            case TINYGLTF_COMPONENT_TYPE_SHORT:
                {
                    int16_t v;
                    std::memcpy(&v, p, 2);
                    return normalized ? std::max(v / 32767.0f, -1.0f) : float(v);
                }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                {
                    u32 v;
                    std::memcpy(&v, p, 4);
                    return float(v);
                }
        }
        throw_error("gltf: unknown component type");
    }

    struct Importer
    {
        const tinygltf::Model& gm;
        ModelData out;
        u32 defaultMaterial = 0;
        std::unordered_map<int, std::vector<u32>> meshParts;

        const byte* accessor_data(const tinygltf::Accessor& acc, size_t& stride, size_t elemSize)
        {
            if (acc.sparse.isSparse)
                throw_error("gltf: sparse accessors aren't supported");
            if (acc.bufferView < 0)
                return nullptr;

            const auto& view = gm.bufferViews[acc.bufferView];
            const auto& buf = gm.buffers[view.buffer].data;
            const int s = acc.ByteStride(view);
            if (s <= 0)
                throw_error("gltf: bad accessor stride");
            stride = size_t(s);

            const size_t start = view.byteOffset + acc.byteOffset;
            if (acc.count > 0 && start + (acc.count - 1) * stride + elemSize > buf.size())
                throw_error("gltf: accessor runs past its buffer");
            return buf.data() + start;
        }

        template <int N>
        std::vector<std::array<float, N>> read_floats(int accessor, std::array<float, N> fill = {})
        {
            const auto& acc = gm.accessors[accessor];
            const int compSize = tinygltf::GetComponentSizeInBytes(acc.componentType);
            const int stored = tinygltf::GetNumComponentsInType(acc.type);

            std::vector<std::array<float, N>> result(acc.count, fill);
            size_t stride = 0;
            const byte* base = accessor_data(acc, stride, size_t(compSize) * stored);
            if (!base)
                return result;

            const int n = std::min(N, stored);
            for (size_t i = 0; i < acc.count; ++i)
                for (int c = 0; c < n; ++c)
                    result[i][c] = read_component(base + i * stride + c * compSize, acc.componentType, acc.normalized);
            return result;
        }

        std::vector<u32> read_indices(int accessor)
        {
            const auto& acc = gm.accessors[accessor];
            if (acc.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE &&
                acc.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT &&
                acc.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                throw_error("gltf: bad index type");

            const int size = tinygltf::GetComponentSizeInBytes(acc.componentType);
            size_t stride = 0;
            const byte* base = accessor_data(acc, stride, size_t(size));
            if (!base)
                throw_error("gltf: index accessor has no data");

            std::vector<u32> result(acc.count);
            for (size_t i = 0; i < acc.count; ++i)
            {
                u32 v = 0;
                std::memcpy(&v, base + i * stride, size_t(size)); // little endian
                result[i] = v;
            }
            return result;
        }

        void read_textures()
        {
            for (const auto& tex : gm.textures)
            {
                TextureData t;
                if (tex.source >= 0)
                {
                    const auto& img = gm.images[tex.source];
                    t.name = img.name;
                    if (!img.image.empty())
                    {
                        t.width = u32(img.width);
                        t.height = u32(img.height);
                        t.format = TextureFormat::RGBA8;
                        t.mips.push_back(img.image);
                    }
                }
                out.textures.push_back(std::move(t));
            }
        }

        void read_materials()
        {
            for (const auto& gmat : gm.materials)
            {
                MaterialData m;
                m.name = gmat.name;
                m.shader = "gltf_pbr";

                const auto& pbr = gmat.pbrMetallicRoughness;
                const auto& c = pbr.baseColorFactor;
                const auto& e = gmat.emissiveFactor;
                m.params["baseColor"] = math::vec4{float(c[0]), float(c[1]), float(c[2]), float(c[3])};
                m.params["emissive"] = math::vec3{float(e[0]), float(e[1]), float(e[2])};
                m.params["metallic"] = float(pbr.metallicFactor);
                m.params["roughness"] = float(pbr.roughnessFactor);

                auto tex = [&](int index, const char* sampler, bool srgb)
                {
                    if (index < 0 || size_t(index) >= out.textures.size())
                        return;
                    m.textures[sampler] = u32(index);
                    out.textures[size_t(index)].srgb |= srgb;
                };
                tex(pbr.baseColorTexture.index, "_a0", true);
                tex(gmat.normalTexture.index, "_n0", false);
                tex(gmat.emissiveTexture.index, "_e0", true);

                if (gmat.alphaMode == "MASK")
                    m.params["alphaCutoff"] = float(gmat.alphaCutoff);
                m.state.blend = gmat.alphaMode == "BLEND";
                m.state.depthWrite = !m.state.blend;
                m.state.cullBack = !gmat.doubleSided;
                out.materials.push_back(std::move(m));
            }

            defaultMaterial = u32(out.materials.size());
            MaterialData fallback;
            fallback.name = "default";
            fallback.shader = "gltf_pbr";
            fallback.params["baseColor"] = math::vec4{1.0f, 1.0f, 1.0f, 1.0f};
            out.materials.push_back(std::move(fallback));
        }

        bool read_primitive(const tinygltf::Primitive& prim, const std::string& name)
        {
            if (prim.mode != -1 && prim.mode != TINYGLTF_MODE_TRIANGLES)
                return false;

            auto attr = [&](const std::string& key)
            {
                auto it = prim.attributes.find(key);
                return it == prim.attributes.end() ? -1 : it->second;
            };

            const int pos = attr("POSITION");
            if (pos < 0)
                return false;

            MeshData m;
            m.name = name;
            m.material =
                prim.material >= 0 && size_t(prim.material) < defaultMaterial ? u32(prim.material) : defaultMaterial;

            for (const auto& p : read_floats<3>(pos))
                m.positions.push_back({p[0], p[1], p[2]});
            if (m.positions.empty())
                return false;

            if (int i = attr("NORMAL"); i >= 0)
                for (const auto& v : read_floats<3>(i))
                    m.normals.push_back({v[0], v[1], v[2]});

            if (int i = attr("TANGENT"); i >= 0)
                for (const auto& v : read_floats<4>(i))
                    m.tangents.push_back({v[0], v[1], v[2], v[3]});

            for (int set = 0; set < 3; ++set)
                if (int i = attr("TEXCOORD_" + std::to_string(set)); i >= 0)
                    for (const auto& v : read_floats<2>(i))
                        m.uvs[set].push_back({v[0], v[1]});

            if (int i = attr("COLOR_0"); i >= 0)
                for (const auto& v : read_floats<4>(i, {0.0f, 0.0f, 0.0f, 1.0f}))
                    m.colors.push_back({v[0], v[1], v[2], v[3]});

            if (int i = attr("JOINTS_0"); i >= 0)
                for (const auto& v : read_floats<4>(i))
                    m.joints.push_back({u16(v[0]), u16(v[1]), u16(v[2]), u16(v[3])});

            if (int i = attr("WEIGHTS_0"); i >= 0)
                for (const auto& v : read_floats<4>(i))
                    m.weights.push_back({v[0], v[1], v[2], v[3]});

            if (prim.indices >= 0)
                m.indices = read_indices(prim.indices);
            else
            {
                m.indices.resize(m.positions.size());
                std::iota(m.indices.begin(), m.indices.end(), 0u);
            }

            m.indices.resize(m.indices.size() - m.indices.size() % 3);
            if (m.indices.empty())
                return false;

            out.meshes.push_back(std::move(m));
            return true;
        }

        const std::vector<u32>& mesh_parts(int gltfMesh)
        {
            auto [it, inserted] = meshParts.try_emplace(gltfMesh);
            if (inserted)
            {
                const auto& mesh = gm.meshes[gltfMesh];
                for (const auto& prim : mesh.primitives)
                    if (read_primitive(prim, mesh.name))
                        it->second.push_back(u32(out.meshes.size() - 1));
            }
            return it->second;
        }

        void visit(int gltfIndex, int parent, int depth)
        {
            if (depth > 256)
                throw_error("gltf: node hierarchy too deep (cycle?)");

            const auto& gn = gm.nodes[gltfIndex];
            NodeData n;
            n.name = gn.name;
            n.parent = parent;

            if (gn.matrix.size() == 16)
            {
                math::mat4 mat{};
                for (int col = 0; col < 4; ++col)
                    for (int row = 0; row < 4; ++row)
                        mat[col][row] = float(gn.matrix[size_t(col * 4 + row)]);

                n.translation = {mat[3].x, mat[3].y, mat[3].z};
                for (int i = 0; i < 3; ++i)
                {
                    math::vec3 axis{mat[i].x, mat[i].y, mat[i].z};
                    n.scale[i] = length(axis);
                    if (n.scale[i] > 1e-8f)
                        axis = axis / n.scale[i];
                    mat[i] = {axis.x, axis.y, axis.z, 0.0f};
                }
                n.rotation = math::from_mat4(mat);
            }
            else
            {
                if (gn.translation.size() == 3)
                    n.translation = {float(gn.translation[0]), float(gn.translation[1]), float(gn.translation[2])};
                if (gn.rotation.size() == 4)
                    n.rotation = {float(gn.rotation[0]), float(gn.rotation[1]), float(gn.rotation[2]),
                                  float(gn.rotation[3])};
                if (gn.scale.size() == 3)
                    n.scale = {float(gn.scale[0]), float(gn.scale[1]), float(gn.scale[2])};
            }

            if (gn.mesh >= 0)
                n.meshes = mesh_parts(gn.mesh);

            const int index = int(out.nodes.size());
            out.nodes.push_back(std::move(n));

            for (int child : gn.children)
                visit(child, index, depth + 1);
        }

        ModelData run()
        {
            if (gm.scenes.empty())
                throw_error("gltf has no scenes");

            read_textures();
            read_materials();

            const auto& scene = gm.scenes[gm.defaultScene >= 0 ? size_t(gm.defaultScene) : 0];
            for (int root : scene.nodes)
                visit(root, -1, 0);

            return std::move(out);
        }
    };

    class GltfLoader : public ModelLoader
    {
    public:
        bool handles(std::string_view ext) const override { return ext == ".gltf" || ext == ".glb"; }

        ModelData load(std::span<const byte> data, const LoadContext& ctx) override
        {
            tinygltf::TinyGLTF loader;
            loader.SetImageLoader(load_image, nullptr);

            tinygltf::Model gm;
            std::string err, warn;
            const bool binary = data.size() >= 4 && std::memcmp(data.data(), "glTF", 4) == 0;
            const bool ok = binary
                ? loader.LoadBinaryFromMemory(&gm, &err, &warn, data.data(), unsigned(data.size()), ctx.baseDir)
                : loader.LoadASCIIFromString(&gm, &err, &warn, reinterpret_cast<const char*>(data.data()),
                                             unsigned(data.size()), ctx.baseDir);
            if (!ok)
                throw_error("gltf: " + err);

            return Importer{gm}.run();
        }
    };
} // namespace

std::unique_ptr<ModelLoader> graphics::make_gltf_loader() { return std::make_unique<GltfLoader>(); }

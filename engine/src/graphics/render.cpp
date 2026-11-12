#include "graphics/render.hpp"

#include <algorithm>
#include <glad/glad.h>
#include <utility>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>



namespace
{
    constexpr u32 initial_vertex_cap = 1 << 16;
    constexpr u32 initial_index_cap = 1 << 18;

    constexpr GLuint material_binding = 0;
    constexpr GLuint camera_binding = 1;
    constexpr GLuint object_binding = 2;

    struct AttribInfo
    {
        VertexFormat flag;
        GLint components;
        GLuint offset;
        bool is_int = false;
    };

    // index in this table = attribute location, so shaders can use layout(location = N)
    constexpr AttribInfo attribs[] = {
        {VertexFormat::Position, 3, offsetof(Vertex, position)},
        {VertexFormat::Normal, 3, offsetof(Vertex, normal)},
        {VertexFormat::UV0, 2, offsetof(Vertex, uv0)},
        {VertexFormat::Joints,4, offsetof(Vertex, joints), true},
        {VertexFormat::Weights,4, offsetof(Vertex, weights)}
    };

    constexpr GLuint joint_binding = 3;

    struct ObjectData
    {
        math::mat4 model;
        u32 jointOffset;
        u32 jointCount;
        u32 pad[2];
    };

    uin make_buffer(GLsizeiptr bytes)
    {
        uin buf = 0;
        glGenBuffers(1, &buf);
        glBindBuffer(GL_COPY_WRITE_BUFFER, buf);
        glBufferStorage(GL_COPY_WRITE_BUFFER, bytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
        return buf;
    }

    uin make_shader(GLenum type,const char* src) {
        uin shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);

        return shader;
    }

    uin make_program(uin* shaders, int shader_count) {
        uin program = glCreateProgram();

        for (int i = 0; i < shader_count; i++) {
            glAttachShader(program,shaders[i]);
        }

        glLinkProgram(program);
        for (int i = 0; i < shader_count; i++) {
            glDeleteShader(shaders[i]);
        }
        return program;
    }

    uin make_texture(const graphics::TextureDescriptor& d)
    { 
        GLenum internal = GL_RGBA8;
        switch (d.format)
        {
        case GL_RED: internal = GL_R8; break;
        case GL_RG: internal = GL_RG8; break;
        case GL_RGB: internal = d.sRGB ? GL_SRGB8 : GL_RGB8; break;
        case GL_RGBA: internal = d.sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8; break;
        }

        uin tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GLint(internal), d.width, d.height, 0, d.format, GL_UNSIGNED_BYTE, d.data.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        return tex;
    }

    void apply_state(const graphics::RenderState& s, graphics::RenderState& cur, bool force)
    {
        if (force || s.blend != cur.blend)
        {
            if (s.blend)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
                glDisable(GL_BLEND);
        }
        if (force || s.cullBack != cur.cullBack)
            s.cullBack ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
        if (force || s.depthWrite != cur.depthWrite)
            glDepthMask(s.depthWrite ? GL_TRUE : GL_FALSE);
        cur = s;
    }

    // immutable storage can't be resized, so allocate a bigger one and copy over
    uin grow_buffer(uin old, GLsizeiptr usedBytes, GLsizeiptr newBytes)
    {
        uin buf = make_buffer(newBytes);
        glBindBuffer(GL_COPY_READ_BUFFER, old);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, usedBytes);
        glDeleteBuffers(1, &old);
        return buf;
    }

    void upload(uin buf, GLintptr offset, GLsizeiptr bytes, const void* data)
    {
        glBindBuffer(GL_COPY_WRITE_BUFFER, buf);
        glBufferSubData(GL_COPY_WRITE_BUFFER, offset, bytes, data);
    }

    u32 grow_cap(u32 cap, u32 needed)
    {
        while (cap < needed)
            cap *= 2;
        return cap;
    }


    u32 gl_type_size(GLenum type)
    {
        switch (type)
        {
            case GL_FLOAT:
            case GL_INT:
            case GL_UNSIGNED_INT:
            case GL_BOOL:
                return 4;
            case GL_FLOAT_VEC2:
                return 8;
            case GL_FLOAT_VEC3:
                return 12;
            case GL_FLOAT_VEC4:
                return 16;
            case GL_FLOAT_MAT4:
                return 64;
            default:
                return 0;
        }
    }

    bool is_sampler(GLenum type)
    {
        return type == GL_SAMPLER_2D || type == GL_SAMPLER_2D_ARRAY || type == GL_SAMPLER_CUBE;
    }

    std::string resource_name(uin program, GLenum iface, GLuint index, GLint len)
    {
        std::string name(len, '\0');
        glGetProgramResourceName(program, iface, index, len, nullptr, name.data());
        name.resize(len - 1);
        return name;
    }

    graphics::ShaderLayout reflect(uin program)
    {
        graphics::ShaderLayout layout;

        GLuint block = glGetProgramResourceIndex(program, GL_UNIFORM_BLOCK, "Material");
        if (block != GL_INVALID_INDEX)
        {
            glUniformBlockBinding(program, block, material_binding);

            const GLenum blockProps[] = {GL_BUFFER_DATA_SIZE, GL_NUM_ACTIVE_VARIABLES};
            GLint blockInfo[2];
            glGetProgramResourceiv(program, GL_UNIFORM_BLOCK, block, 2, blockProps, 2, nullptr, blockInfo);
            layout.blockSize = u32(blockInfo[0]);

            std::vector<GLint> vars(blockInfo[1]);
            const GLenum activeVars = GL_ACTIVE_VARIABLES;
            glGetProgramResourceiv(program, GL_UNIFORM_BLOCK, block, 1, &activeVars, GLsizei(vars.size()), nullptr,
                                   vars.data());

            for (GLint v : vars)
            {
                const GLenum props[] = {GL_OFFSET, GL_TYPE, GL_NAME_LENGTH};
                GLint info[3];
                glGetProgramResourceiv(program, GL_UNIFORM, v, 3, props, 3, nullptr, info);
                layout.params[resource_name(program, GL_UNIFORM, v, info[2])] = {u32(info[0]), gl_type_size(info[1]),
                                                                                 u32(info[1])};
            }
        }

        auto bind_block = [&](const char* name, GLuint binding)
        {
            GLuint idx = glGetProgramResourceIndex(program, GL_UNIFORM_BLOCK, name);
            if (idx != GL_INVALID_INDEX)
                glUniformBlockBinding(program, idx, binding);
        };
        bind_block("Camera", camera_binding);
        bind_block("Object", object_binding);

        GLint count = 0;
        glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &count);
        u32 unit = 0;
        for (GLint i = 0; i < count; ++i)
        {
            const GLenum props[] = {GL_TYPE, GL_BLOCK_INDEX, GL_LOCATION, GL_NAME_LENGTH};
            GLint info[4];
            glGetProgramResourceiv(program, GL_UNIFORM, i, 4, props, 4, nullptr, info);
            if (info[1] != -1 || !is_sampler(info[0]))
                continue;

            glProgramUniform1i(program, info[2], GLint(unit));
            layout.samplers[resource_name(program, GL_UNIFORM, i, info[3])] = unit++;
        }

        return layout;
    }

} // namespace

using namespace graphics;

GpuShader::~GpuShader() { glDeleteProgram(handle); }
GpuShader::GpuShader(GpuShader&& o) noexcept : layout(std::move(o.layout)), handle(std::exchange(o.handle, 0)) {}

GpuShader& GpuShader::operator=(GpuShader&& o) noexcept
{
    if (this != &o)
    {
        glDeleteProgram(handle);
        handle = std::exchange(o.handle, 0);
        layout = std::move(o.layout);
    }
    return *this;
}
GpuShader::GpuShader(u32 handle) : handle(handle) { }


RenderMgr::RenderMgr(const RenderConfiguration& cfg)
{
    drawCalls.reserve(1024);

    GLint align = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &align);
    materialStride = std::max<u32>(256, u32(align));

    cameraBuffer = make_buffer(sizeof(CameraData));
    glBindBufferBase(GL_UNIFORM_BUFFER, camera_binding, cameraBuffer);

    auto solid = [&](byte r, byte g, byte b)
    {
        const byte px[4] = {r, g, b, 255};
        return create_texture({1, 1, px, GL_RGBA});
    };
    whiteTexture = solid(255, 255, 255); // first, so handle 0 is white
    blackTexture = solid(0, 0, 0);
    normalTexture = solid(128, 128, 255);
    glEnable(GL_FRAMEBUFFER_SRGB);
}

RenderMgr::~RenderMgr()
{
    for (auto& entry : pools)
    {
        FormatPool& pool = entry.second;
        glDeleteVertexArrays(1, &pool.vao);
        glDeleteBuffers(1, &pool.vbo);
        glDeleteBuffers(1,&pool.ibo);
    }
}

u32 RenderMgr::push_joints(std::span<const math::mat4> joints)
{
    const u32 offset = u32(jointStaging.size());
    jointStaging.insert(jointStaging.end(), joints.begin(), joints.end());
    return offset;
}

ShaderHandle RenderMgr::create_shader(const ShaderDescriptor& descriptor) {

    uin* _shaders = new uin[2]; // todo: allow multiple stages
    _shaders[0] = make_shader(GL_VERTEX_SHADER,descriptor.vertexSource.c_str());
    _shaders[1] = make_shader(GL_FRAGMENT_SHADER,descriptor.fragmentSource.c_str());
    
    GpuShader shader(make_program(_shaders, 2));
    shader.layout = reflect(shader.handle);
    if (shader.layout.blockSize > materialStride)
        throw_error("material block is bigger than the material stride");
    shaders.push_back(std::move(shader));

    return ShaderHandle(shaders.size() - 1);
}

MaterialHandle RenderMgr::create_material(const ShaderHandle& handle)
{
    const ShaderLayout& layout = shaders[handle].layout;
    const u32 index = u32(materials.size());

    if (index >= materialCap)
    {
        const u32 cap = materialCap ? materialCap* 2 : 256;
        materialBuffer = materialBuffer
            ? grow_buffer(materialBuffer, materialCap * materialStride, cap * materialStride)
            : make_buffer(cap * materialStride);
        materialCap = cap;
    }

    Material m{};
    m.shader = handle;
    m.params.resize(layout.blockSize);
    m.textures.resize(layout.samplers.size(), 0);

    for (const auto& [name, unit] : layout.samplers)
        m.textures[unit] = default_texture(name);

    m.gpuOffset = index * materialStride;
    materials.push_back(std::move(m));

    return MaterialHandle(index);
}

MeshHandle RenderMgr::create_mesh(const MeshDescriptor& descriptor)
{
    if (descriptor.vertices.empty() || descriptor.indices.empty())
        throw_error("mesh has no vertices or indices");

    FormatPool& pool = get_pool(descriptor.format);

    const u32 vtxCount = u32(descriptor.vertices.size());
    const u32 idxCount = u32(descriptor.indices.size());

    if (pool.vertexCt + vtxCount > pool.vertexCap)
    {
        const u32 cap = grow_cap(pool.vertexCap, pool.vertexCt + vtxCount);
        pool.vbo = grow_buffer(pool.vbo, int(pool.vertexCt) * sizeof(Vertex), GLsizeiptr(cap) * sizeof(Vertex));
        pool.vertexCap = cap;

        glBindVertexArray(pool.vao);
        glBindVertexBuffer(0, pool.vbo, 0, sizeof(Vertex));
        glBindVertexArray(0);
    }

    if (pool.indexCt + idxCount > pool.indexCap)
    {
        const u32 cap = grow_cap(pool.indexCap, pool.indexCt + idxCount);
        pool.ibo = grow_buffer(pool.ibo, GLsizeiptr(pool.indexCt) * sizeof(u32), GLsizeiptr(cap) * sizeof(u32));
        pool.indexCap = cap;

        glBindVertexArray(pool.vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pool.ibo);
        glBindVertexArray(0);
    }

    upload(pool.vbo, GLintptr(pool.vertexCt) * sizeof(Vertex), GLsizeiptr(descriptor.vertices.size_bytes()),
           descriptor.vertices.data());
    upload(pool.ibo, GLintptr(pool.indexCt) * sizeof(u32), GLsizeiptr(descriptor.indices.size_bytes()),
           descriptor.indices.data());

    meshes.push_back({descriptor.format, pool.vertexCt, pool.indexCt, idxCount});

    pool.vertexCt += vtxCount;
    pool.indexCt += idxCount;

    return MeshHandle(meshes.size() - 1);
}

TextureHandle RenderMgr::create_texture(const TextureDescriptor& descriptor)
{
    textures.push_back(GpuTexture(make_texture(descriptor)));
    return TextureHandle(textures.size() - 1);
}

void RenderMgr::submit_draw_call(const DrawCall& drawCall)
{
    if (drawCall.mesh >= meshes.size() || drawCall.material >= materials.size())
        throw_error("draw call references a mesh or material that doesn't exist");
    if (drawCalls.size() >= cfg.maxDrawCalls)
        return;

    drawCalls.push_back(drawCall);
}

void RenderMgr::screenshot(std::string filename)
{
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    int x = viewport[0];
    int y = viewport[1];
    int width = viewport[2];
    int height = viewport[3];

    char *data = (char*) malloc((size_t) (width * height * 3)); // 3 components (R, G, B)

    if (!data)
        return;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);

    stbi_write_png(filename.c_str(), width, height, 3, data, 0);

    free(data);
}

void RenderMgr::flush()
{
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE); // a transparent material from last frame can leave this off, and then the clear skips depth
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    upload(cameraBuffer, 0, sizeof(CameraData), &camera_);
    upload_materials();
    std::sort(drawCalls.begin(), drawCalls.end(),
              [this](const DrawCall& a, const DrawCall& b)
              {
                  const bool ta = materials[a.material].state.blend, tb = materials[b.material].state.blend;
                  if (ta != tb)
                      return !ta;
                  if (a.material != b.material)
                      return a.material < b.material;
                  return meshes[a.mesh].fmt < meshes[b.mesh].fmt;
              });

    const u32 count = u32(drawCalls.size());
    if (count > objectCap)
    {
        if (objectBuffer)
            glDeleteBuffers(1, &objectBuffer);
        objectCap = grow_cap(std::max(objectCap, 256u), count);
        objectBuffer = make_buffer(GLsizeiptr(objectCap) * materialStride);
    }

    objectStaging.resize(size_t(count) * materialStride);
    for (u32 i = 0; i < count; ++i)
    {
        const ObjectData od{drawCalls[i].model, drawCalls[i].jointOffset, drawCalls[i].jointCount, {}};
        std::memcpy(objectStaging.data() + size_t(i) * materialStride, &od, sizeof(od));
    }
    upload(objectBuffer, 0, GLsizeiptr(objectStaging.size()), objectStaging.data());

    if (!jointStaging.empty())
    {
        const u32 needed = u32(jointStaging.size());
        if (needed > jointCap)
        {
            if (jointBuffer)
                glDeleteBuffers(1, &jointBuffer);
            jointCap = grow_cap(std::max(jointCap, 1024u), needed);
            jointBuffer = make_buffer(GLsizeiptr(jointCap) * sizeof(math::mat4));
        }
        upload(jointBuffer, 0, GLsizeiptr(needed) * sizeof(math::mat4), jointStaging.data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, joint_binding, jointBuffer);
    }

    uin curProgram = 0;
    uin curVao = 0;
    u32 curMaterial = UINT32_MAX;
    RenderState curState{};
    bool firstState = true;

    for (u32 i = 0; i < count; ++i)
    {
        const DrawCall& dc = drawCalls[i];
        const GpuMesh& mesh = meshes[dc.mesh];

        Material& material = materials[dc.material];
        u32 program = shaders[material.shader].handle;
        if (program != curProgram)
        {
            glUseProgram(program);
            curProgram = program;
        }
        if (dc.material != curMaterial)
        {
            const Material& m = materials[dc.material];

            if (!m.params.empty())
                glBindBufferRange(GL_UNIFORM_BUFFER, material_binding, materialBuffer, m.gpuOffset, m.params.size());

            for (u32 unit = 0; unit < m.textures.size(); ++unit)
            {
                glActiveTexture(GL_TEXTURE0 + unit);
                glBindTexture(GL_TEXTURE_2D, textures[m.textures[unit]].handle);
            }

            curMaterial = dc.material; // u32 curMaterial = UINT32_MAX; before the loop

            apply_state(m.state, curState, firstState);
            firstState = false;
        }

        const uin vao = pools.at(mesh.fmt).vao;
        if (vao != curVao)
        {
            glBindVertexArray(vao);
            curVao = vao;
        }

        glBindBufferRange(GL_UNIFORM_BUFFER, object_binding, objectBuffer, GLintptr(i) * materialStride,
                          sizeof(ObjectData));

        glDrawElementsInstancedBaseVertexBaseInstance(
            GL_TRIANGLES, GLsizei(mesh.idx), GL_UNSIGNED_INT,
            reinterpret_cast<const void*>(std::uintptr_t(mesh.first_idx) * sizeof(u32)), 1, GLint(mesh.base_vtx), 0);
    }

    glBindVertexArray(0);
    drawCalls.clear();
    jointStaging.clear();
}

TextureHandle RenderMgr::default_texture(std::string_view sampler) const
{
    if (sampler.starts_with("_n"))
        return normalTexture;
    if (sampler.starts_with("_e"))
        return blackTexture;
    return whiteTexture;
}

RenderMgr::FormatPool& RenderMgr::get_pool(VertexFormat fmt)
{
    if (auto it = pools.find(fmt); it != pools.end())
        return it->second;

    if (!has(fmt, VertexFormat::Position))
        throw_error("vertex format has no position");
    if ((fmt & (VertexFormat::UV1 | VertexFormat::UV2)) != VertexFormat::None)
        throw_error("UV1/UV2 aren't in Vertex yet");

    FormatPool pool;
    pool.vertexCap = initial_vertex_cap;
    pool.indexCap = initial_index_cap;
    pool.vbo = make_buffer(GLsizeiptr(pool.vertexCap) * sizeof(Vertex));
    pool.ibo = make_buffer(GLsizeiptr(pool.indexCap) * sizeof(u32));

    glGenVertexArrays(1, &pool.vao);
    glBindVertexArray(pool.vao);

    for (GLuint loc = 0; loc < std::size(attribs); ++loc)
    {
        const AttribInfo& a = attribs[loc];
        if (!has(fmt, a.flag))
            continue;

        glEnableVertexAttribArray(loc);
        if (a.is_int)
            glVertexAttribIFormat(loc, a.components, GL_UNSIGNED_SHORT, a.offset);
        else
            glVertexAttribFormat(loc, a.components, GL_FLOAT, GL_FALSE, a.offset);
        glVertexAttribBinding(loc, 0);
    }

    glBindVertexBuffer(0, pool.vbo, 0, sizeof(Vertex));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pool.ibo);

    glBindVertexArray(0);

    return pools.emplace(fmt, pool).first->second;
}

void RenderMgr::upload_materials()
{
    for (Material& m : materials)
    {
        if (!m.dirty || m.params.empty())
            continue;
        upload(materialBuffer, m.gpuOffset, GLsizeiptr(m.params.size()), m.params.data());
        m.dirty = false;
    }
}

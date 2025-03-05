#include "orxata.h"

#include <llulu/lu_time.h>
#include <spine/spine.h>
#include <spine/extension.h>
#include <glad/glad.h>
#include <stb/stb_image.h>
#include <sys/stat.h>

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

enum {
    ORX_MAX_VERTICES = 1U << 14,
    ORX_MAX_INDICES = 1U << 14,
    ORX_MAX_UNIFORMS = 126,
    ORX_MAX_DRAW_INDIRECT = 128,
    ORX_MAX_TEXTURE_ARRAYS = 16,
    ORX_MAX_TEXTURE_ARRAY_SIZE = 16,

    ORX_MAX_SHADER_SOURCE_LEN = 4096,
    ORX_MAX_ERROR_MSG_LEN = 2048,
    ORX_MAX_SYNC_TIMEOUT_NANOSEC = 50000000
};

typedef struct orx_shader_shape_data_t {
    float pos_x;
    float pos_y;
    float scale_x;
    float scale_y;
    float rotation;
    int32_t tex_idx;
    float tex_layer;
    float padding;
} orx_shader_shape_data_t;

typedef struct orx_shader_data_t {
    float view_proj[16]; // Ortho projection matrix 
    orx_shader_shape_data_t data[ORX_MAX_UNIFORMS];
} orx_shader_data_t;

typedef struct orx_texarr_pool_t {
    uint32_t id[ORX_MAX_TEXTURE_ARRAYS];
    orx_texture_format_t fmt[ORX_MAX_TEXTURE_ARRAYS];
    int count[ORX_MAX_TEXTURE_ARRAYS];
} orx_texarr_pool_t;

typedef struct orx_drawcmd {
    uint32_t element_count;
    uint32_t instance_count;
    uint32_t first_idx;
    int32_t  base_vtx;
    uint32_t draw_index; // base_instance
} orx_drawcmd_t;

typedef struct orx_gfx_pers_map {
    enum {
        ORX_GFX_MAP_FLAGS = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT,
        ORX_GFX_STORAGE_FLAGS = ORX_GFX_MAP_FLAGS | GL_DYNAMIC_STORAGE_BIT
    };
    void *data;
    size_t head;
    GLuint id;
} orx_gfx_pers_map;

typedef struct orx_gl_renderer_t {
    //int idx_count;
    //int vtx_count;
    //orx_vertex_t vtxbuf[ORX_MAX_VERTICES]; // TODO: better heap than static
    //orx_index_t idxbuf[ORX_MAX_INDICES];

    orx_texarr_pool_t tex;
    int tex_count;

    int phase; /* for the triphassic fence */
    GLsync fence[3];
    //orx_shader_data_t shader_data;
    float view_proj[16];

    orx_drawcmd_t drawlist[ORX_MAX_DRAW_INDIRECT];
    orx_drawcmd_t *draw_next;

    uint32_t program_id;
    uint32_t va_id; // vertex array object 
    orx_gfx_pers_map vertices_vmap;
    orx_gfx_pers_map indices_vmap;
    orx_gfx_pers_map uniforms_vmap;
    orx_gfx_pers_map draw_indirect_vmap;
} orx_gl_renderer_t;

enum {
    ORX_VERTICES_MAP_SIZE = ORX_MAX_VERTICES * 3 * sizeof(orx_vertex_t),
    ORX_INDICES_MAP_SIZE = ORX_MAX_INDICES * 3 * sizeof(orx_index_t),
    ORX_UNIFORMS_MAP_SIZE = 3 * sizeof(orx_shader_data_t),
    ORX_DRAW_INDIRECT_MAP_SIZE = ORX_MAX_DRAW_INDIRECT * 3 * sizeof(orx_drawcmd_t),
};

static orx_gl_renderer_t g_r; // Depends on zero init
static orx_config_t g_config;

static inline void
orx_gfx_sync()
{
    #if 0
    lu_timestamp start = lu_time_get();
    GLenum err = glClientWaitSync(g_r.fence[g_r.phase], GL_SYNC_FLUSH_COMMANDS_BIT, ORX_MAX_SYNC_TIMEOUT_NANOSEC);
    int64_t sync_time_ns = lu_time_elapsed(start);
    if (err == GL_TIMEOUT_EXPIRED) {
        printf("Error: something is wrong with the gpu fences: sync blocked for more than 5ms.\n");
    } else if (err == GL_CONDITION_SATISFIED) {
        printf("Warning: gpu fence blocked for %llu ns.\n", sync_time_ns);
    }
        #endif
}

void
orx_shader_gpu_setup(void)
{
    // Ortho proj matrix
    float left = 0.0f;
    float right = (float)g_config.canvas_width;
    float bottom = (float)g_config.canvas_height;
    float top = 0.0f;
    float near = -1.0f;
    float far = 1.0f;
    g_r.view_proj[0] = 2.0f / (right - left);
    g_r.view_proj[5] = 2.0f / (top - bottom);
    g_r.view_proj[10] = -2.0f / (far - near);
    g_r.view_proj[12] = -(right + left) / (right - left);
    g_r.view_proj[13] = -(top + bottom) / (top - bottom);
    g_r.view_proj[14] = -(far + near) / (far - near);
    g_r.view_proj[15] = 1.0f;

    g_r.program_id = glCreateProgram();
    orx_shader_reload();
    //glCreateBuffers(1, &g_r.ub_id);
    //glBindBuffer(GL_UNIFORM_BUFFER, g_r.ub_id);
    //glBufferData(GL_UNIFORM_BUFFER, sizeof(orx_shader_data_t), &g_r.shader_data, GL_DYNAMIC_DRAW);
}

void
orx_shader_reload(void)
{
    char src_buf[ORX_MAX_SHADER_SOURCE_LEN];
    const GLchar *src1 = &src_buf[0];
    const GLchar *const *src2 = &src1;
    // Vert
    FILE *f = fopen(g_config.vert_shader_path, "r");
    if (!f) {
        printf("Could not open shader source %s.\n", g_config.vert_shader_path);
        return;
    }
    size_t len = fread(&src_buf[0], 1, ORX_MAX_SHADER_SOURCE_LEN- 1, f);
    src_buf[len] = '\0'; // TODO: pass len to glShaderSource instead of this (remove the '- 1' in the fread too).
    fclose(f);
    assert(len < ORX_MAX_SHADER_SOURCE_LEN);

    GLchar out_log[ORX_MAX_ERROR_MSG_LEN];
    GLint err;
    GLuint vert_id = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_id, 1, src2, NULL);
    glCompileShader(vert_id);
    glGetShaderiv(vert_id, GL_COMPILE_STATUS, &err);
    if (!err) {
        glGetShaderInfoLog(vert_id, ORX_MAX_ERROR_MSG_LEN, NULL, out_log);
        printf("Vert Shader:\n%s\n", out_log);
        glDeleteShader(vert_id);
        return;
    }
    glAttachShader(g_r.program_id, vert_id);

    // Frag
    f = fopen(g_config.frag_shader_path, "r");
    if (!f) {
        printf("Could not open shader source %s.\n", g_config.frag_shader_path);
        return;
    }
    len = fread(&src_buf[0], 1, ORX_MAX_SHADER_SOURCE_LEN- 1, f);
    src_buf[len] = '\0';
    fclose(f);
    assert(len < ORX_MAX_SHADER_SOURCE_LEN);

    GLuint frag_id = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_id, 1, src2, NULL); // see comment in the vertex shader
    glCompileShader(frag_id);
    glGetShaderiv(frag_id, GL_COMPILE_STATUS, &err);
    if (!err) {
        glGetShaderInfoLog(frag_id, ORX_MAX_ERROR_MSG_LEN, NULL, out_log);
        printf("Frag Shader:\n%s\n", out_log);
        glDetachShader(g_r.program_id, vert_id);
        glDeleteShader(frag_id);
        glDeleteShader(vert_id);
        return;
    }
    glAttachShader(g_r.program_id, frag_id);

    // Program
    glLinkProgram(g_r.program_id);
    glGetProgramiv(g_r.program_id, GL_LINK_STATUS, &err);
    if (!err) {
        glGetProgramInfoLog(g_r.program_id, ORX_MAX_ERROR_MSG_LEN, NULL, out_log);
        printf("Program link error:\n%s\n", out_log);
    }

    glUseProgram(g_r.program_id);
    glDetachShader(g_r.program_id, vert_id);
    glDeleteShader(vert_id);
    glDetachShader(g_r.program_id, frag_id);
    glDeleteShader(frag_id);
}

#ifndef _WIN32
#define _stat stat
#else
#define fstat _stat
#endif

static void
orx_shader_check_reload(void)
{
    if (g_config.seconds_between_shader_file_changed_checks > 0.0f) {
        static time_t timer;
        static time_t last_modified;
        if (!timer) {
            assert(!last_modified);
            time(&timer);
            last_modified = timer;
        }

        if (difftime(time(NULL), timer) > g_config.seconds_between_shader_file_changed_checks) {
            struct _stat stat;

            int err = fstat(g_config.vert_shader_path, &stat);
            if (err) {
                if (err == -1) {
                    printf("%s not found.\n", g_config.vert_shader_path);
                } else if (err == 22) {
                    printf("Invalid parameter in _stat (vert).\n");
                }
            } else {
                if (difftime(stat.st_mtime, last_modified) > 0.0) {
                    last_modified = stat.st_mtime;
#ifdef ORX_VERBOSE
                    printf("Reloading shaders.\n");
#endif
                    orx_shader_reload();
                } else {
                    err = fstat(g_config.frag_shader_path, &stat);
                    if (err) {
                        if (err == -1) {
                            printf("%s not found.\n", g_config.frag_shader_path);
                        } else if (err == 22) {
                            printf("Invalid parameter in _stat (frag).\n");
                        }
                    } else {
                        if (difftime(stat.st_mtime, last_modified) > 0.0) {
                            last_modified = stat.st_mtime;
#ifdef ORX_VERBOSE
                            printf("Reloading shaders.\n");
#endif
                            orx_shader_reload();
                        }
                    }
                }
            }
            time(&timer);
        }
    }
}

void
orx_mesh_gpu_setup(void)
{
    glCreateVertexArrays(1, &g_r.va_id);
    glBindVertexArray(g_r.va_id);
    glVertexArrayVertexBuffer(g_r.va_id, 0, g_r.vertices_vmap.id, 0, sizeof(orx_vertex_t));
    glVertexArrayElementBuffer(g_r.va_id, g_r.indices_vmap.id);

    glEnableVertexArrayAttrib(g_r.va_id, 0);
    glVertexArrayAttribFormat(g_r.va_id, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(g_r.va_id, 0, 0);

    glEnableVertexArrayAttrib(g_r.va_id, 1);
    glVertexArrayAttribFormat(g_r.va_id, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
    glVertexArrayAttribBinding(g_r.va_id, 1, 0);

    glEnableVertexArrayAttrib(g_r.va_id, 2);
    glVertexArrayAttribFormat(g_r.va_id, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 4 * sizeof(float));
    glVertexArrayAttribBinding(g_r.va_id, 2, 0);
}

static const int g_tex_internfmt_lut[ORX_PIXFMT_COUNT] = {
    GL_R8,
    GL_RG8,
    GL_RGB8,
    GL_SRGB8,
    GL_RGBA8,
    GL_R16F,
    GL_RG16F,
    GL_RGB16F,
    GL_RGBA16F,
    GL_R32F,
    GL_RG32F,
    GL_RGB32F,
    GL_RGBA32F,
};

static const int g_tex_format_lut[ORX_PIXFMT_COUNT] = {
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGB,
    GL_RGBA,
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGBA,
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGBA,
};

static const int g_tex_typefmt_lut[ORX_PIXFMT_COUNT] = {
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_HALF_FLOAT,
    GL_HALF_FLOAT,
    GL_HALF_FLOAT,
    GL_HALF_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
};

static inline void
orx_texture_gpu_setup(void)
{
    glCreateTextures(GL_TEXTURE_2D_ARRAY, g_r.tex_count, g_r.tex.id);
    glBindTextures(0, g_r.tex_count, g_r.tex.id);
    for (int i = 0; i < g_r.tex_count; ++i) {
        const orx_texture_format_t *fmt = &g_r.tex.fmt[i];
        glTextureStorage3D(g_r.tex.id[i], 1, g_tex_internfmt_lut[fmt->pixel_fmt], fmt->width, fmt->height, g_r.tex.count[i]);
    }
}

orx_texture_t
orx_texture_reserve(orx_texture_format_t fmt)
{
    assert(fmt.width + fmt.height != 0);
    assert(fmt.pixel_fmt >= 0 && fmt.pixel_fmt < ORX_PIXFMT_COUNT && "Invalid pixel format.");

    // Look for an array of textures of the same format and push the new tex.
    for (int i = 0; i < g_r.tex_count; ++i) {
        if (!memcmp(&g_r.tex.fmt[i], &fmt, sizeof(fmt))) {
            int layer = g_r.tex.count[i]++;
            return (orx_texture_t){i, layer};
        }
    }

    // If it is the first tex with that format, add a new vector to the pool.
    if (g_r.tex_count >= ORX_MAX_TEXTURE_ARRAYS) {
        printf("Error: orx_texture_reserve failed (full texture pool). Consider increasing ORX_MAX_TEXTURE_ARRAYS.\n");
        return (orx_texture_t){-1, -1};
    }

    int index = g_r.tex_count++;
    g_r.tex.fmt[index] = fmt;
    g_r.tex.count[index] = 1;
    return (orx_texture_t){index, 0};
}

void
orx_texture_set(orx_texture_t tex, void *data)
{
    assert(data);
    const orx_texture_format_t *fmt = &g_r.tex.fmt[tex.idx];
    glTextureSubImage3D(g_r.tex.id[tex.idx], 0, 0, 0, (int)tex.layer, fmt->width, fmt->height, 1, g_tex_format_lut[fmt->pixel_fmt], g_tex_typefmt_lut[fmt->pixel_fmt], data);
}

orx_image_t
orx_load_image(const char *path)
{
    orx_image_t img = {.path = path, .tex = {-1, -1} };
    img.data = stbi_load(img.path, &img.w, &img.h, &img.channels, 0);

    if (!img.data) {
        printf("Error loading the image %s.\nAborting execution.\n", img.path);
        exit(1);
    } else {
        img.tex = orx_texture_reserve((orx_texture_format_t){
            .width = img.w,
            .height = img.h,
            .pixel_fmt = img.channels == 4 ? ORX_PIXFMT_RGBA : ORX_PIXFMT_RGB,
            .flags = 0
        });
        assert(img.tex.idx >= 0);
    }
    return img;
}

void
orx_init(orx_config_t *cfg)
{
    g_r.draw_next = g_r.drawlist;

    if (cfg) {
        g_config = *cfg;
        gladLoadGLLoader(cfg->gl_loader);
    } else {
        g_config.canvas_width = 1024;
        g_config.canvas_height = 1024;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.2f, 0.0f, 0.2f, 1.0f);
    glViewport(0, 0, g_config.canvas_width, g_config.canvas_height);

    /* Persistent mapped buffers for vertices, indices uniforms and indirect draw commands. */
    GLuint buf_id[4];
    glCreateBuffers(4, buf_id);
    g_r.vertices_vmap.id = buf_id[0];
    glNamedBufferStorage(g_r.vertices_vmap.id, ORX_VERTICES_MAP_SIZE, NULL, ORX_GFX_STORAGE_FLAGS);
    g_r.vertices_vmap.data = glMapNamedBufferRange(g_r.vertices_vmap.id, 0, ORX_VERTICES_MAP_SIZE, ORX_GFX_MAP_FLAGS);
    g_r.indices_vmap.id = buf_id[1];
    glNamedBufferStorage(g_r.indices_vmap.id, ORX_INDICES_MAP_SIZE, NULL, ORX_GFX_STORAGE_FLAGS);
    g_r.indices_vmap.data = glMapNamedBufferRange(g_r.indices_vmap.id, 0, ORX_INDICES_MAP_SIZE, ORX_GFX_MAP_FLAGS);
    g_r.uniforms_vmap.id = buf_id[2];
    glNamedBufferStorage(g_r.uniforms_vmap.id, ORX_UNIFORMS_MAP_SIZE, NULL, ORX_GFX_STORAGE_FLAGS);
    g_r.uniforms_vmap.data = glMapNamedBufferRange(g_r.uniforms_vmap.id, 0, ORX_UNIFORMS_MAP_SIZE, ORX_GFX_MAP_FLAGS);
    g_r.draw_indirect_vmap.id = buf_id[3];
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_r.draw_indirect_vmap.id);
    glNamedBufferStorage(g_r.draw_indirect_vmap.id, ORX_DRAW_INDIRECT_MAP_SIZE, NULL, ORX_GFX_STORAGE_FLAGS);
    g_r.draw_indirect_vmap.data = glMapNamedBufferRange(g_r.draw_indirect_vmap.id, 0, ORX_DRAW_INDIRECT_MAP_SIZE, ORX_GFX_MAP_FLAGS);

    orx_shader_gpu_setup();
    orx_mesh_gpu_setup();
    orx_texture_gpu_setup();
}

orx_shape_t
orx_gfx_add_mesh(const void *vert, size_t vert_size, const void *indices, size_t indices_size)
{
    assert(vert_size < (ORX_VERTICES_MAP_SIZE / 3));
    assert(indices_size < (ORX_INDICES_MAP_SIZE / 3));
    orx_gfx_sync();

    orx_shape_t shape = { .idx_count = indices_size / sizeof(orx_index_t)};

    if ((g_r.vertices_vmap.head + vert_size) >= ORX_VERTICES_MAP_SIZE) {
        g_r.vertices_vmap.head = 0;
    }
    shape.base_vtx = g_r.vertices_vmap.head / sizeof(orx_vertex_t);
    void *dst = (char*)g_r.vertices_vmap.data + g_r.vertices_vmap.head;
    memcpy(dst, vert, vert_size);
    g_r.vertices_vmap.head += vert_size;

    if ((g_r.indices_vmap.head + indices_size) >= ORX_INDICES_MAP_SIZE) {
        g_r.indices_vmap.head = 0;
    }
    shape.first_idx = g_r.indices_vmap.head / sizeof(orx_index_t);
    dst = (char*)g_r.indices_vmap.data + g_r.indices_vmap.head;
    memcpy(dst, indices, indices_size);
    g_r.indices_vmap.head += indices_size;
    return shape;
}

int orx_gfx_add_material(orx_material_t mat)
{
    assert(((g_r.uniforms_vmap.head - offsetof(orx_shader_data_t, data)) % sizeof(orx_shader_shape_data_t)) == 0);
    orx_shader_shape_data_t *uniform = (void*)((char*)g_r.uniforms_vmap.data + g_r.uniforms_vmap.head);
    orx_gfx_sync();
    uniform->pos_x = mat.apx;
    uniform->pos_y = mat.apy;
    uniform->scale_x = mat.asx;
    uniform->scale_y = mat.asy;
    uniform->rotation = mat.arot;
    uniform->tex_idx = mat.tex.idx;
    uniform->tex_layer = (float)mat.tex.layer;

    int idx = (g_r.uniforms_vmap.head - offsetof(orx_shader_data_t, data)) / sizeof(orx_shader_shape_data_t);
    g_r.uniforms_vmap.head += sizeof(orx_shader_shape_data_t);
    return idx;
}

void
orx_gfx_submit(orx_shape_t shape)
{
    size_t frame_start = g_r.phase * (ORX_DRAW_INDIRECT_MAP_SIZE / 3);
    assert(g_r.draw_indirect_vmap.head - frame_start < ORX_DRAW_INDIRECT_MAP_SIZE / 3);

    orx_gfx_sync();
    
    *((orx_drawcmd_t*)(g_r.draw_indirect_vmap.data + g_r.draw_indirect_vmap.head)) = (orx_drawcmd_t){
        .element_count = shape.idx_count,
        .instance_count = 1,
        .first_idx = shape.first_idx,
        .base_vtx = shape.base_vtx,
        .draw_index = shape.material_idx
    };
    g_r.draw_indirect_vmap.head += sizeof(orx_drawcmd_t);
}

void orx_render(void)
{
    // Check if any of the GLSL files has changed for hot-reload.
    // TODO: async
    orx_shader_check_reload();

    orx_gfx_sync();
    memcpy(g_r.uniforms_vmap.data + g_r.phase * sizeof(orx_shader_data_t), g_r.view_proj, sizeof(g_r.view_proj));
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, g_r.uniforms_vmap.id, g_r.phase * sizeof(orx_shader_data_t), sizeof(orx_shader_data_t));

    glClear(GL_COLOR_BUFFER_BIT);
    // sync mesh buffer
    // TODO: Persistent mapped fenced ring buffer ?? Spines at least
    //glNamedBufferData(g_r.vb_id, g_r.vtx_count * sizeof(orx_vertex_t), (const void*)(&g_r.vtxbuf[0]), GL_STATIC_DRAW);
    //glNamedBufferData(g_r.ib_id, g_r.idx_count * sizeof(orx_index_t), (const void*)(&g_r.idxbuf[0]), GL_STATIC_DRAW);
    // sync shader data buffer
    // TODO: Persistent mapped fenced ring buffer
    //glBufferData(GL_UNIFORM_BUFFER, sizeof(orx_shader_data_t), &g_r.shader_data, GL_DYNAMIC_DRAW);
    //glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_r.ub_id);
    // sync draw command buffer 
    // TODO: Persistent mapped fenced ring buffer
    //glBufferData(GL_DRAW_INDIRECT_BUFFER, (char*)g_r.draw_next - (char*)g_r.drawlist, g_r.drawlist, GL_DYNAMIC_DRAW);

    size_t offset = (g_r.phase * (ORX_DRAW_INDIRECT_MAP_SIZE / 3));
    size_t cmdbuf_size = g_r.draw_indirect_vmap.head - offset;
    assert(cmdbuf_size < (ORX_DRAW_INDIRECT_MAP_SIZE / 3) && "The draw command list is larger than a third of the mapped buffer.");
    size_t cmdbuf_count = cmdbuf_size / sizeof(orx_drawcmd_t);
    assert(cmdbuf_count < ORX_MAX_DRAW_INDIRECT);
    glMultiDrawElementsIndirect(GL_TRIANGLES, sizeof(orx_index_t) == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, (void*)offset, cmdbuf_count, 0);

    g_r.fence[g_r.phase] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    // reset cpu drawlist for the next frame
    g_r.phase = (g_r.phase + 1) % 3;
    g_r.uniforms_vmap.head = g_r.phase * sizeof(orx_shader_data_t) + offsetof(orx_shader_data_t, data);
    g_r.draw_indirect_vmap.head = g_r.phase * (ORX_DRAW_INDIRECT_MAP_SIZE / 3);

    lu_timestamp start = lu_time_get();
    GLenum err = glClientWaitSync(g_r.fence[g_r.phase], GL_SYNC_FLUSH_COMMANDS_BIT, ORX_MAX_SYNC_TIMEOUT_NANOSEC);
    int64_t sync_time_ns = lu_time_elapsed(start);
    if (err == GL_TIMEOUT_EXPIRED) {
        printf("Error: something is wrong with the gpu fences: sync blocked for more than 5ms.\n");
    } else if (err == GL_CONDITION_SATISFIED) {
        printf("Warning: gpu fence blocked for %llu ns.\n", sync_time_ns);
    }
}

void orx_spine_update(orx_spine_t *self, float delta_sec)
{
    spAnimationState_update(self->anim, delta_sec);
	spAnimationState_apply(self->anim, self->skel);
	spSkeleton_update(self->skel, delta_sec);
	spSkeleton_updateWorldTransform(self->skel, SP_PHYSICS_UPDATE);
}

void orx_spine_draw(orx_spine_t *self)
{
    static orx_index_t quad_indices[] = {0, 1, 2, 2, 3, 0};
    static spSkeletonClipping *g_clipper = NULL;
	if (!g_clipper) {
		g_clipper = spSkeletonClipping_create();
	}

    orx_vertex_t vertbuf[2048];
    orx_index_t indibuf[2048];
    orx_vertex_t *vertices = vertbuf;
    orx_index_t *indices = indibuf;
    int slot_idx_count = 0;
    int slot_vtx_count = 0;
    float *uv = NULL;
	spSkeleton *sk = self->skel;
	for (int i = 0; i < sk->slotsCount; ++i) {
		spSlot *slot = sk->drawOrder[i];
		spAttachment *attachment = slot->attachment;
		if (!attachment) {
			spSkeletonClipping_clipEnd(g_clipper, slot);
			continue;
		}

		if (slot->color.a == 0 || !slot->bone->active) {
			spSkeletonClipping_clipEnd(g_clipper, slot);
			continue;
		}

		spColor *attach_color = NULL;

		if (attachment->type == SP_ATTACHMENT_REGION) {
			spRegionAttachment *region = (spRegionAttachment *)attachment;
			attach_color = &region->color;

			if (attach_color->a == 0) {
				spSkeletonClipping_clipEnd(g_clipper, slot);
				continue;
			}

            memcpy(indices, quad_indices, sizeof(quad_indices));
            slot_idx_count = 6;
			slot_vtx_count = 4;
			spRegionAttachment_computeWorldVertices(region, slot, (float*)vertices, 0, sizeof(orx_vertex_t) / sizeof(float));
            uv = region->uvs;
			self->node.tex = ((orx_image_t *) (((spAtlasRegion *) region->rendererObject)->page->rendererObject))->tex;
		} else if (attachment->type == SP_ATTACHMENT_MESH) {
			spMeshAttachment *mesh = (spMeshAttachment *) attachment;
			attach_color = &mesh->color;

			// Early out if the slot color is 0
			if (attach_color->a == 0) {
				spSkeletonClipping_clipEnd(g_clipper, slot);
				continue;
			}

            slot_vtx_count = mesh->super.worldVerticesLength / 2;
			spVertexAttachment_computeWorldVertices(SUPER(mesh), slot, 0, slot_vtx_count * 2, (float*)vertices, 0, sizeof(orx_vertex_t) / sizeof(float));
            uv = mesh->uvs;
            memcpy(indices, mesh->triangles, mesh->trianglesCount * sizeof(*indices));
            slot_idx_count = mesh->trianglesCount;
			self->node.tex = ((orx_image_t *) (((spAtlasRegion *) mesh->rendererObject)->page->rendererObject))->tex;
		} else if (attachment->type == SP_ATTACHMENT_CLIPPING) {
			spClippingAttachment *clip = (spClippingAttachment *) slot->attachment;
			spSkeletonClipping_clipStart(g_clipper, slot, clip);
			continue;
		} else {
			continue;
        }

        uint32_t color = (uint8_t)(sk->color.r * slot->color.r * attach_color->r * 255);
        color |= ((uint8_t)(sk->color.g * slot->color.g * attach_color->g * 255) << 8);
        color |= ((uint8_t)(sk->color.b * slot->color.b * attach_color->b * 255) << 16);
        color |= ((uint8_t)(sk->color.a * slot->color.a * attach_color->a * 255) << 24);

        for (int v = 0; v < slot_vtx_count; ++v) {
            vertices[v].u = *uv++;
            vertices[v].v = *uv++;
            vertices[v].color = color;
        }

		if (spSkeletonClipping_isClipping(g_clipper)) {
            // TODO: Optimize but first try with spine-cpp-lite compiled as .so for C
            spSkeletonClipping_clipTriangles(g_clipper, (float*)vertices, slot_vtx_count * 2, indices, slot_idx_count, &vertices->u, sizeof(*vertices));
            slot_vtx_count = g_clipper->clippedVertices->size >> 1;
            orx_vertex_t *vtxit = vertices;
            float *xyit = g_clipper->clippedVertices->items;
            float *uvit = g_clipper->clippedUVs->items;
            for (int j = 0; j < slot_vtx_count; ++j) {
                vtxit->x = *xyit++;
                vtxit->u = *uvit++;
                vtxit->y = *xyit++;
                (vtxit++)->v = *uvit++;
            }
            slot_idx_count = g_clipper->clippedTriangles->size;
            memcpy(indices, g_clipper->clippedTriangles->items, slot_idx_count * sizeof(*indices));
		}

        orx_shape_t shape = orx_gfx_add_mesh(vertices, slot_vtx_count * sizeof(orx_vertex_t), indices, slot_idx_count * sizeof(orx_index_t));
        shape.material_idx = orx_gfx_add_material((orx_material_t){
            .apx = self->node.pos_x, .apy = self->node.pos_y,
            .asx = self->node.scale_x, .asy = self->node.scale_y,
            .arot = self->node.rotation, .tex = self->node.tex
        });

        orx_gfx_submit(shape);

        /*
        switch (slot->data->blendMode) {
            case SP_BLEND_MODE_NORMAL:
                break;
            case SP_BLEND_MODE_MULTIPLY:
                break;
            case SP_BLEND_MODE_ADDITIVE:
                break;
            case SP_BLEND_MODE_SCREEN:
                break;
                */
        vertices = vertbuf;
        indices = indibuf;
        slot_vtx_count = 0;
        slot_idx_count = 0;

		spSkeletonClipping_clipEnd(g_clipper, slot);
	}
	spSkeletonClipping_clipEnd2(g_clipper);
}

void _spAtlasPage_createTexture(spAtlasPage *self, const char *path)
{
    (void)path;
    assert(self->atlas->rendererObject);
    self->rendererObject = self->atlas->rendererObject;
#ifdef ORX_VERBOSE
    printf("Spine atlas: %s\n", ((orx_image_t*)self->rendererObject)->path);
#endif
}

void _spAtlasPage_disposeTexture(spAtlasPage *self)
{
    (void)self;
}

char *_spUtil_readFile(const char *path, int *length)
{
    return _spReadFile(path, length);
}

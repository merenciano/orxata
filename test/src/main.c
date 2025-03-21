#include "orxata.h"
#include "spine/AnimationState.h"
#include "spine/Skeleton.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <llulu/lu_time.h>
#include <llulu/lu_math.h>
#include <spine/spine.h>

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#ifdef ORX_DEBUG
#define APP_PATHETIC_SHUTDOWN
#endif
#define APP_HALF_ASS_INIT

static const orx_vertex_t QUAD_VERTICES[] = {
    { .x = -1.0f, .y = -1.0f,
      .u = 0.0f, .v = 0.0f,
      .color = 0xFFFF00FF },
    { .x = -1.0f, .y = 1.0f,
      .u = 0.0f, .v = 1.0f,
      .color = 0xFF00FFFF },
    { .x = 1.0f, .y = 1.0f,
      .u = 1.0f, .v = 1.0f,
      .color = 0xFFFFFF00 },
    { .x = 1.0f, .y = -1.0f,
      .u = 1.0f, .v = 0.0f,
      .color = 0xFF00FF00 }
};

static const orx_index_t QUAD_INDICES[] = { 0, 1, 2, 0, 2, 3 };

/* TODO: To app_ctx */
struct {
    int64_t frame_count;
    int64_t glfw_init_ns;
    int64_t orx_init_ns;
    int64_t stb_img_load_ns;
    int64_t frame_time[256];
    int64_t init_time_ns;
    int64_t shutdown_time_ns;
    int64_t total_time_ns;
} timer_data;

struct {
    const char *name;

    /* SO window */
    char window_title[1024];
    int window_width;
    int window_height;
    bool vsync;
    /* Render surface */
    orx_canvas_t canvas;
    /* SO inputs */
    float mouse_x;
    float mouse_y;
    bool close;
} app_ctx;

static inline void print_timer_data();

GLFWwindow *g_window;

void on_canvas_resized(GLFWwindow* window, int width, int height)
{
    app_ctx.canvas.w = width;
    app_ctx.canvas.h = height;
}

void on_window_close(GLFWwindow* window)
{
    app_ctx.close = true;
}

void on_mouse_motion(GLFWwindow* window, double x, double y)
{
    app_ctx.mouse_x = (float)x;
    app_ctx.mouse_y = (float)y;
}

int main(int argc, char **argv)
{
    app_ctx.name = "XEE PROVEM";
    app_ctx.window_width = 2280;
    app_ctx.window_height = 1860;
    app_ctx.vsync = false;
    app_ctx.canvas = (orx_canvas_t){.clear_color = true, .clear_depth = false, .clear_stencil = false,
        .bg_col = {0.3f, 0.0f, 0.2f}};

    lu_timestamp start_time = lu_time_get();
    lu_timestamp timer = start_time;
    if (!glfwInit()) {
        printf("Could not init glfw. Aborting...\n");
        return 1;
    }

#ifndef APP_HALF_ASS_INIT
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);
#endif

    g_window = glfwCreateWindow(app_ctx.window_width, app_ctx.window_height, app_ctx.window_title, NULL, NULL);
    if (!g_window) {
        glfwTerminate();
        printf("Could not open glfw window. Aborting...\n");
        return 1;
    }

    glfwGetFramebufferSize(g_window, &app_ctx.canvas.w, &app_ctx.canvas.h);

    glfwSetWindowCloseCallback(g_window, on_window_close);
    glfwSetFramebufferSizeCallback(g_window, on_canvas_resized);
    glfwSetCursorPosCallback(g_window, on_mouse_motion);
    glfwMakeContextCurrent(g_window);
    glfwSwapInterval((int)app_ctx.vsync);

    orx_config_t cfg = {
        .gl_loader = (void *(*)(const char *))glfwGetProcAddress,
        .canvas = app_ctx.canvas,
        .seconds_between_shader_file_changed_checks = 1.0f,
        .vert_shader_path = "./assets/vert.glsl",
        .frag_shader_path = "./assets/frag.glsl",
    };

    int64_t elapsed = lu_time_elapsed(timer);
    timer_data.glfw_init_ns = elapsed;
    timer = lu_time_get();

    /* TODO: Asset map with user-defined paths */
    orx_image_t owl_atlas = orx_load_image("./assets/owl.png");
    orx_image_t windmill_atlas = orx_load_image("./assets/windmill.png");
    orx_image_t tex_test0 = orx_load_image("./assets/tex_test_0.png");
    orx_image_t tex_test1 = orx_load_image("./assets/tex_test_1.png");
    orx_image_t tex_test2 = orx_load_image("./assets/tex_test_2.png");
    orx_image_t tex_test3 = orx_load_image("./assets/tex_test_3.png");

    elapsed = lu_time_elapsed(timer);
    timer_data.stb_img_load_ns = elapsed;
    timer = lu_time_get();
    
    orx_init(&cfg);

    orx_texture_set(owl_atlas.tex, owl_atlas.data);
    stbi_image_free(owl_atlas.data);
    owl_atlas.data = NULL;

    orx_texture_set(windmill_atlas.tex, windmill_atlas.data);
    stbi_image_free(windmill_atlas.data);
    windmill_atlas.data = NULL;

    orx_texture_set(tex_test0.tex, tex_test0.data);
    stbi_image_free(tex_test0.data);
    tex_test0.data = NULL;

    orx_texture_set(tex_test1.tex, tex_test1.data);
    stbi_image_free(tex_test1.data);
    tex_test1.data = NULL;

    orx_texture_set(tex_test2.tex, tex_test2.data);
    stbi_image_free(tex_test2.data);
    tex_test2.data = NULL;

    orx_texture_set(tex_test3.tex, tex_test3.data);
    stbi_image_free(tex_test3.data);
    tex_test3.data = NULL;

    orx_node_t nodes[] = {
        {
            .pos_x = -10.5f,
            .pos_y = -10.0f,
            .scale_x = 200.0f,
            .scale_y = 200.0f,
            .rotation = 0.0f,
            .tex = tex_test0.tex,
#ifdef ORX_DEBUG
            .name = "Node0"
#endif
        },
        {
            .pos_x = 30.0f,
            .pos_y = 60.0f,
            .scale_x = 100.0f,
            .scale_y = 100.0f,
            .rotation = 0.0f,
            .tex = tex_test1.tex,
#ifdef ORX_DEBUG
            .name = "Node1"
#endif
        },
        {
            .pos_x = 600.0f,
            .pos_y = 400.0f,
            .scale_x = 100.0f,
            .scale_y = 100.0f,
            .rotation = 0.0f,
            .tex = tex_test2.tex,
#ifdef ORX_DEBUG
            .name = "Node2"
#endif
        },
        {
            .pos_x = -200.0f,
            .pos_y = 200.0f,
            .scale_x = 100.0f,
            .scale_y = 100.0f,
            .rotation = 0.0f,
            .tex = tex_test3.tex,
#ifdef ORX_DEBUG
            .name = "Node3"
#endif
        },
    };

    elapsed = lu_time_elapsed(timer);
    timer_data.orx_init_ns = elapsed;

    spBone_setYDown(-1);
    spAtlas *sp_atlas = spAtlas_createFromFile("./assets/owl.atlas", &owl_atlas);
    spSkeletonJson *skel_json = spSkeletonJson_create(sp_atlas);
    spSkeletonData *skel_data = spSkeletonJson_readSkeletonDataFile(skel_json, "./assets/owl.json");
    if (!skel_data) {
        printf("%s\n", skel_json->error);
        return 1;
    }
    spAnimationStateData *anim_data = spAnimationStateData_create(skel_data);
    orx_spine_t spine_node = {.skel = spSkeleton_create(skel_data), .anim = spAnimationState_create(anim_data)};
    spSkeleton_setToSetupPose(spine_node.skel);

    spine_node.node.tex = owl_atlas.tex;
    spSkeleton_updateWorldTransform(spine_node.skel, SP_PHYSICS_UPDATE);

    spine_node.node.pos_x = 400.0f;
    spine_node.node.pos_y = 400.0f;
    spine_node.node.scale_x = 0.6f;
    spine_node.node.scale_y = 0.6f;
    spine_node.node.rotation = 0.0f;
    spAnimationState_setAnimationByName(spine_node.anim, 0, "idle", true);
    spAnimationState_setAnimationByName(spine_node.anim, 1, "blink", true);

    spTrackEntry *left = spAnimationState_setAnimationByName(spine_node.anim, 2, "left", true);
	spTrackEntry *right = spAnimationState_setAnimationByName(spine_node.anim, 3, "right", true);
	spTrackEntry *up = spAnimationState_setAnimationByName(spine_node.anim, 4, "up", true);
	spTrackEntry *down = spAnimationState_setAnimationByName(spine_node.anim, 5, "down", true);

	left->alpha = 0;
	left->mixBlend = SP_MIX_BLEND_ADD;
	right->alpha = 0;
	right->mixBlend = SP_MIX_BLEND_ADD;
	up->alpha = 0;
	up->mixBlend = SP_MIX_BLEND_ADD;
	down->alpha = 0;
	down->mixBlend = SP_MIX_BLEND_ADD;

    /* Windmill */
    spAtlas *windmill_atlas_file = spAtlas_createFromFile("./assets/windmill.atlas", &windmill_atlas);
    spSkeletonJson *windmill_skel_json = spSkeletonJson_create(windmill_atlas_file);
    spSkeletonData *windmill_skel_data = spSkeletonJson_readSkeletonDataFile(windmill_skel_json, "./assets/windmill.json");
    if (!windmill_skel_data) {
        printf("%s\n", windmill_skel_json->error);
        return 1;
    }
    spAnimationStateData *windmill_anim_data = spAnimationStateData_create(windmill_skel_data);
    orx_spine_t windmill_spine = {.skel = spSkeleton_create(windmill_skel_data), .anim = spAnimationState_create(windmill_anim_data)};
    spSkeleton_setToSetupPose(windmill_spine.skel);
    windmill_spine.node.tex = windmill_atlas.tex;
    spSkeleton_updateWorldTransform(windmill_spine.skel, SP_PHYSICS_UPDATE);
    windmill_spine.node.pos_x = -400.0f;
    windmill_spine.node.pos_y = -400.0f;
    windmill_spine.node.scale_x = 0.4f;
    windmill_spine.node.scale_y = 0.4f;
    windmill_spine.node.rotation = 0.0f;
    spAnimationState_setAnimationByName(windmill_spine.anim, 0, "animation", true);

    timer_data.init_time_ns = lu_time_elapsed(start_time);
    timer = lu_time_get();

    while(!app_ctx.close) {
        glfwGetWindowSize(g_window, &app_ctx.window_width, &app_ctx.window_height);
        float x = app_ctx.mouse_x / app_ctx.window_width;
        float y = app_ctx.mouse_y / app_ctx.window_height;
        left->alpha = (lu_maxf(x, 0.5f) - 0.5f) * 1.0f;
        right->alpha = (0.5f - lu_minf(x, 0.5f)) * 1.0f;
        down->alpha = (lu_maxf(y, 0.5f) - 0.5f) * 1.0f;
        up->alpha = (0.5f - lu_minf(y, 0.5f)) * 1.0f;

        spSkeleton_setToSetupPose(spine_node.skel);
        orx_spine_update(&spine_node, lu_time_sec(elapsed));  /* Delta time in seconds to APP_CTX */
        orx_spine_update(&windmill_spine, lu_time_sec(elapsed));

        float curr_time_sec = lu_time_sec(lu_time_elapsed(start_time));
        const float sin_time = sinf(curr_time_sec);
        nodes[0].scale_x = 100.0f + sin_time * 50.0f;
        nodes[0].scale_y = 100.0f + cosf(curr_time_sec) * 50.0f;
        nodes[2].rotation = curr_time_sec;

        orx_gfx_sync();

        orx_spine_draw(&windmill_spine);
        orx_mesh_t mesh = orx_gfx_add_mesh(QUAD_VERTICES, sizeof(QUAD_VERTICES),
                                           QUAD_INDICES,  sizeof(QUAD_INDICES));
        for (int i = 0; i < 4; ++i) {
            orx_draw_idx draw_index = orx_gfx_add_material((orx_material_t){
                .apx = nodes[i].pos_x,
                .apy = nodes[i].pos_y,
                .asx = nodes[i].scale_x,
                .asy = nodes[i].scale_y,
                .arot = nodes[i].rotation,
                .tex = nodes[i].tex
            });
            orx_gfx_submit(mesh, draw_index);
        }

        orx_spine_draw(&spine_node);
        orx_render(&app_ctx.canvas);
        glfwSwapBuffers(g_window);

        elapsed = lu_time_elapsed(timer);
        timer = lu_time_get();

        timer_data.frame_time[timer_data.frame_count % 256] = elapsed;
        timer_data.frame_count++;

        sprintf(app_ctx.window_title, "%s  |  %f fps", app_ctx.name, 1.0f / lu_time_sec(elapsed));
        glfwSetWindowTitle(g_window, app_ctx.window_title);
        glfwPollEvents();
    }

    timer = lu_time_get();

#ifdef APP_PATHETIC_SHUTDOWN
    glfwDestroyWindow(g_window);
    glfwTerminate();
#endif /* APP_PATHETIC_SHUTDOWN */

    timer_data.shutdown_time_ns = lu_time_elapsed(timer);
    timer_data.total_time_ns = lu_time_elapsed(start_time);

#ifdef ORX_VERBOSE
    print_timer_data();
#endif
    return 0;
}

static inline void print_timer_data()
{
    printf(" * Report (%lld frames, %.2f seconds) * \n", timer_data.frame_count, lu_time_sec(timer_data.total_time_ns));
    printf("Init time: %lld ms\n", lu_time_ms(timer_data.init_time_ns));
    printf(" - glfw:\t%lld ms\n", lu_time_ms(timer_data.glfw_init_ns));
    printf(" - stb_img:\t%lld ms\n", lu_time_ms(timer_data.stb_img_load_ns));
    printf(" - orxata:\t%lld ms\n", lu_time_ms(timer_data.orx_init_ns));
    int64_t sum = 0;
    for (int i = 0; i < 256; ++i) {
        sum += timer_data.frame_time[i];
    }
    sum /= 256;
    double sum_d = sum;
    printf("Last 256 frame times average: %.2f ms\n", sum / 1000000.0f);
    printf("Shutdown: %lld ms\n", lu_time_ms(timer_data.shutdown_time_ns));
}

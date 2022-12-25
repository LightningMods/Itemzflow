/*
    GLES2 ItemzFlow
    my implementation of coverflow, from scratch.
    (shaders beware)
    2021, masterzorag && LM
*/

#include "defines.h"
#include "GLES2_common.h"
#include "utils.h"
#include <atomic>
#include <pthread.h>
#if defined(__ORBIS__)
#include "shaders.h"
#include "net.h"
#include <signal.h>
#include "installpkg.h"
#else
#include "pc_shaders.h"
#endif
#include <errno.h>

#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#if defined(__ORBIS__)
#include "patcher.h"
extern int retry_mp3_count;
#endif

extern vec2 resolution;

GameStatus app_status = APP_NA_STATUS;
extern int numb_of_settings;

// redef for yaml-cpp
int isascii(int c)
{
    return !(c&~0x7f);
}

#define isascii(a) (0 ? isascii(a) : (unsigned)(a) < 128)
extern std::vector<std::string> ITEMZ_SETTING_TEXT;
extern std::atomic_long started_epoch;
std::unordered_map<std::string, std::atomic<int>> m;
#if defined(__ORBIS__)
// end of redef

// global variables for patcher
// Patcher by illusion0001
// with UI help from LM
// https://gist.github.com/gchudnov/c1ba72d45e394180e22f
std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
u32 patch = 0;
u32 num_title_id = 0;
u32 patch_current_index = 0;
std::string key_title = "title";
std::string key_app_ver = "app_ver";
std::string key_patch_ver = "patch_ver";
std::string key_name = "name";
std::string key_author = "author";
std::string key_note = "note";
std::string key_app_titleid = "app_titleid";
std::string key_app_elf = "app_elf";
u8 arr8[1];
u8 arr16[2];
u8 arr32[4];
u8 arr64[8];
u8 max_node = 2;
// patch data
std::string blank = " ";
std::string type;
u64 addr = 0;
std::ofstream cfg_data;
std::string patch_file;
std::string patch_applied;
std::string patch_path_base = "/data/GoldHEN";
bool is_first_eboot;
bool is_non_eboot_entry;
bool is_trainer_launch;
// bool is_trainer_page;
bool no_launch;
s32 itemflow_pid;
char patcher_notify_icon[] = "cxml://psnotification/tex_icon_system";
struct _trainer_struct trs;

const u32 max_tokens = 4096;

typedef struct {
    json_t mem[max_tokens];
    unsigned int nextFree;
    jsonPool_t pool;
} jsonStaticPool_t;

const char *type_byte = "byte";
const char *type_bytes16 = "bytes16";
const char *type_bytes32 = "bytes32";
const char *type_bytes64 = "bytes64";
const char *type_bytes = "bytes";
const char *type_float32 = "float32";
const char *type_float64 = "float64";
const char *type_utf8 = "utf8";
const char *type_utf16 = "utf16";

const char *key_app_type = "type";
const char *key_app_addr = "addr";
const char *key_app_value = "value";
const char *key_patch_list = "patch_list";

json_t const *json_key_title;
json_t const *json_key_app_ver;
json_t const *json_key_patch_ver;
json_t const *json_key_name;
json_t const *json_key_author;
json_t const *json_key_note;
json_t const *json_key_app_titleid;
json_t const *json_key_app_elf;

const char *title_val;
const char *app_ver_val;
const char *patch_ver_val;
const char *name_val;
const char *author_val;
const char *note_val;
const char *app_titleid_val;
const char *app_elf_val;
#endif
// ------------------------------------------------------- global variables ---
// the Settings
char ipc_msg[100];
extern ItemzSettings set,
    *get;
extern double u_t;
extern std::vector<std::string> gm_p_text;

static GLuint sh_prog1,
    sh_prog2,
    sh_prog3,
    sh_prog4,
    sh_prog5,
    rdisc_tex;

extern std::vector<item_t> all_apps;

vertex_buffer_t *cover_o, // object, with depth: 6 faces
    *cover_t,             // full coverbox, template
    *cover_i,             // just icon coverbox, overlayed
        *title_vbo = NULL,
        *rdisc_i,
    *trainer_btns = NULL, *launch_text = NULL, *as_text = NULL, *remove_music = NULL;
double rdisc_rz = 0.;
std::atomic_int inserted_disc = 0;

int v_curr = ITEMzFLOW;
static mat4 model,
    view,
    projection;

struct retry_t *cf_tex = NULL;
bool skipped_first_X = false;
// ------------------------------------------------------ freetype-gl shaders ---
typedef struct
{
    xyz position,
        normal;
    rgba color;
} v3f3f4f_t;

typedef struct
{
    xyz position;
    st tex_uv;
    rgba color;
} v3f2f4f_t;

vec4 c[8];
int up_next = ITEMzFLOW;

GLuint btn_X = GL_NULL, btn_options = GL_NULL, btn_tri = GL_NULL, btn_down = GL_NULL,
       btn_up = GL_NULL, btn_right = GL_NULL, btn_left, btn_o = GL_NULL, btn_sq = GL_NULL, btn_l1 = GL_NULL, btn_r1 = GL_NULL, btn_l2 = GL_NULL;

bool Download_icons = true;
int old_curr = -1;
int ret = -1;
// better if odd to get one in the center, eh
#define NUM_OF_PIECES (7)

// one in the center: draw +/- item count: it's the `plusminus_range`
const int pm_r = (NUM_OF_PIECES - 1) / 2;

GLuint cb_tex = 0, // default coverbox textured image (empty: w/ no icon)
    bg_tex = 0;    // background textured image
int g_idx = pm_r,
    pr_dbg = 1; // just to debug once

typedef struct
{
    xyz position;
    rgba color;
} v3fc4f_t;

vec3 off = (vec3){1.05, 0, 0}; // item position offset


/* for animations */
extern "C" typedef struct ani_t
{
    double now,   // current time
        life;     // total duration
    vec3 v3_t,    // single translate vector
        v3_r;     // single rotation vector
    void *handle; // what we are animating?
    bool is_running;
} ani_t;

// we have two animation pointers, and the pointer to current one
std::vector<ani_t> ani;

/* view (matrix) vectors */
vec3 v0 = (vec3)(0),
     v1,
     v2;

// ---------------------------------------------------------------- display ---

/* model rotation angles and camera position */
float gx = 0.,
      gy = 90.,
      gz = 90.;
/*    xC =  0.,
      yC =  0.,
      zC = -5.; // camera z (-far-, +near) */

float testoff = 3.6;

static double g_Time = .0,
              xoff = .0;

// for Settings (options_panel)
bool use_reflection = true,
     use_pixelshader = true;

extern ivec4 menu_pos;

layout_t gm_p, ls_p, setting_p; // settings

#if defined(__ORBIS__)
extern std::atomic_int mins_played;
extern std::atomic_int AppVis;
#else
std::atomic_int mins_played = 2;
std::atomic_bool AppVis = 0;
#endif
bool current_app_is_fpkg = false;
bool is_dumpable = false;
std::string title;
std::string title_id;
std::string app_version;
std::string tmp;
std::string pkg_path;
std::string icon_path;
extern int setting_on;
int wait_time = 0;
bool confirm_close = false;

/************************ END OF GLOBALS ******************/

void render_button(GLuint btn, float h, float w, float x, float y, float multi)
{

    vec4 r;
    vec2 s = (vec2){w / multi, h / multi}, p = (resolution - s);
    p.y = y;
    p.x = x;
    s += p;
    // convert to normalized coordinates
    r.xy = px_pos_to_normalized(&p);
    r.zw = px_pos_to_normalized(&s);
    on_GLES2_Render_icon(USE_COLOR, btn, 2, &r, NULL);
}

void itemzcore_add_text(vertex_buffer_t* vert_buffer,float x, float y, std::string text)
{
    vec2 pen_s = (vec2){0., 0.};
    pen_s.x = x;
    pen_s.y = y;
    texture_font_load_glyphs(sub_font, text.c_str());
    add_text(vert_buffer, sub_font, text, &col, &pen_s);
}

static void append_to_vbo(vertex_buffer_t *vbo, vec3 *v)
{
    float s0 = 0., t0 = 0.,
          s1 = 1., t1 = 1.;
    GLuint idx[6] = {0, 1, 2, 0, 2, 3};
    v3f2f4f_t vtx[4]; 

    vtx[0].position = (xyz){(v + 0)->x, (v + 0)->y, (v + 0)->z};
    vtx[0].color = (rgba){c[0].r, c[0].g, c[0].b, c[0].a};
    vtx[0].tex_uv = (st){s1, t0};

    vtx[1].position = (xyz){(v + 1)->x, (v + 1)->y, (v + 1)->z};
    vtx[1].color = (rgba){c[1].r, c[1].g, c[1].b, c[1].a};
    vtx[1].tex_uv = (st){s0, t0};

    vtx[2].position = (xyz){(v + 2)->x, (v + 2)->y, (v + 2)->z};
    vtx[2].color = (rgba){c[2].r, c[2].g, c[2].b, c[2].a};
    vtx[2].tex_uv = (st){s0, t1};

    vtx[3].position = (xyz){(v + 3)->x, (v + 3)->y, (v + 3)->z};
    vtx[3].color = (rgba){c[3].r, c[3].g, c[3].b, c[3].a};
    vtx[3].tex_uv = (st){s1, t1};


    vertex_buffer_push_back(vbo, vtx, 4, idx, 6);
}

// add to vbo from rectangle, in px coordinates
extern "C" void vbo_add_from_rect(vertex_buffer_t *vbo, vec4 *rect)
{
    vec4 color = {1, 0, 0, 1};
    float r = color.r, g = color.g, b = color.b, a = color.a;
    /* VBO is setup as: "vertex:3f, color:4f */
    v3f2f4f_t vtx[4];

    vtx[0].position = {rect->x, rect->y, 0};
    vtx[1].position = {rect->x, rect->w, 0};
    vtx[2].position = {rect->z, rect->w, 0};
    vtx[3].position = {rect->z, rect->y, 0};

    vtx[0].color = vtx[1].color = vtx[2].color = vtx[3].color  = {r, g, b, a};

    // two triangles: 2 * 3 vertex
    GLuint idx[6] = {0, 1, 2, 0, 2, 3};
    vertex_buffer_push_back(vbo, vtx, 4, idx, 6);
}
void realloc_app_retries()
{
    if (cf_tex)
    {
        free(cf_tex);
        cf_tex = (struct retry_t *)calloc(all_apps[0].token_c /* 1st is reseved */ + 1, sizeof(retry_t));
    }
}
void InitScene_5(int width, int height)
{
    glFrontFace(GL_CCW);

    fs_ret.status = FS_NOT_STARTED;
    fs_ret.last_v = RANDOM;
    fs_ret.fs_func = NULL;
    fs_ret.last_layout = NULL;
    fs_ret.filter = FS_NONE;

    // reset view
    v2 = v1 = v0;

    ani.resize(1);
    ani[0].is_running = false;

    // unused normals
    vec3 n[6];
    n[0] = (vec3){0, 0, 1},
    n[1] = (vec3){1, 0, 0},
    n[2] = (vec3){0, 1, 0},
    n[3] = (vec3){-1, 0, 1},
    n[4] = (vec3){0, -1, 0},
    n[5] = (vec3){0, 0, -1};

    // setup color palette
    c[0] = (vec4){1, 1, 1, 1},               // 1. white   TL
        c[1] = (vec4){1, 1, 0, 1},           // 2. yellow  BL (fix bottom)
        c[2] = (vec4){1, 0, 1, 1},           // 3. purple  BR (fix bottom)
        c[3] = (vec4){0, 1, 1, 1},           // 4. cyan    TR
        c[4] = (vec4){1, 0, 0, 1},           // 5: red
        c[5] = (vec4){0, 0.1145, 0.9412, 1}, // 6. blue
        c[6] = (vec4){0, 1, 0, 1},           // 7. green
        c[7] = (vec4){0, 0, 0, 1};           // 8. black

    // the object
    cover_o = vertex_buffer_new("vertex:3f,color:4f");
    // the textuted objects
    cover_t = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f"); // owo template
    cover_i = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f"); // single diff item

    if (1) /* the single rotating disc icon texture (overlayed) */
    {
        rdisc_i = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f"); // rotating disc icon

        vec3 rdisc_vtx[4];
        // keep center dividing by 2.f, CW, squared texture
        rdisc_vtx[0] = (vec3){ .7 /2.,  .7 /2., .0 /2.};  // up R
        rdisc_vtx[1] = (vec3){-.7 /2.,  .7 /2., .0 /2.};  // up L
        rdisc_vtx[2] = (vec3){-.7 /2., -.7 /2., .0 /2.};  // bm L
        rdisc_vtx[3] = (vec3){ .7 /2., -.7 /2., .0 /2.};  // bm R
        append_to_vbo(rdisc_i, &rdisc_vtx[0]);
    }

    // one rect
    for (int i = 0; i < 1 /* ONE_PIECE */; i++)
    {
        /* the single different item texture (overlayed) */
        vec3 cover_item_v[8];
        // keep center dividing by 2.f, CW
        cover_item_v[0] = (vec3){ .956 /2.,   .925 /2., .0 /2.};  // up R
        cover_item_v[1] = (vec3){-.991 /2.,   .925 /2., .0 /2.};  // up L
        cover_item_v[2] = (vec3){-.991 /2., -1.195 /2., .0 /2.};  // bm L
        cover_item_v[3] = (vec3){ .956 /2., -1.195 /2., .0 /2.};  // bm R
        append_to_vbo(cover_i, &cover_item_v[0]);
        /*
          xyz  v[] = { { 1, 1, 1},  {-1, 1, 1},  {-1,-1, 1}, { 1,-1, 1},
                       { 1,-1,-1},  { 1, 1,-1},  {-1, 1,-1}, {-1,-1,-1} };
        */
        vec3 cover_v[8];
        // keep center dividing by 2.f
        cover_v[0] = (vec3){1. / 2., 1.25 / 2., .0 / 2.};
        cover_v[1] = (vec3){-1. / 2., 1.25 / 2., .0 / 2.};
        cover_v[2] = (vec3){-1. / 2., -1.25 / 2., .0 / 2.};
        cover_v[3] = (vec3){1. / 2., -1.25 / 2., .0 / 2.};
        // for shared/template cover box
        append_to_vbo(cover_t, &cover_v[0]);

        // for the other 5 faces of the object
        cover_v[4] = (vec3){1. / 2., -1.25 / 2., -.2 / 2.};
        cover_v[5] = (vec3){1. / 2., 1.25 / 2., -.2 / 2.};
        cover_v[6] = (vec3){-1. / 2., 1.25 / 2., -.2 / 2.};
        cover_v[7] = (vec3){-1. / 2., -1.25 / 2., -.2 / 2.};

        v3fc4f_t cover_vtx[24] = {
            // back
            {{cover_v[6].x, cover_v[6].y, cover_v[6].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            /* w */ {{cover_v[7].x, cover_v[7].y, cover_v[7].z}, {c[4].r, c[4].g, c[4].b, c[4].a}}, // red
            {{cover_v[4].x, cover_v[4].y, cover_v[4].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            {{cover_v[5].x, cover_v[5].y, cover_v[5].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            // top 056, 061
            {{cover_v[0].x, cover_v[0].y, cover_v[0].z}, {c[0].r, c[0].g, c[0].b, c[0].a}}, // white
            {{cover_v[5].x, cover_v[5].y, cover_v[5].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            {{cover_v[6].x, cover_v[6].y, cover_v[6].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            {{cover_v[1].x, cover_v[1].y, cover_v[1].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            // bottom
            {{cover_v[3].x, cover_v[3].y, cover_v[3].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            {{cover_v[4].x, cover_v[4].y, cover_v[4].z}, {c[0].r, c[0].g, c[0].b, c[0].a}}, // white
            {{cover_v[7].x, cover_v[7].y, cover_v[7].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            {{cover_v[2].x, cover_v[2].y, cover_v[2].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            // Right
            {{cover_v[0].x, cover_v[0].y, cover_v[0].z}, {c[5].r, c[5].g, c[5].b, c[5].a}}, // white
            {{cover_v[3].x, cover_v[3].y, cover_v[3].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            {{cover_v[4].x, cover_v[4].y, cover_v[4].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            {{cover_v[5].x, cover_v[5].y, cover_v[5].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            // left
            {{cover_v[6].x, cover_v[6].y, cover_v[6].z}, {c[5].r, c[5].g, c[5].b, c[5].a}}, //
            {{cover_v[7].x, cover_v[7].y, cover_v[7].z}, {c[5].r, c[5].g, c[5].b, c[5].a}}, //
            {{cover_v[2].x, cover_v[2].y, cover_v[2].z}, {c[5].r, c[5].g, c[5].b, c[5].a}}, //
            {{cover_v[1].x, cover_v[1].y, cover_v[1].z}, {c[5].r, c[5].g, c[5].b, c[5].a}}, //
            // front
            {{cover_v[1].x, cover_v[1].y, cover_v[1].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            {{cover_v[2].x, cover_v[2].y, cover_v[2].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            {{cover_v[3].x, cover_v[3].y, cover_v[3].z}, {c[5].r, c[5].g, c[5].b, c[5].a}},
            {{cover_v[0].x, cover_v[0].y, cover_v[0].z}, {c[0].r, c[0].g, c[0].b, c[0].a}} // white
        };

        // define front to origin, ClockWise
        GLuint cover_idx[36] = {
            0, 1, 2, 0, 2, 3,       // Front: 012, 023 CCW -> not drawn
            4, 5, 6, 4, 6, 7,       // R:     034, 045 CCW ->
            8, 9, 10, 8, 10, 11,    // Top:   056, 061
            12, 13, 14, 12, 14, 15, // L:     167, 172 green
            16, 17, 18, 16, 18, 19, // Bot:   743, 732
            20, 21, 22, 20, 22, 23  // Back: 476 rwb, 465 rbb -> CW is drawn
        };

        vertex_buffer_push_back(cover_o, cover_vtx, 24, cover_idx, 36);

        // we have shapes/objects
    }

#if defined(__ORBIS__)
    if (!cb_tex)
        cb_tex = load_png_asset_into_texture(asset_path("cover.png"));
#else
    if (!cb_tex)
        cb_tex = load_png_data_into_texture(cover_temp, cover_temp_length);
#endif

    // the fullscreen image
    if (!get->setting_bools[using_sb] && get->setting_bools[has_image] && bg_tex == GL_NULL)
        bg_tex = load_png_asset_into_texture(get->setting_strings[IMAGE_PATH].c_str());

    // we will need at max all_apps count coverbox
    if (!cf_tex)
        cf_tex = (struct retry_t *)calloc(all_apps[0].token_c /* 1st is reseved */ + 1, sizeof(retry_t));

    if (!rdisc_tex)
        rdisc_tex = load_png_asset_into_texture(asset_path("disc.png"));

    // all done
}

// ------------------------------------------------------------------- init ---

void InitScene_4(int width, int height)
{
    // compile, link and use custom shaders
    if (!sh_prog1)
        sh_prog1 = BuildProgram(coverflow_vs0, coverflow_fs0, coverflow_vs0_length, coverflow_fs0_length);

    // default one
    if (!sh_prog2)
        sh_prog2 = BuildProgram(coverflow_vs1, coverflow_fs1, coverflow_vs1_length, coverflow_fs1_length);

    // for reflection
    if (!sh_prog3)
        sh_prog3 = BuildProgram(coverflow_vs2, coverflow_fs2, coverflow_vs2_length, coverflow_fs2_length);

    // for fake disc shadow
    if (!sh_prog4)
        sh_prog4 = BuildProgram(coverflow_vs1, coverflow_fs3, coverflow_vs1_length, coverflow_fs3_length);

    // for fake disc shadow
    if (!sh_prog5)
        sh_prog5 = BuildProgram(coverflow_vs4, coverflow_fs4, coverflow_vs4_length, coverflow_fs4_length);

    mat4_set_identity(&projection);
    mat4_set_identity(&model);
    mat4_set_identity(&view);

    // reshape
    glViewport(0, 0, width, height);
    mat4_set_perspective(&projection, 60.0f, width / (float)height, 1.0, 10.0);
}

extern GLuint shader;
static void render_tex(GLuint texture, int idx, vertex_buffer_t *vbo, vec4 col)
{
    // we already clean in main renderloop()!

    GLuint sh_prog = sh_prog2;

    if (idx == -1) // reflection
        sh_prog = sh_prog3;
    else // fake shadow
    if (idx == -2)
        sh_prog = sh_prog4;
    else
    if (idx == -3)
        sh_prog = sh_prog5;

    glUseProgram(sh_prog);

    int num = 2;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0 + num);
    glBindTexture(GL_TEXTURE_2D, texture);
    {
        glUniform1i(glGetUniformLocation(sh_prog, "texture"), num);
        glUniformMatrix4fv(glGetUniformLocation(sh_prog, "model"), 1, 0, model.data);
        glUniformMatrix4fv(glGetUniformLocation(sh_prog, "view"), 1, 0, view.data);
        glUniformMatrix4fv(glGetUniformLocation(sh_prog, "projection"), 1, 0, projection.data);

        glUniform4f(glGetUniformLocation(sh_prog, "Color"), col.r, col.g, col.b, col.a);

        /* draw whole VBO (storing all added) */
        vertex_buffer_render(vbo, GL_TRIANGLES);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

static bool reset_tex_slot(int idx)
{
    bool res = false;
    if (cf_tex[idx].tex > 0)
    {
        //      log_debug( "%s[%3d]: %3d, %3d", __FUNCTION__, idx, cf_tex[ idx ].tex, all_apps[ idx ].texture);
        if (cf_tex[idx].tex != cb_tex) // keep the coverbox template
        {
            //          log_debug( "%s[%3d]: (%d)  %d ", __FUNCTION__, idx, cf_tex[ idx ].tex, all_apps[ idx ].texture);
            glDeleteTextures(1, &cf_tex[idx].tex), cf_tex[idx].tex = 0;
            res = true;
        }
        else
        {
            if (all_apps[idx].texture > GL_NULL && all_apps[idx].texture != fallback_t) // keep icon0 template
            {                                                                           // discard icon0.png
                                                                                        //              log_debug( "%s[%3d]:  %d  (%d)", __FUNCTION__, idx, cf_tex[ idx ].tex, all_apps[ idx ].texture);
                glDeleteTextures(1, &all_apps[idx].texture), all_apps[idx].texture = GL_NULL;
                res = true;
            }
        }
    }

    return res;
}

void drop_some_coverbox(void)
{
    int count = 0;
    bool locked_by_func = false;

    if (pthread_mutex_trylock(&disc_lock) == 0)
    {
        locked_by_func = true;
        log_warn("Mutex not locked!, Locked it");
    }

    if (all_apps[0].token_c < 100)
    {
        pthread_mutex_unlock(&disc_lock);
        return;
    }

    for (int i = 1; i < all_apps[0].token_c + 1; i++)
    {
        if (i > g_idx - pm_r - 1 && i < g_idx + pm_r + 1)
            continue;

        if (reset_tex_slot(i))
            count++;
    }

    if (locked_by_func)
        pthread_mutex_unlock(&disc_lock);

    if (count > 0)
        log_info("%s discarded %d textures", __FUNCTION__, count);
}

void check_tex_for_reload(int idx)
{
    std::string cb_path;

    bool locked_by_func = false;
    if (pthread_mutex_trylock(&disc_lock) == 0)
    {
        locked_by_func = true;
        log_warn("Mutex not locked!, Locked it");
    }
    int w, h;
    cb_path = fmt::format("{}/{}.png", APP_PATH("covers"), all_apps[idx].id);

    if (!if_exists(cb_path.c_str()) && !is_png_vaild(cb_path.c_str(), &w, &h))
    {
        log_info("%s doesnt exist", cb_path.c_str());
        cf_tex[idx].exists = false;
    }
    else
        cf_tex[idx].exists = true;

    if (locked_by_func)
        pthread_mutex_unlock(&disc_lock);
}

void check_n_load_textures(int idx)
{
    bool locked_by_func = false;
    if (pthread_mutex_trylock(&disc_lock) == 0)
    {
        locked_by_func = true;
        log_warn("Mutex not locked!, Locked it");
    }

    if (!cf_tex[idx].tex)
    {
        //log_info("%s: %s", __FUNCTION__, all_apps[idx].id.c_str());
        
        std::string cb_path = fmt::format("{}/{}.png", APP_PATH("covers"), all_apps[idx].id);
        
        //log_info(cb_path.c_str());
        cf_tex[idx].tex = cb_tex;
        int w, h;
        if (cf_tex[idx].exists && is_png_vaild(cb_path.c_str(), &w, &h))
            cf_tex[idx].tex = load_png_asset_into_texture(cb_path.c_str());
    }
    if (cf_tex[idx].tex == 0)
        cf_tex[idx].tex = cb_tex; // as final fallback

    if (locked_by_func)
        pthread_mutex_unlock(&disc_lock);
}

#if defined(__ORBIS__)

static int download_texture(int idx)
{
    bool locked_by_func = false;
    if (pthread_mutex_trylock(&disc_lock) == 0)
    {
        locked_by_func = true;
        log_warn("Mutex not locked!, Locked it");
    }
     
    if (1)
    {
        std::string cb_path = fmt::format("{}/{}.png", APP_PATH("covers"), all_apps[idx].id);

        log_info("Trying to Download cover for %s", all_apps[idx].id.c_str());

        loadmsg(getLangSTR(DL_COVERS));
        int w, h = 0;
        if (!if_exists(cb_path.c_str()) || !is_png_vaild(cb_path.c_str(), &w, &h)) // download
        {
            cf_tex[idx].exists = false;
            std::string cb_url = fmt::format("https://api.pkg-zone.com/download?cover_tid={}", all_apps[idx].id);

            int ret = dl_from_url(cb_url.c_str(), cb_path.c_str());
            if (ret != 0) {
                log_warn("dl_from_url for %s failed with %i", all_apps[idx].id.c_str(), ret);
                return ret;
            }
            else if (ret == 0)
                cf_tex[idx].exists = true;
        }
        else
            cf_tex[idx].exists = true;
    }
    if (locked_by_func)
        pthread_mutex_unlock(&disc_lock);

    return ITEMZCORE_SUCCESS;
}

#endif

static void check_n_draw_textures(int idx, int SH_type, vec4 col)
{

    bool locked_by_func = false;
    if (pthread_mutex_trylock(&disc_lock) == 0)
    {
        locked_by_func = true;
        log_warn("Mutex not locked!, Locked it");
    }

    if (cf_tex[idx].tex == cb_tex) // craft a coverbox stretching the icon0
    {

        render_tex(cb_tex, /* template */ SH_type, cover_t, col);

        if (all_apps[idx].texture == GL_NULL)
            all_apps[idx].texture = load_png_asset_into_texture(all_apps[idx].picpath.c_str());

        if (all_apps[idx].texture == GL_NULL)
            all_apps[idx].texture = fallback_t; // fallback icon0

        // overlayed stretched icon0
        render_tex(all_apps[idx].texture, SH_type, cover_i, col);
    }
    else // available cover box texture
        render_tex(cf_tex[idx].tex, SH_type, cover_t, col);

    if (locked_by_func)
        pthread_mutex_unlock(&disc_lock);
}

static void send_cf_action(int plus_or_minus_one, int type)
{

    if (ani[0].is_running)
        return; // don't race any running ani

    // we will operate on this
    ani[0].is_running = true;
    ani[0].handle = &g_idx;

    // set ani, off
    switch (type)
    {
    case 0:
        ani[0].life = .1976,                    // time_life in seconds
            ani[0].now = .0,                    // set actual time
            ani[0].v3_t = (vec3){1.05, 0., 0.}; // item position offset
        ani[0].v3_r = (vec3){0., 90., 0.};      // item position rotation
        break;
    case 1:
        ani[0].life = 3 * .1976,                // time_life in seconds
            ani[0].now = .0,                    // set actual time
            ani[0].v3_t = (vec3){1.05, 0., .2}; // item position offset
        ani[0].v3_r = (vec3){0., 60., .0};      // item position rotation
        break;
    }

    // we will move +/- 1 so: keep the sign!
    xoff = plus_or_minus_one * off.x; // set total x offset with sign!

    //log_info("%d, %s, %s, %d", g_idx,
    //         all_apps[g_idx].name.c_str(),
    //         all_apps[g_idx].id.c_str(),
    //         all_apps[g_idx].texture);
}

static vec3 set_view(enum view_t vt) // view type
{
    vec3 ret = (0);

    if (ani[0].is_running)
        return ret; // don't race any running ani
    // we will operate on this
    //  if(vt == ITEM_SETTINGS)
    up_next = vt;
    ani[0].is_running = true;
    ani[0].handle = &v_curr;

    ani[0].life = 2 * .1976,                // time_life in seconds
        ani[0].now = .0,                    // set actual time
        ani[0].v3_t = (vec3){1.05, 0., 0.}; // item position offset
    ani[0].v3_r = (vec3){0., 90., 0.};      // item position rotation

    switch (vt)
    {
    case ITEM_PAGE:
        ret = (vec3){.70, -.55, 1.85};
        break;

    case RANDOM:
    {
        //double r = rand() % 2 - 1;
        ret = (vec3){4, 4, 4};
    }
    break;

    case ITEM_SETTINGS:
    {
        ret = (vec3){0, 3, 0};
        // v_curr = ITEM_SETTINGS;
        break;
    }
    default:
        break;
    }

    GLES2_refresh_sysinfo();
    // log_info("%.3f %.3f %.3f", ret.x, ret.y, ret.z);

    return ret;
}

static void O_action_settings(layout_t *l)
{

    switch (l->curr_item)
    {
    case REBUILD_DB_OPTION:
    case OPEN_SHELLUI_MENU:
    case POWER_CONTROL_OPTION:
    case SORT_BY_OPTION:
    {
        if (l->item_d[l->curr_item].multi_sel.is_active &&
         l->item_d[l->curr_item].multi_sel.pos.x != RESET_MULTI_SEL)
        {
            log_info("Operation is Cancelled!");
            goto refresh;
        }
        break;
    }
    default:
        break;
    };


    //log_info("Settings: %d, %i", l->curr_item, numb_of_settings);
    numb_of_settings = NUMBER_OF_SETTINGS;
    ITEMZ_SETTING_TEXT[11] = fmt::format("{0:.30}", getLangSTR(SETTINGS_12));
    

    /* DELETE SETTINGS VIEW */
    if (setting_p.vbo)
        vertex_buffer_delete(setting_p.vbo), setting_p.vbo = NULL;

    if (setting_p.f_rect)
        free(setting_p.f_rect), setting_p.f_rect = NULL;

    setting_p.item_d.clear();
    v2 = set_view(ITEMzFLOW);
    return;
refresh:
    if (l->item_d[l->curr_item].multi_sel.is_active && l->item_d[l->curr_item].multi_sel.pos.x != RESET_MULTI_SEL)
    {
        l->item_d[l->curr_item].multi_sel.pos.x = RESET_MULTI_SEL;
        l->vbo_s = ASK_REFRESH;
        skipped_first_X = false;
    }
}

int SETTING_V_CURR = -1;
int GAME_PANEL_V_CURR = -1;
SAVE_Multi_Sel SAVE_CB = GM_INVAL;

int FS_GP_Callback(std::string filename, std::string fullpath)
{
    log_info("GAME_PANEL_V_CURR %i", GAME_PANEL_V_CURR);

    switch (GAME_PANEL_V_CURR)
    {
    case Change_icon_opt:
    {
        int w, h;
        if (is_png_vaild(fullpath.c_str(), &w, &h) && h == 512 && w == 512)
        {
#if defined(__ORBIS__)
            if (!copyFile(fullpath, icon_path, true))
                ani_notify(NOTIFI_WARNING, getLangSTR(ICON_CHANGE_FAILED), getLangSTR(FAILED_TO_COPY));
            else
            {
                ani_notify(NOTIFI_SUCCESS, getLangSTR(ICON_CHANGED_SUCCESS), getLangSTR(PS4_REBOOT_REQ));
                fmt::print("[ICON] copied {} >> {}", fullpath, icon_path);
                icon_path = fullpath;
            }
#else
            ani_notify(NOTIFI_SUCCESS, getLangSTR(ICON_CHANGED_SUCCESS), getLangSTR(PS4_REBOOT_REQ));
#endif
        }
        else
            ani_notify(NOTIFI_WARNING, getLangSTR(ICON_CHANGE_FAILED), getLangSTR(PNG_NOT_VAILD));
        // log_info("w: %i h: %i", h, w);
        break;
    }
    case Game_Save_opt:
    {
#if defined(__ORBIS__)
        SaveData_Operations(RESTORE_GAME_SAVE, title_id, fullpath);
#endif
        break;
    }
    default:
        break;
    }
    GAME_PANEL_V_CURR = -1;
    return 0;
}

int FS_Setting_Callback(std::string filename, std::string fullpath)
{
    std::string tmp;
    int bg_w, bg_h = 0;
    log_info("SETTING_V_CURR %i", SETTING_V_CURR);

    switch (SETTING_V_CURR)
    {
    case CHANGE_BACKGROUND_OPTION:
        //get->setting_bools[using_sb] = false;
        if(is_png_vaild(fullpath.c_str(), &bg_w, &bg_h)){
            ani_notify(NOTIFI_SUCCESS, getLangSTR(IMAGE_APPLIED), "");
            get->setting_bools[has_image] = true;
            get->setting_strings[IMAGE_PATH] = fullpath;
            bg_tex = load_png_asset_into_texture(get->setting_strings[IMAGE_PATH].c_str());
        }
        else
            ani_notify(NOTIFI_WARNING, getLangSTR(PNG_NOT_VAILD), "");

        log_info("IMAGE_PATH %s", get->setting_strings[IMAGE_PATH].c_str());
        break;
    case DUMPER_PATH_OPTION:
        get->setting_strings[DUMPER_PATH] = fullpath;
        ani_notify(NOTIFI_SUCCESS, getLangSTR(DUMPER_PATH_UPDATED), fmt::format("{0:.20}: {1:.20}",  getLangSTR(NEW_PATH_STRING), fullpath));
        break;
    case THEME_OPTION:{
        #if defined(__ORBIS__)
        if(filename == "Reset Theme to Default.zip"){
           log_info("Reset Theme selected... ");
           get->setting_bools[using_theme] = false;
           //RESET FONT TO DEFAULT
           get->setting_strings[FNT_PATH] = "/system_ex/app/NPXS20113/bdjstack/lib/fonts/SCE-PS3-RD-R-LATIN.TTF";
           rmdir(APP_PATH("theme"));
           if(!SaveOptions(get))
              log_error("[THEME] Failed to save Theme Options");
           else{
                 ani_notify(NOTIFI_SUCCESS, getLangSTR(THEME_RESET), getLangSTR(RELAUCHING_IN_5));
                 relaunch_timer_thread();
           }

           break;
        }
        if(install_IF_Theme(fullpath)){
            ani_notify(NOTIFI_SUCCESS, getLangSTR(THEME_INSTALL_SUCCESS), getLangSTR(RELAUCHING_IN_5));
            relaunch_timer_thread();
        }
        else
            ani_notify(NOTIFI_WARNING, getLangSTR(THEME_NOT_VAILD), "");
        #endif
        break;
    }
    case FONT_OPTION:
    {
        if (GLES2_fonts_from_ttf(fullpath.c_str())){
            get->setting_strings[FNT_PATH] =  fullpath;
            if (title_vbo)
                 vertex_buffer_delete(title_vbo), title_vbo = NULL;

            ani_notify(NOTIFI_SUCCESS, getLangSTR(NEW_FONT_STRING), fmt::format("{0:.20}: {1:.20}",  getLangSTR(NEW_FONT_STRING), filename));
        }
        break;
    }
    case BACKGROUND_MP3_OPTION:
    {
        mp3_status_t status =  MP3_STATUS_ERROR;
        get->setting_strings[MP3_PATH] = fullpath;
#if defined(__ORBIS__)
        retry_mp3_count = 0;
        mp3_playlist.clear();
        if ((status = MP3_Player(get->setting_strings[MP3_PATH])) == MP3_STATUS_STARTED)
            ani_notify(NOTIFI_SUCCESS, getLangSTR(MP3_PLAYING), getLangSTR(MP3_LOADED));
        else if (status == MP3_STATUS_PLAYING){
            ani_notify(NOTIFI_SUCCESS, getLangSTR(MUSIC_Q), getLangSTR(PLAYING_NEXT));
        }
#endif
        break;
    }
    case IF_PKG_INSTALLER_OPTION:{
#if defined(__ORBIS__)

if ((ret = pkginstall(fullpath.c_str(), filename.c_str(), true, false)) != ITEMZCORE_SUCCESS){
    if (ret == IS_DISC_GAME)
        ani_notify(NOTIFI_WARNING, getLangSTR(OPTION_DOESNT_SUPPORT_DISC), fmt::format("{0:#x}",  ret));
}

#endif
        break;
    }
    default:
        break;
    }
    SETTING_V_CURR = -1;
    return 0;
}

static void X_action_settings(int action, layout_t *l)
{
    // selected item from active panel
    log_info("execute setting %d -> '%s' for 'ItemzCore'", l->curr_item,
             l->item_d[l->curr_item].name.c_str());

    l->vbo_s = ASK_REFRESH;
    SETTING_V_CURR = l->curr_item;
    switch (l->curr_item)
    {
    case POWER_CONTROL_OPTION:
    case SORT_BY_OPTION:
    case REBUILD_DB_OPTION:
    case OPEN_SHELLUI_MENU:
        if (l->item_d[l->curr_item].multi_sel.is_active)
        {
            const char *uri = NULL;
            log_info("[OP]: opt: %i | Skip X?: %s", skipped_first_X, skipped_first_X ? "YES" : "NO");
            if (skipped_first_X)
            {
                if (l->curr_item == REBUILD_DB_OPTION)
                {
                    switch (l->item_d[l->curr_item].multi_sel.pos.y)
                    {
                        case REBUILD_ALL:
                            log_info("REBUILD ALL");
                            #ifdef __ORBIS__
                            rebuild_db();
                            #endif
                            break;
                        case REBUILD_DLC:
                            log_info("REBUILD DLC");
                            #ifdef __ORBIS__
                            rebuild_dlc_db();
                            #endif
                            break;
                        case REBUILD_REACT:
                            log_info("REBUILD REACTPSN");
                            #ifdef __ORBIS__
                            if(Reactivate_external_content(false))
                                ani_notify(NOTIFI_SUCCESS, getLangSTR(ADDCONT_REACTED), "");
                            else
                                ani_notify(NOTIFI_WARNING, getLangSTR(ADDCONT_REACT_FAILED), "");

                            #endif
                            break;
                    }
                }
                else if (l->curr_item == OPEN_SHELLUI_MENU)
                {
                    switch (l->item_d[l->curr_item].multi_sel.pos.y)
                    {
                    case SHELLUI_SETTINGS_MENU:
                        log_info("SETTINGS MENU");
                        uri = "pssettings:play?mode=settings";
                        break;
                    case SHELLUI_POWER_SAVE:
                        log_info("POWER SAVE MENU");
                        uri = "pssettings:play?mode=settings&function=power_save_standby";
                        break;
                    default:
                        break;
                    }
#if defined(__ORBIS__)
                    if (uri != NULL)
                    {
                        if ((ret = ItemzLaunchByUri(uri)) != ITEMZCORE_SUCCESS)
                            ani_notify(NOTIFI_WARNING, getLangSTR(FAILED_MENU_OPEN), fmt::format("{0:.20}: {1:#x}",  getLangSTR(ERROR_CODE), ret));
                    }
#endif
                }
                else if (l->curr_item == SORT_BY_OPTION)
                {
                    get->sort_by = (Sort_Multi_Sel)l->item_d[l->curr_item].multi_sel.pos.y;
                    refresh_apps_for_cf(get->sort_by, get->sort_cat);
                }
                else if (l->curr_item == POWER_CONTROL_OPTION)
                {
#if defined(__ORBIS__)
                    if(!power_settings((Power_control_sel)l->item_d[l->curr_item].multi_sel.pos.y))
                        ani_notify(NOTIFI_WARNING, getLangSTR(ERROR_POWER_CONTROL), fmt::format("{}", strerror(errno)));
#endif
                }
                skipped_first_X = false;
            }
            else
                skipped_first_X = true;

            if (l->item_d[l->curr_item].multi_sel.pos.x == 0)
            {
                l->item_d[l->curr_item].multi_sel.pos.x = 1; // EDITABLE
            }
            else
                l->item_d[l->curr_item].multi_sel.pos.x = 0;
        }

        break;
    case BACKGROUND_MP3_OPTION:
        StartFileBrowser((view_t)v_curr, active_p, FS_Setting_Callback, FS_MP3);
        break;
    case DUMPER_PATH_OPTION:
        StartFileBrowser((view_t)v_curr, active_p, FS_Setting_Callback, FS_FOLDER);
        break; // ps4 keyboard func
    case IF_PKG_INSTALLER_OPTION:
        StartFileBrowser((view_t)v_curr, active_p, FS_Setting_Callback, FS_PKG);
        break;
    case THEME_OPTION:
        StartFileBrowser((view_t)v_curr, active_p, FS_Setting_Callback, FS_ZIP);
        break;
    case CHECK_FOR_UPDATE_OPTION:
    {
#if defined(__ORBIS__)
        loadmsg(getLangSTR(CHECH_UPDATE_IN_PROGRESS));
#if UPDATE_TESTING==1 
        if (1)
#else  
        if((ret = check_update_from_url(ITEMZFLOW_TID)) == IF_UPDATE_FOUND)
#endif
        {
#if UPDATE_TESTING==1 
            if (1)
#else
            if(Confirmation_Msg(getLangSTR(OPT_UPDATE)) == YES)
#endif
            {

                loadmsg(getLangSTR(CHECH_UPDATE_IN_PROGRESS));
                mkdir("/user/update_tmp", 0777);
	            mkdir("/user/update_tmp/covers", 0777);
	            mkdir("/user/update_tmp/custom_app_names", 0777);
                copyFile("/user/app/ITEM00001/settings.ini", "/user/update_tmp/settings.ini", false);
                if(usbpath() != -1){
                    mkdir(fmt::format("/mnt/usb{0:d}/Itemzflow", usbpath()).c_str(), 0777);
                    if(copyFile("/user/app/ITEM00001/settings.ini", fmt::format("/mnt/usb{0:d}/Itemzflow/settings.ini", usbpath()), false))
                    log_debug("[UPDATE] Copied settings USB");
                }
                else
                  log_error("[UPDATE] Failed to copy settings USB");
    
	            copy_dir("/user/app/ITEM00001/covers", "/user/update_tmp/covers");
	            copy_dir("/user/app/ITEM00001/custom_app_names", "/user/update_tmp/custom_app_names");

                if(do_update(UPDATE_URL))
                {
                    ani_notify(NOTIFI_SUCCESS, getLangSTR(IF_UPDATE_SUCESS), getLangSTR(RELAUCHING_IN_5));
                }
                else
                  ani_notify(NOTIFI_WARNING, getLangSTR(IF_UPDATE_FAILD), getLangSTR(OP_CANNED_1));
            }
            else
                ani_notify(NOTIFI_WARNING, getLangSTR(OP_CANNED), getLangSTR(OP_CANNED_1));
        }
        else if (ret == IF_NO_UPDATE)
            ani_notify(NOTIFI_WARNING, getLangSTR(NO_UPDATE_FOUND), "");
        else if (ret == IF_UPDATE_ERROR)
            ani_notify(NOTIFI_WARNING, getLangSTR(NO_INTERNET), getLangSTR(BE_SURE_INTERNET));

        sceMsgDialogTerminate();
#endif
        break;
    }
    case HOME_MENU_OPTION:
    {
        get->setting_bools[HomeMenu_Redirection] = get->setting_bools[HomeMenu_Redirection] ? false : true;
#if defined(__ORBIS__)
        if (is_connected_app)
        {
            log_info("Turning Home menu %s", get->setting_bools[HomeMenu_Redirection] ? "ON (ItemzFlow)" : "OFF (Orbis)");

            if (get->setting_bools[HomeMenu_Redirection])
            {
                if (IPCSendCommand(ENABLE_HOME_REDIRECT, (uint8_t*)&ipc_msg[0]) == NO_ERROR)
                    ani_notify(NOTIFI_INFO, getLangSTR(REDIRECT_TURNED_ON), "");
            }
            else
            {
                if (IPCSendCommand(DISABLE_HOME_REDIRECT, (uint8_t*)&ipc_msg[0]) == NO_ERROR)
                    ani_notify(NOTIFI_INFO, getLangSTR(REDIRECT_TURNED_OFF), "");
            }
        }
        else
            ani_notify(NOTIFI_WARNING, getLangSTR(DAEMON_FAILED_NOTIFY), getLangSTR(DAEMON_FAILED_NOTIFY2));
#endif
        break;
    }
    case SHOW_BUTTON_OPTION:
    {

        get->setting_bools[Show_Buttons] = get->setting_bools[Show_Buttons] ? false : true;
        if (title_vbo)
            vertex_buffer_delete(title_vbo), title_vbo = NULL;
        break;
    }
    case COVER_MESSAGE_OPTION:
    {
        get->setting_bools[cover_message] = get->setting_bools[cover_message] ? false : true;
        break;
    }
    case REFLECTION_OPTION:
    {
        use_reflection = (use_reflection) ? false : true;
        break;
    }
    case CHANGE_BACKGROUND_OPTION:
        StartFileBrowser((view_t)v_curr, active_p, FS_Setting_Callback, FS_PNG);
     break;
    case RESET_THEME_OPTION:
    {
#ifdef __ORBIS__
        log_info("Reset Theme selected... ");
        get->setting_bools[using_theme] = false;
        get->setting_bools[has_image] = false;
        get->setting_strings[IMAGE_PATH].clear();
        //RESET FONT TO DEFAULT
        get->setting_strings[FNT_PATH] = "/system_ex/app/NPXS20113/bdjstack/lib/fonts/SCE-PS3-RD-R-LATIN.TTF";
        rmdir(APP_PATH("theme"));
        if(!SaveOptions(get))
            log_error("[THEME] Failed to save Theme Options");
        else{
                 ani_notify(NOTIFI_SUCCESS, getLangSTR(THEME_RESET), getLangSTR(RELAUCHING_IN_5));
                 relaunch_timer_thread();
        }
#endif
        break;
    }
    case DOWNLOAD_COVERS_OPTION:
    
#if defined(__ORBIS__)
            if (Confirmation_Msg(getLangSTR(DOWNLOAD_COVERS)) == YES)
            {
                
                loadmsg(getLangSTR(DOWNLOADING_COVERS));
                log_info("Downloading covers...");
                for (int i = 1; i < all_apps[0].token_c + 1; i++){
                     int ret = download_texture(i);
                     if(ret != 0 && ret != 404){
                        log_error("Failed to download cover for %s", all_apps[i].name.c_str());
                        sceMsgDialogTerminate();
                        ani_notify(NOTIFI_WARNING, getLangSTR(FAILED_TO_DOWNLOAD_COVER), fmt::format("{0:d}", ret));
                        goto tex_err;
                     }
                }

                sceMsgDialogTerminate();
                log_info("Downloaded covers");
                ani_notify(NOTIFI_GAME, getLangSTR(COVERS_UP_TO_DATE), "");
            }
            else
                log_info("Download covers canceled");
tex_err:

#endif
         break;
    case SAVE_ITEMZFLOW_SETTINGS:
    {
        if(numb_of_settings == NUMBER_OF_SETTINGS) // is advanced settings???
        {
save_setting:
            if (!SaveOptions(get))
                ani_notify(NOTIFI_WARNING, getLangSTR(SAVE_ERROR), "");
            else
            {
                ani_notify(NOTIFI_SUCCESS, getLangSTR(SAVE_SUCCESS), (get->sort_cat == NO_CATEGORY_FILTER) ?  "" : getLangSTR(INC_CAT));
                fmt::print("Settings saved to {}", get->setting_strings[INI_PATH]);
                // reload settings
                LoadOptions(get);
            }
        }
        else {
            log_info("nu %i", numb_of_settings);
            StartFileBrowser((view_t)v_curr, active_p, FS_Setting_Callback, FS_FONT);
        }

        break;
    }
    case NUMB_OF_EXTRAS:
        if (numb_of_settings > NUMBER_OF_SETTINGS) // is advanced settings???
            goto save_setting;
        else
            StartFileBrowser((view_t)v_curr, active_p, FS_Setting_Callback, FS_FONT);
        
        break;
    default:
        break;
    };
}
// else if (is_sub_enabled && l->curr_item == NA_SORT && )
void Start_Dump(std::string name, std::string path, Dump_Multi_Sels opt)
{
#if defined(__ORBIS__)

    if (path.find("/mnt/usb") != std::string::npos)
    {
        if (usbpath() == -1)
        {
            fmt::print("path is %s However, no USB is connected...", path);
            ani_notify(NOTIFI_WARNING, getLangSTR(NO_USB), "");
            return;
        }
    }

    if (if_exists(path.c_str()))
    {
        if (check_free_space(path.c_str()) >= app_inst_util_get_size(title_id.c_str()))
        {
           if (opt == SEL_FPKG) {
               tmp = fmt::format("{}/{}.pkg", path, title_id);

               if(copyFile(pkg_path, tmp, true))
                  ani_notify(NOTIFI_SUCCESS, getLangSTR(FPKG_COPY_SUCCESS), "");
                else
                  ani_notify(NOTIFI_WARNING, getLangSTR(FPKG_COPY_FAILED), "");

               return;
            }
            else if(opt == SEL_GAME_PATCH){
                tmp = fmt::format("/user/patch/{}/patch.pkg", title_id);
                if(!if_exists(tmp.c_str())){
                   ani_notify(NOTIFI_WARNING, getLangSTR(NO_PATCH_AVAIL), "");
                   return;
                }
            }
            log_info("Enough free space");
            log_info("Setting dump flag to: %i", opt);
            dump = opt;
            if(app_status != RESUMABLE){
              //auto close app before doing a crit suspend
              Kill_BigApp(app_status);
              if((ret = IPCSendCommand(CRITICAL_SUSPEND, (uint8_t*)&ipc_msg[0]) != NO_ERROR)){
                 log_error("Daemon command failed with: %i", ret);
                 msgok(NORMAL, getLangSTR(DUMP));
               }
               Launch_App(title_id, true);

            }else{
                log_info("App is already running, Sending Dump signal with opt: %i ...", dump);
               raise(SIGCONT);
            }
            return;
        }
        else
        {
            ani_notify(NOTIFI_WARNING, getLangSTR(NOT_ENOUGH_FREE_SPACE), fmt::format("{0:d} GBs | {1:.10}: {2:d} GBs", check_free_space(path.c_str()), title_id, app_inst_util_get_size(title_id.c_str())));

            return;
        }
    }
    else
    {
        ani_notify(NOTIFI_WARNING, getLangSTR(DUMPER_PATH_NO_AVAIL), "");
        return;
    }
#else
    //log_info("Dumping %s to %s, opt: %d", name, path, opt);
#endif
}

static void
X_action_dispatch(int action, layout_t *l)
{
    int ret; 
    int error = -1;
    // avoid not used warning
    ret = -1;
    fmt::print("execute {} -> '{}' for '{}'", l->curr_item, l->item_d[l->curr_item].name, all_apps[g_idx].name);

    std::string dst;
    std::string src;
    std::string tmp;
    std::string usb;

    fmt::print("title_id: {} title: {}", title_id, title);
    usb = fmt::format("/mnt/usb{}/", usbpath());

    l->vbo_s = ASK_REFRESH;
    GAME_PANEL_V_CURR = l->curr_item;
    switch (l->curr_item)
    {
    // ALL THE 3 MULTI SELS
    case Move_apps_opt:
    case Game_Save_opt:
    case Uninstall_opt:
    case Restore_apps_opt:
    {
        if (l->item_d[l->curr_item].multi_sel.is_active)
        {
            log_info("Multi sel is active");
            if (skipped_first_X)
            {
                switch (l->curr_item)
                {
                case Game_Save_opt:{
#if defined(__ORBIS__)
                    if (!gm_save.is_loaded){
                        StartFileBrowser((view_t)v_curr, active_p, FS_GP_Callback, FS_FOLDER);
                    }
                    else if (l->item_d[l->curr_item].multi_sel.pos.y == RESTORE_GAME_SAVE){
                        StartFileBrowser((view_t)v_curr, active_p, FS_GP_Callback, FS_FOLDER);
                    }
                    else
                        SaveData_Operations((SAVE_Multi_Sel)l->item_d[l->curr_item].multi_sel.pos.y, title_id, "");
#endif
                 break;
                }
                case Move_apps_opt:
                {
#if defined(__ORBIS__)
                    if (usbpath() == -1)
                    {
                        log_info("No USB is connected...");
                        ani_notify(NOTIFI_WARNING, getLangSTR(NO_USB), "");
                        return;
                    }
                    else if (l->item_d[l->curr_item].is_ext_hdd)
                    {
                        ani_notify(NOTIFI_WARNING, getLangSTR(EXT_MOVE_NOT_SUPPORTED), "");
                        return;
                    }
                    if (l->item_d[l->curr_item].multi_sel.pos.y == MOVE_APP && check_free_space(usb.c_str()) < app_inst_util_get_size(title_id.c_str()))
                    {
                        ani_notify(NOTIFI_WARNING, getLangSTR(NOT_ENOUGH_FREE_SPACE), fmt::format("USB: {0:d} GBs | {1:}: {2:d} GBs", check_free_space(usb.c_str()), title_id, app_inst_util_get_size(title_id.c_str())));
                        return;
                    }

                    dst = fmt::format("{}/itemzflow/apps/{}/", usb, title_id);
                    std::filesystem::create_directories(std::filesystem::path(dst));

                    switch (l->item_d[l->curr_item].multi_sel.pos.y)
                    {
                    case MOVE_APP:
                        src = fmt::format("/user/app/{}/app.pkg", title_id);
                        dst = fmt::format("{}/itemzflow/apps/{}/app.pkg", usb, title_id);
                        break;
                    case MOVE_DLC:
                        src  = fmt::format("/user/addcont/{}/ac.pkg", title_id);
                        dst = fmt::format("{}/itemzflow/apps/{}/app_dlc.pkg", usb, title_id);
                        break;
                    case MOVE_PATCH:
                        src  = fmt::format("/user/patch/{}/patch.pkg", title_id);
                        dst = fmt::format("{}/itemzflow/apps/{}/app_patch.pkg", usb, title_id);
                        break;
                    default:
                        break;
                    }
                    if(if_exists(dst.c_str()))
                    {   //ALREADY BACKED UP 
                        ani_notify(NOTIFI_WARNING, getLangSTR(THE_ITEM_IS_ALREADY_BP), getLangSTR(OP_CANNED_1));
                        break;
                    }
                    if (!copyFile(src, dst, true))
                        ani_notify(NOTIFI_WARNING, getLangSTR(MOVE_FAILED), getLangSTR(OP_CANNED_1));
                    else
                    {
                        loadmsg("Validating Move...");
                        if (MD5_file_compare(src.c_str(), dst.c_str()) && unlink(src.c_str()) == 0 && symlink(src.c_str(), dst.c_str()) == 0)
                        {
                            //Backup license files
                            dst = fmt::format("{}/itemzflow/licenses", usb);
                            copy_dir((char*)"/user/license", (char*)dst.c_str());

                            switch (l->item_d[l->curr_item].multi_sel.pos.y)
                            {
                            case MOVE_APP:
                                ani_notify(NOTIFI_GAME, getLangSTR(APP_MOVE_NOTIFY), title_id);
                                break;
                            case MOVE_DLC:
                                ani_notify(NOTIFI_GAME, getLangSTR(DLC_MOVE_NOTIFY), title_id);
                                break;
                            case MOVE_PATCH:
                                ani_notify(NOTIFI_GAME, getLangSTR(PATCH_MOVE_NOTIFY), title_id);
                                break;
                            }
                        }
                        else
                        {
                            if (symlink(dst.c_str(), src.c_str()) < 0)
                                log_debug("[MOVE] symlinked %s >> %s, Errno(%s)", src.c_str(), dst.c_str(), strerror(errno));

                            ani_notify(NOTIFI_WARNING, getLangSTR(MOVE_FAILED), getLangSTR(OP_CANNED_1));
                        }
                    }
                    sceMsgDialogTerminate();
#endif
                    break;
                }
                case Restore_apps_opt:
                {
#if defined(__ORBIS__)
                    if (usbpath() == -1)
                    {
                        log_info("No USB is connected...");
                        ani_notify(NOTIFI_WARNING, getLangSTR(NO_USB), "");
                        return;
                    }

                    src = fmt::format("{}/itemzflow/apps/{}/app.pkg", usb, title_id);
                    size_t len = CalcAppsize((char*)"/user/app");
                    bool app_moved = if_exists(src.c_str());
                    src = fmt::format("{}/itemzflow/apps/{}/app_dlc.pkg", usb, title_id);
                    bool dlc_moved = if_exists(src.c_str());
                    src = fmt::format("{}/itemzflow/apps/{}/app_patch.pkg", usb, title_id);
                    bool patch_moved = if_exists(src.c_str());
                    

                    
                    if (!app_moved && !dlc_moved && !patch_moved)
                    {
                        ani_notify(NOTIFI_WARNING, getLangSTR(APP_NOT_MOVED), "");
                        return;
                    }
                    if (l->item_d[l->curr_item].multi_sel.pos.y == RESTORE_APP && len > check_free_space(usb.c_str()))
                    {
                        ani_notify(NOTIFI_WARNING, getLangSTR(NOT_ENOUGH_FREE_SPACE), fmt::format("{0:d} GBs | /user/app: {1:d} GBs", check_free_space(usb.c_str()), len));
                        return;
                    }
                    switch (l->item_d[l->curr_item].multi_sel.pos.y)
                    {
                    case RESTORE_APP:{
                        src = fmt::format("{}/itemzflow/apps/{}/app.pkg", usb, title_id);
                        dst = fmt::format("/user/app/{}/app.pkg", title_id);
                        break;
                    }
                    case RESTORE_DLC:{
                        src = fmt::format("{}/itemzflow/apps/{}/app_dlc.pkg", usb, title_id);
                        dst = fmt::format("/user/addcont/{}/ac.pkg", title_id);
                        break;
                    }
                    case RESTORE_PATCH:{
                        src = fmt::format("{}/itemzflow/apps/{}/app_patch.pkg", usb, title_id);
                        dst = fmt::format("/user/patch/{}/patch.pkg", title_id);
                        break;
                    }
                    default:
                        break;
                    }
                    // Unlink the Symlink we had
                    unlink(dst.c_str());
                    // RESTORE the License files after reinstalling
                    dst = fmt::format("{}/itemzflow/licenses", usb);
                    // Reinstall PKG (Uninstalls app first) with progress bar
                    if ((ret = pkginstall(src.c_str(), title_id.c_str(), true, true)) != ITEMZCORE_SUCCESS && copy_dir((char*)dst.c_str(), (char*)"/user/license")){
                        if (ret == IS_DISC_GAME)
                            ani_notify(NOTIFI_WARNING, getLangSTR(OPTION_DOESNT_SUPPORT_DISC), fmt::format("{0:#x}", ret));
                        else
                            ani_notify(NOTIFI_WARNING, getLangSTR(RESTORE_FAILED), fmt::format("{0:#x}", ret));
                    }
                    else if (ret == 0)
                    {
                        fmt::print("[RESTORE] installed {} >> {}", src, dst);
                        ani_notify(NOTIFI_GAME, getLangSTR(RESTORE_SUCCESS), fmt::format("@ {0:}", dst));
                        sceMsgDialogTerminate();
                    }
#endif
                    break;
                }
                case Uninstall_opt:
                {
#if defined(__ORBIS__)
                    switch ((Uninstall_Multi_Sel)get->un_opt)
                    {
                    case UNINSTALL_PATCH:
                    {
                        if (Confirmation_Msg(uninstall_get_str((Uninstall_Multi_Sel)get->un_opt)) == NO)
                        {
                            break;
                        }
                        if (app_inst_util_uninstall_patch(title_id.c_str(), &error) && error == 0)
                            ani_notify(NOTIFI_GAME, getLangSTR(UNINSTALL_UPDATE), "");
                        else
                            ani_notify(NOTIFI_WARNING, getLangSTR(UNINSTAL_UPDATE_FAILED), fmt::format("{0:}: {1:#x} ({2:d})", getLangSTR(ERROR_CODE), error, error));

                        break;
                    }
                    case UNINSTALL_GAME:
                    {
                        if (Confirmation_Msg(uninstall_get_str((Uninstall_Multi_Sel)get->un_opt)) == NO)
                        {
                            break;
                        }
                        if (app_inst_util_uninstall_game(title_id.c_str(), &error) && error == 0)
                        {
                            ani_notify(NOTIFI_GAME, getLangSTR(UNINSTALL_GAME1), "");
                            fw_action_to_cf(0x1337);
                            ani_notify(NOTIFI_GAME, getLangSTR(APPS_RELOADED), getLangSTR(APPS_RELOADED_2));
                            refresh_apps_for_cf(get->sort_by, get->sort_cat);
                        }
                        else
                            ani_notify(NOTIFI_WARNING, getLangSTR(UNINSTALL_FAILED), fmt::format("{0:}: {1:#x} ({2:d})", getLangSTR(ERROR_CODE), error, error));

                        break;
                    }
                    case UNINSTALL_DLC:
                        if (Confirmation_Msg(uninstall_get_str((Uninstall_Multi_Sel)get->un_opt)) == NO)
                        {
                            //break;

                        }
                        break;
                    }
#endif
                    break;
                }
                }
                skipped_first_X = false;
            }
            else{
                log_info("[MOVE] Skipped first X");
                skipped_first_X = true;
            }

            if (l->item_d[l->curr_item].multi_sel.pos.x == 0)
                l->item_d[l->curr_item].multi_sel.pos.x = 1; // EDITABLE
            else
                l->item_d[l->curr_item].multi_sel.pos.x = 0;
        }
        break;
    } // end of huge switch
    case Dump_Game_opt:
    {
        if (l->item_d[l->curr_item].multi_sel.is_active)
        {
            int last_opt = SEL_DLC;
            fmt::print("$$$$ item {} is Multi {}", l->curr_item, skipped_first_X);

            if (l->item_d[l->curr_item].is_fpkg)
                last_opt = SEL_FPKG;

            if (l->item_d[l->curr_item].multi_sel.pos.x == last_opt)
                l->item_d[l->curr_item].multi_sel.pos.x = SEL_DUMP_ALL;
            else if (l->item_d[l->curr_item].multi_sel.pos.x == SEL_DUMP_ALL)
                l->item_d[l->curr_item].multi_sel.pos.x++;
            else
                l->item_d[l->curr_item].multi_sel.pos.x--;

            if (skipped_first_X)
            {
              log_info("dumping option: %i ...", get->Dump_opt);
#if defined(__ORBIS__)
              Start_Dump(title, get->setting_strings[DUMPER_PATH], get->Dump_opt);
#else
                Start_Dump(title, "NOT A REAL PATH", get->Dump_opt);
#endif
                skipped_first_X = false;
            }
            else
                skipped_first_X = true;
        }
        break;
    }
    case Launch_Game_opt:
    {
        if (app_status == RESUMABLE)
        {
            if (l->item_d[l->curr_item].multi_sel.is_active)
            {
                if (skipped_first_X)
                {
                    int multi_opt = l->item_d[l->curr_item].multi_sel.pos.y+1; // +1 skip first option
                    log_info("ll option: %i ...", multi_opt);
                    if (multi_opt == RESUMABLE)
                    {
#if defined(__ORBIS__)
                        if (get_patch_path(title_id)){
                            is_first_eboot = true;
                            trainer_launcher();
                        }

                        
                        Launch_App(title_id, (bool)(app_status == RESUMABLE), g_idx);
#else
                        log_info("[PC DEV] *********** RESUMED");
#endif
                    }
                    else if (multi_opt == OTHER_APP_OPEN)
                    {
#if defined(__ORBIS__)
                         Kill_BigApp(app_status);
#else
                         log_info("[PC DEV] *********** killed");
#endif
                    }

                    skipped_first_X = false;
                    (l->item_d[l->curr_item].multi_sel.pos.x == OTHER_APP_OPEN) ? l->item_d[l->curr_item].multi_sel.pos.x = RESUMABLE : l->item_d[l->curr_item].multi_sel.pos.x = OTHER_APP_OPEN;
                }
                else{
                    skipped_first_X = true;
                    l->item_d[l->curr_item].multi_sel.pos.x = RESUMABLE;
                    // refresh view
                    GLES2_render_list((view_t)v_curr);
                }
            }
        }
        else
        {
           // app_status = (app_status == NO_APP_OPENED) ? RESUMABLE : NO_APP_OPENED;
            if (app_status == NO_APP_OPENED){
#if defined(__ORBIS__)
                        
                        
                if(!is_dumpable && Confirmation_Msg(getLangSTR(CLOSE_GAME)) == NO) break;
                ret = Launch_App(title_id, (bool)(app_status == RESUMABLE), g_idx);
                if (!(ret & 0x80000000))
                    app_status = RESUMABLE;
                else
                    log_error("Launch failed with Error: 0x%X", ret);
#else
                log_info("[PC DEV] ************ LAUNCHED");
                app_status = RESUMABLE;
#endif
            }
            else if (app_status == OTHER_APP_OPEN)
            {
#if defined(__ORBIS__)
                Kill_BigApp(app_status);
#else
                log_info("[PC DEV] ************ KILLED");
                app_status = NO_APP_OPENED;
#endif
            }
        }
        break;
    }
    case Trainers_opt:
    {
#if defined(__ORBIS__)
        if (get_patch_path(title_id)){
        patch_current_index = l->item_d[l->curr_item].multi_sel.pos.y;
        if (skipped_first_X) {
            patch_file = fmt::format("/data/GoldHEN/patches/json/{}.json", title_id);
            std::string patch_title = trs.patcher_title.at(patch_current_index);
            std::string patch_name = trs.patcher_name.at(patch_current_index);
            std::string patch_app_ver = trs.patcher_app_ver.at(patch_current_index);
            std::string patch_app_elf = trs.patcher_app_elf.at(patch_current_index);
            u64 hash_ret = patch_hash_calc(patch_title, patch_name, patch_app_ver, patch_file, patch_app_elf);
            std::string hash_id = fmt::format("{:#016x}", hash_ret);
            std::string hash_file = fmt::format("/user/data/GoldHEN/patches/settings/{0}.txt",  hash_id);
            write_enable(hash_id, hash_file, patch_name);
            // clear items when user does an action
            trs.patcher_title.clear();
            trs.patcher_app_ver.clear();
            trs.patcher_app_elf.clear();
            trs.patcher_patch_ver.clear();
            trs.patcher_name.clear();
            trs.patcher_author.clear();
            trs.patcher_note.clear();
            trs.patcher_enablement.clear();
            trs.controls_text.clear();
            l->item_d[l->curr_item].multi_sel.pos.x = RESET_MULTI_SEL;
            l->vbo_s = ASK_REFRESH;
            skipped_first_X = false;
            break;
        }
        patch = 0;
        char* buffer;
        u64 size;
        int res = Read_File(patch_file.c_str(), &buffer, &size, 32);
        if (res) {
            notifywithicon(patcher_notify_icon, "file %s not found\n error: 0x%08x", patch_file.c_str(), res);
            break;
        }
        json_t mem[max_tokens];
        json_t const *json = json_create(buffer, mem, sizeof mem / sizeof *mem);
        if (!json) {
            notifywithicon(patcher_notify_icon, "Too many tokens or bad file\n");
            break;
        }

        json_t const *patchItems = json_getProperty(json, "patch");
        if (!patchItems || JSON_ARRAY != json_getType(patchItems)) {
            notifywithicon(patcher_notify_icon, "Patch not found\n");
            break;
        }
        json_t const *patches;
        for (patches = json_getChild(patchItems); patches != 0;
            patches = json_getSibling(patches)) {
            if (JSON_OBJ == json_getType(patches)) {
                const char *gameTitle = json_getPropertyValue(patches, key_title.c_str());
                if (!gameTitle)
                {
                    log_debug("%s: not found", key_title.c_str());
                    gameTitle = "";
                }
                const char *gameAppver = json_getPropertyValue(patches, key_app_ver.c_str());
                if (!gameAppver)
                {
                    log_debug("%s: not found", key_app_ver.c_str());
                    gameAppver = "";
                }
                const char *gameAppElf = json_getPropertyValue(patches, key_app_elf.c_str());
                if (!gameAppElf)
                {
                    log_debug("%s: not found", key_app_elf.c_str());
                    gameAppElf = "";
                }
                const char *gameName = json_getPropertyValue(patches, key_name.c_str());
                if (!gameName)
                {
                    log_debug("%s: not found", key_name.c_str());
                    gameName = "";
                }
                const char *gamePatchVer = json_getPropertyValue(patches, key_patch_ver.c_str());
                if (!gamePatchVer)
                {
                    log_debug("%s: not found", key_patch_ver.c_str());
                    gamePatchVer = "";
                }
                const char *gameAuthor = json_getPropertyValue(patches, key_author.c_str());
                if (!gameAuthor)
                {
                    log_debug("%s: not found", key_author.c_str());
                    gameAuthor = "";
                }
                const char *gameNote = json_getPropertyValue(patches, key_note.c_str());
                if (!gameNote)
                {
                    log_debug("%s: not found", key_note.c_str());
                    gameNote = "";
                }
                get_metadata1(&trs, gameAppver, gameAppElf, gameTitle, gamePatchVer, gameName, gameAuthor, gameNote);
                patch++;
                std::string patch_control_title = fmt::format("{1} {0}", patch, getLangSTR(PATCH_NUM));
                trs.controls_text.push_back(patch_control_title);
                skipped_first_X = true;
                }
            }
                if (l->item_d[l->curr_item].multi_sel.pos.x == trs.controls_text.size()) {
                    l->item_d[l->curr_item].multi_sel.pos.x = 0;
                }
                else if (l->item_d[l->curr_item].multi_sel.pos.x == 0) {
                    l->item_d[l->curr_item].multi_sel.pos.x++;
                }
                else {
                    l->item_d[l->curr_item].multi_sel.pos.x--;
                }
        break;
        } else {
            std::string patchq = fmt::format("{}\n{}", getLangSTR(NO_PATCHES), getLangSTR(PATCH_DL_QUESTION));
            int conf = Confirmation_Msg(patchq);
            if(conf == YES){
                dl_patches();
            } else if (conf == NO) {
                msgok(NORMAL, getLangSTR(PATCH_DL_QUESTION_NO));
            }
            sceMsgDialogTerminate();
            break;
        }
#endif
        break;
    }
#if defined(__ORBIS__)
    case Hide_App_opt:
    {

        log_info("App vis b4: %i", AppVis.load());
        msgok(WARNING, getLangSTR(DB_MOD_WARNING));

        if (!AppDBVisiable(title, VIS_WRTIE, AppVis ? 0 : 1))
            ani_notify(NOTIFI_WARNING, getLangSTR(UNHIDE_FAILED_NOTIFY), "");
        else
        {
            if (AppVis)
                ani_notify(NOTIFI_GAME, getLangSTR(UNHIDE_NOTIFY), getLangSTR(HIDE_SUCCESS_NOTIFY));
            else
                ani_notify(NOTIFI_GAME, getLangSTR(UNHIDE_NOTIFY), getLangSTR(UNHIDE_SUCCESS_NOTIFY));

            AppVis = !AppVis;
            all_apps[g_idx].app_vis = AppVis;
        }
        break;
    }
    case Change_name_opt:
    {
        // SCE Keyboard C Function needs a char array
        msgok(WARNING, getLangSTR(DB_MOD_WARNING));
        char buf[255];

        if (!Keyboard(getLangSTR(CHANGE_APP_NAME).c_str(), title.c_str(), &buf[0]))
            return;

        tmp = std::string(&buf[0]);

        if (change_game_name(tmp, title_id, all_apps[g_idx].sfopath))
        {
            pthread_mutex_lock(&disc_lock);

            all_apps[g_idx].name = tmp;

            pthread_mutex_unlock(&disc_lock);
            
            ani_notify(NOTIFI_GAME, getLangSTR(TITLE_CHANGED), getLangSTR(PS4_REBOOT_REQ));
        }
        else
            ani_notify(NOTIFI_WARNING, getLangSTR(FAILED_TITLE_CHANGE), "0xDEADBEEF");

        break;
    }
#endif
    case Change_icon_opt:
        StartFileBrowser((view_t)v_curr, active_p, FS_GP_Callback, FS_PNG);
        break;
    default:
        break;
    }
}

static void item_page_Square_action(layout_t *l)
{
#if defined(__ORBIS__)
    if (l->item_d[l->curr_item].multi_sel.is_active && l->item_d[l->curr_item].multi_sel.pos.x != RESET_MULTI_SEL)
    {
        switch (l->curr_item)
        {
        case Trainers_opt:
            if (get_patch_path(title_id)){
                patch_current_index = l->item_d[l->curr_item].multi_sel.pos.y;
                std::string patch_title = trs.patcher_title.at(patch_current_index);
                std::string patch_name = trs.patcher_name.at(patch_current_index);
                std::string patch_app_ver = trs.patcher_app_ver.at(patch_current_index);
                std::string patch_author = trs.patcher_author.at(patch_current_index);
                std::string patch_note = trs.patcher_note.at(patch_current_index);
                std::string patch_ver = trs.patcher_patch_ver.at(patch_current_index);
                std::string patch_app_elf = trs.patcher_app_elf.at(patch_current_index);
                std::string patch_msg = fmt::format("{} {}\n"
                                                    "{} {}\n"
                                                    "{} {}\n"
                                                    "{} {}\n"
                                                    "{} {}\n"
                                                    "{} {}\n"
                                                    "{} {}\n",
                                                    getLangSTR(PATCH_GAME_TITLE),
                                                    patch_title,
                                                    getLangSTR(PATCH_NAME),
                                                    patch_name,
                                                    getLangSTR(PATCH_APP_VER),
                                                    patch_app_ver,
                                                    getLangSTR(PATCH_AUTHOR),
                                                    patch_author,
                                                    getLangSTR(PATCH_NOTE),
                                                    patch_note,
                                                    getLangSTR(PATCH_VER),
                                                    patch_ver,
                                                    getLangSTR(PATCH_APP_ELF),
                                                    patch_app_elf);
                msgok(NORMAL, patch_msg);
            }
            break;
        default:
            break;
        }
    }
#endif
    return;
}
static void item_page_Triangle_action(layout_t *l)
{
#if defined(__ORBIS__)
    if (l->item_d[l->curr_item].multi_sel.is_active && l->item_d[l->curr_item].multi_sel.pos.x != RESET_MULTI_SEL)
    {
        switch (l->curr_item)
        {
        case Trainers_opt:
        {
            int conf = Confirmation_Msg(getLangSTR(PATCH_DL_QUESTION));
            if(conf == YES){
                dl_patches();
            } else if (conf == NO) {
                msgok(NORMAL, getLangSTR(PATCH_DL_QUESTION_NO));
            }
            sceMsgDialogTerminate();
            break;
        }
        default:
            break;
        }
    }
#endif
    return;
}

static void item_page_O_action(layout_t *l)
{
    if (l->item_d.size() >= l->curr_item && l->item_d[l->curr_item].multi_sel.is_active
        &&l->item_d[l->curr_item].multi_sel.pos.x != RESET_MULTI_SEL)
    {
        l->item_d[l->curr_item].multi_sel.pos.y = l->item_d[l->curr_item].multi_sel.pos.x = RESET_MULTI_SEL;
        l->item_d[l->curr_item].multi_sel.is_active = false;
        switch (l->curr_item)
        {
        case Uninstall_opt: get->un_opt = UNINSTALL_GAME;
        case Trainers_opt:
        case Dump_Game_opt: get->Dump_opt = SEL_DUMP_ALL;
        case Game_Save_opt:
        case Launch_Game_opt:
        case Restore_apps_opt:
        case Move_apps_opt:
            goto multi_back;
        default:
            break;
        }
    }
    
    l->is_active = l->is_shown = false;
    ls_p.curr_item = 0;
    setting_p.curr_item = 0;
    gm_p.vbo_s = ASK_REFRESH;
    v2 = set_view(ITEMzFLOW);

    return;

multi_back:
    log_info("Operation is Cancelled!");
    // game save clear
    gm_save.is_loaded = false;
#if defined(__ORBIS__)
    // trainer clear
    trs.patcher_title.clear();
    trs.patcher_app_ver.clear();
    trs.patcher_app_elf.clear();
    trs.patcher_patch_ver.clear();
    trs.patcher_name.clear();
    trs.patcher_author.clear();
    trs.patcher_note.clear();
    trs.patcher_enablement.clear();
    trs.controls_text.clear();
    patch_current_index = 0;
#endif
    l->vbo_s = ASK_REFRESH;
    skipped_first_X = false;
}
void fw_action_to_cf(int button)
{
    // ask to refresh vbo to update title
    // avoid not used warning
    ret = -1;

    if (v_curr == FILE_BROWSER)
    {
        fw_action_to_fm(button);
        return;
    }

    GLES2_UpdateVboForLayout(&gm_p);
    GLES2_UpdateVboForLayout(&ls_p);
    GLES2_UpdateVboForLayout(&setting_p);

    GLES2_refresh_sysinfo();

    if (ani[0].is_running)
        return; // don't race any running ani

    if (v_curr == ITEMzFLOW)
    {
        active_p = NULL; //
        switch (button)
        { // movement
        case L2:
        if (get->sort_cat != NO_CATEGORY_FILTER){
            get->sort_cat = NO_CATEGORY_FILTER;
            refresh_apps_for_cf(get->sort_by);
            v2 = set_view(ITEMzFLOW);
        }
        break;
        case LEF:
            send_cf_action(-1, 0);
            break; // l_or_r
        case RIG:
            send_cf_action(+1, 0);
            break;
        case OPT:
            confirm_close = false;
            active_p = &setting_p;
            //reset selection pos
            active_p->curr_item = 0;
            active_p->f_sele = 0;
            v2 = set_view(ITEM_SETTINGS);

            //StartFileBrowser((view_t)v_curr, active_p, NULL, FS_NONE);
            break;
        //case 
        case CRO:
        {
            confirm_close = false;
            // Placeholder games are not interactable
            if (all_apps[g_idx].is_ph){
                ani_notify(NOTIFI_WARNING, getLangSTR(PLACEHOLDER_NOT_ENTER), "");
                return;
            }
            active_p = &ls_p; // control cf_game_options
            
            //reset selection pos
            active_p->curr_item = 0;
            active_p->f_sele = 0;

            pthread_mutex_lock(&disc_lock);

            pkg_path = all_apps[g_idx].package;
            icon_path = all_apps[g_idx].picpath;
            title = all_apps[g_idx].name;
            title_id = all_apps[g_idx].id;
            app_version = all_apps[g_idx].version;
            is_dumpable = all_apps[g_idx].is_dumpable;

            started_epoch =  mins_played = MIN_STATUS_RESET;
            current_app_is_fpkg = all_apps[g_idx].is_fpkg;
            AppVis = all_apps[g_idx].app_vis;
            gm_save.is_loaded = false;
            std::string app_stat = fmt::format("|{}|: is_fpkg: {} AppVis: {}", title, current_app_is_fpkg, AppVis.load());
            log_debug("%s", app_stat.c_str());
            // Every X press on a Game case, we will check if the game selected is opened or not
            // If the App Status changed while in this view the Signal Handler will update the app status and view
            // then the render function will redraw text if the app status changed
            // First we check if the game is opened, if not sceLncUtilGetAppId will return -1
            // After we check if the game id is == to the currently opened bigapps id
#if defined(__ORBIS__)
            int id = sceLncUtilGetAppId(title_id.c_str());
            if ((id & ~0xFFFFFF) == 0x60000000 && id == sceSystemServiceGetAppIdOfBigApp())
            {
                fmt::print("found app to resume: {0:}, AppId: {1:#x}", title_id, id);
                app_status = RESUMABLE; // set the app status to resumable
            }
            else
            {
                // See if another app is running
                if ((sceSystemServiceGetAppIdOfBigApp() & ~0xFFFFFF) == 0x60000000)
                    app_status = OTHER_APP_OPEN; // set the app status to other app opened
                else
                    app_status = NO_APP_OPENED; // set the app status to no app opened
            }
#else
            app_status = NO_APP_OPENED; // set the app status to no app opened
#endif
            pthread_mutex_unlock(&disc_lock);
            // set the view to the select game
            v2 = set_view(ITEM_PAGE);
            break; // in_out
        }
        case TRI:
        { // REFRESH APP
            // Update View
            ani[0].is_running = true;
            confirm_close = false;
            refresh_apps_for_cf(get->sort_by, get->sort_cat);
            ani_notify(NOTIFI_SUCCESS, getLangSTR(APPS_RELOADED), getLangSTR(APPS_RELOADED_2)); 
            break;
        }
        case SQU:{
            ani[0].is_running = true;
            confirm_close = false;
            //SILENCE COMPILER
            int cat = get->sort_cat;
            if (cat == NUMB_OF_CATEGORIES)
                cat = NO_CATEGORY_FILTER;
            else
                cat++;
            // workaround for [cannot increment/decrement expression of enum type]
            get->sort_cat = (Sort_Category)cat;
            refresh_apps_for_cf(get->sort_by, get->sort_cat);
            ani_notify(NOTIFI_SUCCESS, getLangSTR(UPDATED_CATEGORY), category_get_str(get->sort_cat));
            break;
        }
            /*          case UP:   gx -= 5.; log_info("gx:%.3f", gx); break;
                        case DOW:  gx += 5.; log_info("gx:%.3f", gx); break; */
        case CIR:
        {

            if (confirm_close)
            {
                log_info("closing....");
#if defined(__ORBIS__)
                raise(SIGQUIT);
#else
                exit(-1);
#endif
            }
            else
            {
                ani_notify(NOTIFI_INFO, getLangSTR(BUTTON_CONFIRM_CLOSE), "");
                confirm_close = true;
            }

            pthread_mutex_lock(&disc_lock);
            drop_some_coverbox();
            pthread_mutex_unlock(&disc_lock);
            v2 = set_view(ITEMzFLOW);
        }
            return;
        }
    }
    else if (v_curr == ITEM_PAGE) // move into game_option_panel
    {
        switch (button)
        { // movement
        case LEF:
            layout_update_sele(active_p, -1);
            break; // l_or_r
        case RIG:
            layout_update_sele(active_p, +1);
            break;
        case UP:
            layout_update_sele(active_p, -active_p->fieldsize.x);
            break;
        case DOW:
            layout_update_sele(active_p, +active_p->fieldsize.x);
            break;

        case CRO:
            X_action_dispatch(0, active_p);
            break; // in_out

        case SQU:
            item_page_Square_action(active_p);
            break;

        case CIR:
            item_page_O_action(active_p);
            break;

        case OPT:
            //eject disc
            if(inserted_disc.load() > 0){
#if __ORBIS__
                if((ret = sceShellCoreUtilRequestEjectDevice("/dev/cd0")) < 0)
                  ani_notify(NOTIFI_ERROR, getLangSTR(EJECT_DISK_ERROR), fmt::format("{0:#x}", ret));
                else
                  ani_notify(NOTIFI_SUCCESS, getLangSTR(EJECT_DISK_SUCCESS), "");
#endif
            inserted_disc = -1;
            break;
        }
        case TRI:
        {
            item_page_Triangle_action(active_p);
            /*
            app_status = OTHER_APP_OPEN; // set the app status to no app opened
            ani_notify(NOTIFI_WARNING, getLangSTR(OPT_NOT_PERMITTED), "");
            */
            break; // goto back_to_Store;
        }
        
        case 0x1337:
        {
            log_info("called 0x1337");
            v2 = set_view(ITEMzFLOW);
            break;
        }
        }
    }
    else if (v_curr == ITEM_SETTINGS) // move into game_option_panel
    {
        switch (button)
        { // movement
        case LEF:
            layout_update_sele(active_p, -1);
            break; // l_or_r
        case RIG:
            layout_update_sele(active_p, +1);
            break;
        case UP:
            layout_update_sele(active_p, -active_p->fieldsize.x);
            break;
        case DOW:
            layout_update_sele(active_p, +active_p->fieldsize.x);
            break;
            // case OPT:
        case CRO:
            X_action_settings(0, active_p);
            break; // in_out
        case R1:
        {
           if (numb_of_settings == NUMBER_OF_SETTINGS)
           {
               numb_of_settings += 6; // add 6 to the number of settings to show the extra settings
               ITEMZ_SETTING_TEXT[11] = fmt::format("{0:.30}", getLangSTR(EXTRA_SETTING_6));
               ITEMZ_SETTING_TEXT[17] = fmt::format("{0:.30}", getLangSTR(SETTINGS_12));
           }
           else
           {
               numb_of_settings = NUMBER_OF_SETTINGS; // remove 6 to the number of settings to hide the extra settings
               ITEMZ_SETTING_TEXT[11] = fmt::format("{0:.30}", getLangSTR(SETTINGS_12));
           }

           /* DELETE SETTINGS VIEW */
           if(setting_p.vbo)
              vertex_buffer_delete(setting_p.vbo), setting_p.vbo = NULL;

           if (setting_p.f_rect)
               free(setting_p.f_rect), setting_p.f_rect = NULL;

           setting_p.is_active = false;
           setting_p.item_d.clear();
           break;
        }

        case OPT:
            //eject disc
            if(inserted_disc.load() > 0){
#if __ORBIS__
                if((ret = sceShellCoreUtilRequestEjectDevice("/dev/cd0")) < 0)
                  ani_notify(NOTIFI_ERROR, getLangSTR(EJECT_DISK_ERROR), fmt::format("{0:#x}", ret));
                else
                  ani_notify(NOTIFI_SUCCESS, getLangSTR(EJECT_DISK_SUCCESS), "");
#endif
                  inserted_disc = -1;
            }
            else
              goto back_05905;

            break;
        case CIR:
        {
back_05905:
            vertex_buffer_delete(setting_p.vbo), setting_p.vbo = NULL;
            setting_p.is_active = false;
            O_action_settings(active_p);
            break;
        }
        case SQU:
        {
            if(active_p->curr_item == BACKGROUND_MP3_OPTION){
                log_info("Disabling Background MP3 ...");
#ifdef __ORBIS__
                Stop_Music();
#endif
                if(setting_p.vbo) vertex_buffer_delete(setting_p.vbo), setting_p.vbo = NULL;
            }
            else {
                log_debug("Killing daemon ...");
                #ifdef __ORBIS__
                if(is_connected_app){
                   IPCSendCommand(CMD_SHUTDOWN_DAEMON, (uint8_t*)&ipc_msg[0]);
                   is_connected_app = false;
                }
                #endif
            }
            break;
        }

        case TRI:
        {
            ani_notify(NOTIFI_WARNING, getLangSTR(OPT_NOT_PERMITTED), "");
            break; // goto back_to_Store;
        }
        }
    }
}
void load_button(const char* file_name, GLuint *tex){

    std::string file_path;

    if (*tex > GL_NULL)
       return;

    if (get->setting_bools[using_theme])
        file_path = fmt::format("{0:}/theme/{1:}", APP_PATH(""), file_name);
     else
        file_path = fmt::format("{0:}{1:}", asset_path("buttons/"), file_name);

    if (*tex == GL_NULL){
        log_debug("tex: %i", *tex);
        *tex = load_png_asset_into_texture(file_path.c_str());
        log_debug("load_button: %s | tex: %i", file_path.c_str(), *tex);
    }

}
extern bool file_options;

void draw_additions(view_t vt)
{
    load_button("btn_x.png", &btn_X);
    load_button("btn_o.png", &btn_o);
    load_button("btn_tri.png", &btn_tri);
    load_button("btn_options.png", &btn_options);
    load_button("btn_down.png", &btn_down);
    load_button("btn_up.png", &btn_up);
    load_button("btn_left.png", &btn_left);
    load_button("btn_right.png", &btn_right);
    load_button("btn_sq.png", &btn_sq);
    load_button("btn_l1.png", &btn_l1);
    load_button("btn_r1.png", &btn_r1);
    load_button("btn_l2.png", &btn_l2);

    if (get->setting_bools[Show_Buttons])
    {
        if (vt == ITEMzFLOW)
        {
            if (btn_X && btn_options && btn_tri && btn_o)
            {
                vec4 r;
                vec2
                    s = (vec2)(68),
                    p = (resolution - s) / 2.;
                p.y -= 400;
                s += p;
                // convert to normalized coordinates
                r.xy = px_pos_to_normalized(&p);
                r.zw = px_pos_to_normalized(&s);
                // char *p = strstr( l->item_d[i].picpath.c_str(), "storedata" );
                //  draw texture, use passed frect
                if(all_apps[0].token_c > 0) // if no apps dont draw x
                   on_GLES2_Render_icon(USE_COLOR, btn_X, 2, &r, NULL);

                render_button(btn_options, 84., 145., 1380, 40., 1.5);
                render_button(btn_tri, 67., 68., 1380, 90., 1.1);
                render_button(btn_o, 67., 68., 1600., 40., 1.1);
                render_button(btn_sq, 67., 68., 1600., 90., 1.1);
                if (get->sort_cat != NO_CATEGORY_FILTER)
                    render_button(btn_l2, 45., 50., 1390., 160., 1.1);
            }
        }
        else if (vt == ITEM_PAGE || vt == ITEM_SETTINGS)
        {

            if (btn_X && btn_up && btn_down && btn_left
             && btn_right && btn_o && btn_tri)
            {

                render_button(btn_X, 67., 68., 25., 0., 1.1);
                render_button(btn_o, 67., 68., 25., 45., 1.1);
                render_button(btn_left, 32., 32., 300., 90., 1);
                render_button(btn_right, 32., 32., 350., 90., 1);
                render_button(btn_up, 32., 32., 325., 50., 1);
                render_button(btn_down, 32., 32., 325., 20., 1);

                if (vt == ITEM_SETTINGS){
                    render_button(btn_r1, 30., 68., 500., 20., 1);
                    
                    if(as_text)
                       vertex_buffer_delete(as_text), as_text = NULL;
                    if(remove_music)
                        vertex_buffer_delete(remove_music), remove_music = NULL;

                    
                    as_text = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f");

                    if (setting_p.curr_item == BACKGROUND_MP3_OPTION &&
                     !get->setting_strings[MP3_PATH].empty())
                    {
                        remove_music = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f");
                        render_button(btn_sq, 67., 68., 500., 55., 1.1);
                        itemzcore_add_text(remove_music, 560., 80., getLangSTR(DISABLE_MUSIC));

                    }
                    #ifdef __ORBIS__
                    else if (is_connected_app && numb_of_settings != NUMBER_OF_SETTINGS){
                    #else
                    else if (numb_of_settings != NUMBER_OF_SETTINGS){
                    #endif
                        remove_music = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f");
                        render_button(btn_sq, 67., 68., 500., 55., 1.1);
                        itemzcore_add_text(remove_music, 560., 80., getLangSTR(SHUTDOWN_DAEMON));
                    }
                    
                    itemzcore_add_text(as_text, 580., 25., (numb_of_settings == NUMBER_OF_SETTINGS) ? getLangSTR(ADVANCED_SETTINGS) : getLangSTR(HIDE_ADVANCED_SETTINGS));

                    if (as_text)
                        ftgl_render_vbo(as_text, NULL);
                    if (remove_music)
                        ftgl_render_vbo(remove_music, NULL);
                }

                if (vt == ITEM_PAGE)
                { // && is_trainer_page == true

                    if (ls_p.curr_item == Trainers_opt)
                    {

                        render_button(btn_sq, 67., 68., 525., 0., 1.1);
                        render_button(btn_tri, 67., 68., 525., 45., 1.1);
                        if(!trainer_btns){
                           trainer_btns = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f");      
                           itemzcore_add_text(trainer_btns,585., 65., getLangSTR(BUTTON_PATCH_TRI_INFO));
                           itemzcore_add_text(trainer_btns,585., 20., getLangSTR(BUTTON_PATCH_SQUARE_INFO));
                        }

                        if(trainer_btns) 
                           ftgl_render_vbo(trainer_btns, NULL);
                    }
                    else if (ls_p.curr_item == Launch_Game_opt)
                    {
                        //started_epoch
                        if(started_epoch.load() > 0){
                           if (launch_text)  vertex_buffer_delete(launch_text), launch_text = NULL;
                           launch_text = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f");
                           if(launch_text){
                               char buff[21];
                               const time_t t = started_epoch.load();
                               strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&t));
                               //log_info("started_epoch: %ld", (long)t);
                               itemzcore_add_text(launch_text, 660., 45., fmt::format("{} - {:.19}", getLangSTR(PLAYING_SINCE), buff));
                               ftgl_render_vbo(launch_text, NULL);
                           }
                       }
                    }
                }
                if (inserted_disc.load() > 0)
                    render_button(btn_options, 84., 145., 25., 90., 1.5);
            }
        }
        else if (vt == FILE_BROWSER)
        {
            if (title_vbo)
                vertex_buffer_delete(title_vbo), title_vbo = NULL;

            title_vbo = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f");

            if (btn_X && btn_up && btn_down && btn_left
             && btn_right && btn_o && btn_tri)
            {

                render_button(btn_X, 67., 68., 25., 90., 1.1);
                render_button(btn_o, 67., 68., 25., 45., 1.1);
                render_button(btn_sq, 67., 68., 305., 100., 1);
                render_button(btn_l1, 25.33, 51.5, 500., 60., 1);
                render_button(btn_r1, 25.33, 51.5, 560., 60., 1);
                render_button(btn_l2, 45., 50., 560., 100., 1);
                render_button(btn_tri, 67., 68., 25., 135., 1.1);

                render_button(btn_up, 32., 32., 325., 70., 1);
                render_button(btn_down, 32., 32., 325., 40., 1);
            }
            // 1=Enter Directory
            // 2=Back out Directory
            // 3=Cancel/Exit
            // 4=Select File or Folder
            // 5=Switch File panels
            // 6=New Directory
            // 7=Copy/Paste

            itemzcore_add_text(title_vbo,80., 110., getLangSTR(FS_CONTROL_BUTTON_1));
            itemzcore_add_text(title_vbo,80., 70., getLangSTR(FS_CONTROL_BUTTON_2));
            itemzcore_add_text(title_vbo,80., 155., getLangSTR(FS_CONTROL_BUTTON_3));

            if (fs_ret.filter != FS_NONE){
                itemzcore_add_text(title_vbo, 365., 125., (fs_ret.filter == FS_FOLDER) ? getLangSTR(FS_CONTROL_BUTTON_4_1) : getLangSTR(FS_CONTROL_BUTTON_4));
                itemzcore_add_text(title_vbo, 1550., 50., (fs_ret.filter == FS_FOLDER) ? getLangSTR(SELECT_A_FOLDER) : getLangSTR(SELECT_A_FILE));
                itemzcore_add_text(title_vbo, 620., 120., getLangSTR(FS_CONTROL_BUTTON_6));
            }
            else if (fs_ret.filter == FS_NONE && !file_options){
                itemzcore_add_text(title_vbo, 365., 125., "File Options");
                itemzcore_add_text(title_vbo, 620., 120., getLangSTR(FS_CONTROL_BUTTON_6));
            }
            else if (fs_ret.filter == FS_NONE && file_options){
                itemzcore_add_text(title_vbo, 365., 125., "Select cunt");
                //itemzcore_add_text(title_vbo, 1550., 50., (fs_ret.filter == FS_FOLDER) ? getLangSTR(SELECT_A_FOLDER) : getLangSTR(SELECT_A_FILE));
                itemzcore_add_text(title_vbo, 620., 120., "Select cunt2");
            }

            itemzcore_add_text(title_vbo,370., 65., getLangSTR(BUTTON_UP_DOWN_INFO));
            itemzcore_add_text(title_vbo,620., 65., getLangSTR(FS_CONTROL_BUTTON_5));
            //itemzcore_add_text(title_vbo,620., 120., getLangSTR(FS_CONTROL_BUTTON_6));

            ftgl_render_vbo(title_vbo, NULL);
        }
    }
}

void DrawScene_4(void)
{

    layout_t *l;
    /* animation alpha fading */
    vec4 colo = (1.), ani_c;
    /* animation vectors */
    vec3 ani_p,
        ani_r;
    /* animation timing */
    double ani_ratio = 0.;

    if(mins_played > 0 || mins_played == MIN_STATUS_NA){
        if(title_vbo)
           vertex_buffer_delete(title_vbo), title_vbo = NULL;
    }
    /* animation l/r */
    const vec3 offset = (vec3){1.05, 0., 0.};

    /* delta view movement vector */
    vec3 view_v = v2 - v1;

    if (ani[0].is_running)
    { // update time
        ani[0].now += u_t - g_Time;
        // check on delta ani time_ratio
        ani_ratio = ani[0].now / ani[0].life;
        // ani ended
        if (ani_ratio > 1.)
        {
            ani_ratio = 0;
            /*  switch on (ani type) but with pointers:
                l/R op_on g_idx
                CRO op_on view_v */
            if (ani[0].handle == &g_idx)
            {                                 // ani ended, move global index by sign!
                g_idx += (xoff > 0) ? 1 : -1; // L:-1, R:+1
                // bounds checking on item count
                pthread_mutex_lock(&disc_lock);

                if (g_idx < 1)
                    g_idx = all_apps[0].token_c;
                if (g_idx > all_apps[0].token_c)
                    g_idx = 1;

                pthread_mutex_unlock(&disc_lock);
            }
            if (ani[0].handle == &v_curr)
            { // ani ended: now finally switch the view
                v1 = v2;

                v_curr = up_next;
            }
            // detach
            ani.clear();
            ani[0].is_running = false;

            xoff = 0.; // ani_flag, pm_flag
            // refresh title
            if (title_vbo)
                vertex_buffer_delete(title_vbo), title_vbo = NULL;
        }
        // the (delta!) fraction
        view_v *= ani_ratio;

        /* consider ani vector, get a fraction from offset
           and get a fraction of degree, for a single step */
        ani_p = ani_ratio * offset;
        ani_r = (vec3){0., 90. / pm_r, 0.};
        ani_r *= ani_ratio;
        // ani color fraction
        ani_c = ani_ratio * colo;
#if 0
        if (ani) log_info("ani_now:%f (%3.1f%%), %.3f (%.3f, %.3f, %.3f)",
            ani[0].now, ani_ratio * 100., ani_ratio * xoff,
            view_v.x, view_v.y, view_v.z);
#endif
    }
    else
        xoff = 0.; // no ani, no offset

    // update time
    g_Time = u_t;
    // add start point to view vector
    view_v += v1;

    switch (v_curr) //
    {
    case ITEMzFLOW:
        ani_c *= -1.;
        break; // decrease 1-0
    case ITEM_PAGE:
        colo = (0.);
        break; // increase 0-1
    case ITEM_SETTINGS:
        colo = (0.);
        break; // increase 0-1
    }

    // we fade color
    if (!xoff)
        colo += ani_c;

    /* background image, or pixelshader */
    if (bg_tex == GL_NULL && !get->setting_bools[has_image])
        pixelshader_render(2, NULL, NULL); // use PS_symbols shader
    else
    { /* bg image: fullscreen frect (normalized coordinates) */
        vec4 r = (vec4){-1., -1., 1., 1.};
        // see if its even loaded
        if (bg_tex > GL_NULL)
            on_GLES2_Render_icon(USE_COLOR, bg_tex, 2, &r, NULL);
    }

    int i, j, c = 0;
    /* loop outer to inner, care overlap */
    for (i = (all_apps[0].token_c > 1) ? pm_r : 0; i > 0; i--) // ...3, 2, 1
    {       
        j = i;

        vec3 pos, rot;

        draw_item:

        // next item position
        pos = j * offset;

        // set -DEFAULT_Z_CAMERA
        pos.z -= testoff; // adjust testoff

        rot = (vec3){gx, 90. / (float)pm_r, 0.};
        rot *= -j;

        //
        if (pr_dbg)
        {
            log_info("%2d: %3d %3d (%3.2f, %3.2f, %3.2f)\t",
                     c, i, j,
                     pos.x, pos.y, pos.z);
            log_info("(%3.2f, %3.2f, %3.2f)", rot.x, rot.y, rot.z);
        }

        if (!j) // means the selected, last one
        {

            colo = (1.);
            if (!xoff)
                rot.y = ani_ratio * 360; // coverbox backflip effect
        }
        //
        // adjust on l/r action
        if (xoff > 0)
        {
            pos -= ani_p;
            rot += ani_r; // right
        }
        else if (xoff < 0)
        {
            pos += ani_p;
            rot -= ani_r; // left
        }

        // ...
        //
        glUseProgram(sh_prog1);
        glEnable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        {
            // order matters!
            mat4_set_identity(&model);
            mat4_translate(&model, 0, 0, 0);
            // move to "floor" plane
            mat4_translate(&model, 0, 1.25 / 2., 0);
            // rotations
            mat4_rotate(&model, rot.x, 1, 0, 0);
            mat4_rotate(&model, rot.y, 0, 1, 0);
            mat4_rotate(&model, rot.z, 0, 0, 1);
            // set -DEFAULT_Z_CAMERA
            mat4_translate(&model, pos.x, pos.y, pos.z);
            if (1) // ani_view
            {
                mat4_set_identity(&view);
                mat4_translate(&view, view_v.x, view_v.y, view_v.z);
            }

            /* don't USE */ // glEnable( GL_DEPTH_TEST );
            {
                glUniformMatrix4fv(glGetUniformLocation(sh_prog1, "model"), 1, 0, model.data);
                glUniformMatrix4fv(glGetUniformLocation(sh_prog1, "view"), 1, 0, view.data);
                glUniformMatrix4fv(glGetUniformLocation(sh_prog1, "projection"), 1, 0, projection.data);
                glUniform4f(glGetUniformLocation(sh_prog1, "Color"), colo.r, colo.g, colo.b, colo.a);
            }
            vertex_buffer_render(cover_o, GL_TRIANGLES);
        }
        glUseProgram(0);
        //
        /* index texture from all_apps array */
        
        int tex_idx = g_idx + j;
        //log_info("tex_idx: %d j: %d", tex_idx, j);
        pthread_mutex_lock(&disc_lock);
        // check bounds: skip first
        if (tex_idx < 1)
            tex_idx += all_apps[0].token_c;
        else if (tex_idx > all_apps[0].token_c)
            tex_idx -= all_apps[0].token_c;

        if (Download_icons)
        {

#if defined(__ORBIS__)
            if (get->setting_bools[cover_message])
            {
                if (Confirmation_Msg(getLangSTR(DOWNLOAD_COVERS)) == YES)
                {
                
                    loadmsg(getLangSTR(DOWNLOADING_COVERS));
                    log_info("Downloading covers...");
                    for (int i = 1; i < all_apps[0].token_c + 1; i++)
                        download_texture(i);

                    sceMsgDialogTerminate();
                    log_info("Downloaded covers");
                }
                else
                  log_info("Download covers canceled");
            }
#else
            if (0)
            {
            }
#endif
            else
            {

                for (int i = 1; i < all_apps[0].token_c + 1; i++)
                     check_tex_for_reload(i);
            }

            log_info("all_apps[0].token_c: %i", all_apps[0].token_c);
            Download_icons = false;
        }

        // rdisc_i
        mat4 rdisc_model;
        // load textures (once)
        check_n_load_textures(tex_idx);
        // draw coverbox, if not avail craft coverbox from icon
        check_n_draw_textures(tex_idx, +1, colo);
        pthread_mutex_unlock(&disc_lock);
        //log_info("tex_idx: %d, inserted_disc.load() %i", tex_idx, inserted_disc.load());
        if (tex_idx == inserted_disc.load()) // single diff item
        {
            rdisc_rz += 1.; // rotation on Z
            // save for restore later
            mat4 model_b = model;
            mat4_set_identity(&model);
            mat4_translate(&model, 0, 0, 0);
    //      circle shadow does not rotates(?)
            mat4_rotate(&model, rdisc_rz, 0, 0, 1);
            // move to "floor" plane
            mat4_translate(&model, 0, 1. /2., 0);
            // move to final pos
            mat4_translate(&model, pos.x, pos.y, pos.z /* a little out from cover */ + .025f);

            // draw a fake shadow
            vec4 sh_col = vec4 {0., 0., 0., .75};
            // we need a new fragment shader / review the fragment shaders sources to clean the mess
            // actually texture is drawn plain, color does not change nothing
            render_tex(rdisc_tex, /* rdisc_texture */ -2, rdisc_i, sh_col);

            // re-position
            mat4_set_identity(&model);
            mat4_translate(&model, 0, 0, 0);
            mat4_rotate(&model, rdisc_rz, 0, 0, 1);
            // move to "floor" plane
            mat4_translate(&model, 0, 1. /2., 0);
            // move to final pos
            mat4_translate(&model, pos.x, pos.y, pos.z /* a little out from cover */ + .050f);

            // draw
            render_tex(rdisc_tex, /* rdisc_texture */ +1, rdisc_i, colo);

            if (1)
            { /* reflect on the Y, actual position matters! */
                mat4_scale(&model, 1, -1, 1);
                // fill blue
                sh_col = vec4 {0., 0., 1., .9 * colo.a};
                // rdisc_i
                rdisc_model = model;
               // render_tex(rdisc_tex, /* rdisc_texture */ -2, rdisc_i, sh_col);
            }
            // restore from saved
            model = model_b;
           // glUniformMatrix4fv(glGetUniformLocation(sh_prog1, "model"), 1, 0, model.data);
        }
        // reflection option
        if (use_reflection)
        { /* reflect on the Y, actual position matters! */
            mat4_scale(&model, 1, -1, 1);

            pthread_mutex_lock(&disc_lock);

            // draw objects
            glUseProgram(sh_prog1);
            glUniformMatrix4fv(glGetUniformLocation(sh_prog1, "model"), 1, 0, model.data);
            glUniform4f(glGetUniformLocation(sh_prog1, "Color"), .3, .3, .3, .1 * colo.a);
            vertex_buffer_render(cover_o, GL_TRIANGLES);
            glUseProgram(0);

            // draw coverbox reflection, if not avail craft coverbox from icon
            check_n_draw_textures(tex_idx, -1, colo);

            pthread_mutex_unlock(&disc_lock);

            if (tex_idx == inserted_disc.load())
            {
                // fill blue, (shader 1 don't use)
                vec4 sh_col = vec4 {1., 1., .8, .7 * colo.a};
                // rdisc_i
                model = rdisc_model;
                render_tex(rdisc_tex, /* rdisc_texture */ -3, rdisc_i, sh_col);
            }
        }
        /* set title */
        if (!j             // last drawn one, the selected in the center
            && !title_vbo) // we cleaned VBO
        {
            std::string tmpstr;

            //       vec2 pen = (vec2){ (float)l->bound_box.x - 20., resolution.y - 140. };
            //
            item_t *li = &all_apps[tex_idx];
           // log_info("tex_idx: %i", tex_idx);
                //
                title_vbo = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f");
            // NAME, is size checked
            tmpstr = fmt::format("{0:.20}{1:} {2:}", li->name, (li->name.length() > 20) ? "..." : "", li->is_ext_hdd ? "(Ext. HDD)" : "");
            if (v_curr == ITEM_SETTINGS)
                tmpstr = getLangSTR(ITEMZ_SETTINGS);

            // we need to know Text_Length_in_px in advance, so we call this:
            texture_font_load_glyphs(titl_font, tmpstr.c_str());
            //
            // Default for ITEMzFlow enum
            vec2 pen = (vec2){(float)((resolution.x - tl) / 2.), (float)((resolution.y / 10.) * 2.)};
            // Game Options Pen
            //  game name and tid
            vec2 pen_game_options = (vec2){100., (float)(resolution.y - 150.)};
#if BETA == 1
            vec2 beta_built = (vec2){(resolution.x - tl) / 2., (resolution.y / 5.) * 2.};
            add_text(title_vbo, titl_font, "BETA Build Watermark", &col, &beta_built);
#endif

            //
            // box is /2 so  test has to be 2.1
            vec2 setting_pen = (vec2){(float)(resolution.y / 2.1), (float)(resolution.y - 190.)};

            if (v_curr == ITEM_PAGE){ 
                pen_game_options.y += 25.;
                add_text(title_vbo, titl_font, tmpstr, &col, &pen_game_options);
            }
            else if (v_curr == ITEMzFLOW) // fill the vbo
                add_text(title_vbo, titl_font, tmpstr, &col, &pen);
            else if (v_curr == ITEM_SETTINGS)
                add_text(title_vbo, titl_font, tmpstr, &col, &setting_pen);

            // log_info("%i",__LINE__);

            //
            // ID
            // we need to know Text_Length_in_px in advance, so we call this:
            texture_font_load_glyphs(sub_font, li->id.c_str());
            // align text centered
            // log_info("%i",__LINE__);

            vec4 c = col * .75f; // 75% of default fg color

            // Default for ITEMzFlow enum
            pen.x = (resolution.x - tl) / 2.,
            pen.y -= 32;

            //
            if (v_curr == ITEM_PAGE)
            {
                if (get->setting_bools[Show_Buttons])
                {
                    //itemzcore_add_text(title_vbo,80., 115., getLangSTR(BUTTON_X_INFO));
                    itemzcore_add_text(title_vbo,80., 20., getLangSTR(BUTTON_X_INFO));
                    itemzcore_add_text(title_vbo,80., 65., getLangSTR(BUTTON_O_INFO));
                    // if (is_trainer_page)
                    itemzcore_add_text(title_vbo,390., 100., getLangSTR(BUTTON_LEFT_RIGHT_INFO));
                    itemzcore_add_text(title_vbo,370., 42., getLangSTR(BUTTON_UP_DOWN_INFO));

                    itemzcore_add_text(title_vbo,inserted_disc.load() > 0 ? 115 : 40., 110., inserted_disc.load() > 0 ? getLangSTR(EJECT_DISK) : getLangSTR(NO_DISC_INSERTED) );
                }

                pen_game_options.x += 10.;
                tmpstr = fmt::format("{0:} {2:} ({1:})", li->id, li->is_fpkg ? "FPKG" : getLangSTR(RETAIL), li->version);
                // fill the vbo
                add_text(title_vbo, sub_font, tmpstr, &c, &pen_game_options);

                fmt::print("mins_played: {}", mins_played.load());

                tmpstr = getLangSTR(FETCH_STATS);

                if(mins_played.load() > 0){
                    if(mins_played.load() < 59){
                        tmpstr = fmt::format("{0:d} {1:}", mins_played.load(), getLangSTR(STATS_MINS));
                    }
                    else if(mins_played < 1440){
                        tmpstr = fmt::format("{0:d} {1:} {2:d} {3:}", mins_played.load()/60, getLangSTR(STATS_HOURS), mins_played.load()%60, getLangSTR(STATS_MINS));
                    }
                    else if(mins_played >= 1440){
                        tmpstr = fmt::format("{0:d} {1:} {2:d} {3:} {4:d} {5:}", mins_played.load()/1440, getLangSTR(STATS_DAYS), mins_played.load()%1440/60, getLangSTR(STATS_HOURS), mins_played.load()%1440%60, getLangSTR(STATS_MINS));
                    }
                    //fmt::print("cpp msg: {}", tmpstr);
                    mins_played = MIN_STATUS_SEEN;
                       
                }//
                else if(mins_played == MIN_STATUS_NA){
                    tmpstr = getLangSTR(NO_STATS_AVAIL);
                    mins_played = MIN_STATUS_SEEN;
                }
                itemzcore_add_text(title_vbo,100., 920., tmpstr);
            }
            else if (v_curr == ITEMzFLOW)
            { // fill the vbo{
                // 1692 y 61
                add_text(title_vbo, sub_font, li->id, & c, &pen);
                if (get->setting_bools[Show_Buttons])
                {
                    
                    itemzcore_add_text(title_vbo,1475., 61., getLangSTR(SETTINGS));
                    itemzcore_add_text(title_vbo,1435., 110., getLangSTR(BUTTON_TRIANGLE));
                    if (!confirm_close)
                        itemzcore_add_text(title_vbo,1660., 61., getLangSTR(BUTTON_CLOSE));

                    itemzcore_add_text(title_vbo, 1660., 110., getLangSTR(CHANGE_CATEGORY));
                }

                if (confirm_close)
                    itemzcore_add_text(title_vbo,1660., 61., getLangSTR(BUTTON_CONFIRM_CLOSE));

                if(get->sort_cat != NO_CATEGORY_FILTER){
                    //pen.y -= 32;
                    itemzcore_add_text(title_vbo, 1440., 175., getLangSTR(RESET_CAT));
                    itemzcore_add_text(title_vbo, 780, 50, fmt::format("{0:} - {1:}", getLangSTR(SHOWING_CATEGORY), category_get_str(get->sort_cat)));
                }
            }
            else if (v_curr == ITEM_SETTINGS)
            { // fill the vbo{

                setting_pen.x += 10.;
                texture_font_load_glyphs(sub_font, (numb_of_settings == NUMBER_OF_SETTINGS) ? getLangSTR(SETTINGS).c_str() : getLangSTR(ADVANCED_SETTINGS).c_str());
                add_text(title_vbo, sub_font, (numb_of_settings == NUMBER_OF_SETTINGS) ? getLangSTR(SETTINGS) : getLangSTR(ADVANCED_SETTINGS), &c, &setting_pen);
                if (get->setting_bools[Show_Buttons])
                {
                    // itemzcore_add_text(title_vbo,80., 115., getLangSTR(BUTTON_X_INFO));
                    itemzcore_add_text(title_vbo, 80., 20., getLangSTR(BUTTON_X_INFO));
                    itemzcore_add_text(title_vbo,80., 65., getLangSTR(BUTTON_O_INFO));
                    itemzcore_add_text(title_vbo,390., 100., getLangSTR(BUTTON_LEFT_RIGHT_INFO));
                    itemzcore_add_text(title_vbo,370., 42., getLangSTR(BUTTON_UP_DOWN_INFO));
                    itemzcore_add_text(title_vbo,inserted_disc.load() > 0 ? 115 : 40., 110., inserted_disc.load() > 0 ? getLangSTR(EJECT_DISK) : getLangSTR(NO_DISC_INSERTED));
                }
            }
            //
            // we eventually added glyphs... (todo: glyph cache)
            refresh_atlas();
        }

        // ...

        // count: index is outer (+R, -L) to inner!
        c += 1;

        // flip sign, get mirrored
        if (j > 0)
        {
            j *= -1;
            goto draw_item;
        }

        // last one drawn: break loop!
        if (!j)
            goto all_drawn;
    }
    // last one is 0: the selected item
    //log_info("********** end");
    if (!i)
    {
        j = 0;
        if (all_apps[0].token_c >= 1)
        {
            // log_info("********** numb %i", all_apps[0].token_c);
            goto draw_item;
        }
       // else
       //    log_info("no apps found");
    }

    if (all_apps[0].token_c < 1 && !title_vbo)
    {
        title_vbo = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f");
        if(v_curr == ITEMzFLOW){
        itemzcore_add_text(title_vbo, 830, 150, getLangSTR(NO_APPS_FOUND));
        if (get->setting_bools[Show_Buttons])
        {
            itemzcore_add_text(title_vbo, 1475., 61., getLangSTR(SETTINGS));
            itemzcore_add_text(title_vbo, 1435., 110., getLangSTR(BUTTON_TRIANGLE));
            if (!confirm_close)
                itemzcore_add_text(title_vbo, 1660., 61., getLangSTR(BUTTON_CLOSE));

            itemzcore_add_text(title_vbo, 1660., 110., getLangSTR(CHANGE_CATEGORY));
        }

        if (confirm_close)
            itemzcore_add_text(title_vbo, 1660., 61., getLangSTR(BUTTON_CONFIRM_CLOSE));

        itemzcore_add_text(title_vbo, 1440., 175., getLangSTR(RESET_CAT));
        itemzcore_add_text(title_vbo, 780, 50, fmt::format("{0:} - {1:}", getLangSTR(SHOWING_CATEGORY), category_get_str(get->sort_cat)));
        }
        else if (v_curr == ITEM_SETTINGS)
        {

            itemzcore_add_text(title_vbo, (float)(resolution.y / 2.1), (float)(resolution.y - 190.), getLangSTR(SETTINGS));
            if (get->setting_bools[Show_Buttons])
            {
                itemzcore_add_text(title_vbo, 80., 20., getLangSTR(BUTTON_X_INFO));
                itemzcore_add_text(title_vbo, 80., 65., getLangSTR(BUTTON_O_INFO));
                itemzcore_add_text(title_vbo, 390., 100., getLangSTR(BUTTON_LEFT_RIGHT_INFO));
                itemzcore_add_text(title_vbo, 370., 42., getLangSTR(BUTTON_UP_DOWN_INFO));
                itemzcore_add_text(title_vbo, 580., 25., getLangSTR(ADVANCED_SETTINGS));
            }
        }
    }

all_drawn:
    glDisable(GL_BLEND);
    glUseProgram(0);
    draw_additions((view_t)v_curr);

    if (old_curr != v_curr)
        GLES2_refresh_sysinfo();

    if (v_curr != ITEMzFLOW)
        GLES2_render_list((view_t)v_curr); // v2

    switch (v_curr) // draw more things, per view
    {
    case ITEMzFLOW:
        if (pr_dbg)
        {
            log_info("%d,\t%s\t%s", g_idx,
                     all_apps[g_idx].name.c_str(),
                     all_apps[g_idx].id.c_str());
        }
        break;
        // add menu
    case ITEM_PAGE:
    {
        if (old_curr != v_curr)
        {
            l = &ls_p;
            l->vbo_s = ASK_REFRESH;
        }
        break;
    }
    case ITEM_SETTINGS:
    {
        // log_info("%i", __LINE__);
        break;
    }
    case FILE_BROWSER:
    {
        break;
    }
    }
    // texts out of VBO
    ftgl_render_vbo(title_vbo, NULL);

    old_curr = v_curr;
    pr_dbg = 0;
}

/*
    this is the main renderloop
    note: don't need to be cleared or swapped any frames !
    this one is drawing each rendered frame!
*/
void Render_ItemzCore(void)
{
    // update the
    // v_curr = FILE_BROWSER;
    on_GLES2_Update(u_t);
    GLES2_ani_update(u_t);

    if ((view_t)v_curr != FILE_BROWSER)
        DrawScene_4();
    else
    {
        draw_additions((view_t)v_curr);
        GLES2_render_list(FILE_BROWSER);
    }

    // show_dumped_frame();
    GLES2_Draw_sysinfo();
    GLES2_ani_draw();
}

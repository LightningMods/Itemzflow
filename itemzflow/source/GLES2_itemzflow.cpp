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
#include <array>
#include "feature_classes.hpp"
#if defined(__ORBIS__)
#include "shaders.h"
#include "net.h"
#include <signal.h>
#include "installpkg.h"
#else
#include "pc_shaders.h"
#endif
#include <errno.h>

#include "patcher.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#if defined(__ORBIS__)
#include "../external/pugixml/pugixml.hpp"
extern int retry_mp3_count;
#endif
#include "utils.h"
#include <future>

extern vec2 resolution;

std::mutex disc_lock;
extern std::atomic_int AppVis;

std::atomic<GameStatus> app_status(APP_NA_STATUS);
extern int numb_of_settings;
extern std::atomic_bool is_disc_inserted;
// redef for yaml-cpp
int isascii(int c)
{
    return !(c&~0x7f);
}

struct _update_struct update_info;
int update_current_index = 0;

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
u32 num_title_id = 0;
u32 patch_current_index = 0;
std::string key_title = "name";
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
#endif
// ------------------------------------------------------- global variables ---
// the Settings

std::string ipc_msg;  // Assuming buffer size of 100
extern ItemzSettings set,
    *get;
extern double u_t;
extern std::vector<std::string> gm_p_text;
using namespace multi_select_options;

static GLuint rdisc_tex;

extern ThreadSafeVector<item_t> all_apps;

VertexBuffer cover_o, cover_t, cover_i, title_vbo, rdisc_i, trainer_btns, launch_text, as_text, remove_music, app_home_refresh, favs, upload_logs_text, stop_daemon;
double rdisc_rz = 0.;
std::atomic_int inserted_disc(0);

int v_curr = ITEMzFLOW;
static mat4 model_main,
    view_main,
    projection_main;

GLuint sh_prog1,
    sh_prog2,
    sh_prog3,
    sh_prog4,
    sh_prog5;

bool skipped_first_X = false;
// ------------------------------------------------------ freetype-gl shaders ---
std::array<IconPosition, 7> heart_icon_positions{
    IconPosition{-3, 10, 850},
    IconPosition{-2, 270, 845},
    IconPosition{-1, 550, 830},
    IconPosition{0, 830, 810},
    IconPosition{1, 1300, 830},
    IconPosition{2, 1590, 845},
    IconPosition{3, 1850, 850}
};

vec4 c[8];
int up_next = ITEMzFLOW;

GLuint btn_X = GL_NULL, btn_options = GL_NULL, btn_tri = GL_NULL, btn_down = GL_NULL,
       btn_up = GL_NULL, btn_right = GL_NULL, btn_left, btn_o = GL_NULL, 
       btn_sq = GL_NULL, btn_l1 = GL_NULL, btn_r1 = GL_NULL, btn_l2 = GL_NULL, debug_icon = GL_NULL, heart_icon = GL_NULL;

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


layout_t& getLayoutbyView(view_t view)
{
    switch (view)
    {
    case ITEMzFLOW:
        return ls_p;
    case ITEM_PAGE:
        return ls_p;
    case ITEM_SETTINGS:
        return setting_p;
    case RANDOM:
         return ls_p;
    case FILE_BROWSER_RIGHT:
        return fm_rp;
    case FILE_BROWSER_LEFT:
        return fm_lp;
    }

    return ls_p;
}

void text_gl_extra( vertex_buffer_t* buffer,int num, GLuint texture, int idx, vec4 col, GLuint sh_prog){
   // int num = 2;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0 + num);
    glBindTexture(GL_TEXTURE_2D, texture);
    {
        glUniform1i(glGetUniformLocation(sh_prog, "texture"), num);
        glUniformMatrix4fv(glGetUniformLocation(sh_prog, "model"), 1, 0, model_main.data);
        glUniformMatrix4fv(glGetUniformLocation(sh_prog, "view"), 1, 0, view_main.data);
        glUniformMatrix4fv(glGetUniformLocation(sh_prog, "projection"), 1, 0, projection_main.data);

        glUniform4f(glGetUniformLocation(sh_prog, "Color"), col.r, col.g, col.b, col.a);

        /* draw whole VBO (storing all added) */
        vertex_buffer_render(buffer, GL_TRIANGLES);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}


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

/* view_main (matrix) vectors */
vec3 v0 = (vec3)(0),
     v1,
     v2;

// ---------------------------------------------------------------- display ---

/* model_main rotation angles and camera position */
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

layout_t ls_p, setting_p; // settings

#if defined(__ORBIS__)
extern std::atomic_int mins_played;
#else
std::atomic_int mins_played(2);
#endif
std::string tmp;
extern int setting_on;
SafeString title_id;
int wait_time = 0;
bool confirm_close = false;

/************************ END OF GLOBALS ******************/
void rescan_for_apps(multi_select_options::Sort_Multi_Sel op, Sort_Category cat );

void refresh_apps_for_cf(multi_select_options::Sort_Multi_Sel op, Sort_Category cat ){
    rescan_for_apps(op, cat);
    // load textures of new apps
    for (auto item : all_apps)
    {
        check_tex_for_reload(item);
        check_n_load_textures(item);
    }
    log_info("Done reloading # of App: %i", all_apps[0].count.token_c);
}

void rescan_for_apps(multi_select_options::Sort_Multi_Sel op, Sort_Category cat )
{
    std::lock_guard<std::mutex> lock(disc_lock);
    
    int before = all_apps[0].count.token_c;
    log_debug("Reloading Installed Apps before: %i", before);
#ifdef __ORBIS__
    loadmsg(getLangSTR(LANG_STR::RELOAD_LIST));
#endif
    // leak not so much
    // print_Apps_Array(all_apps, before);
    delete_apps_array(all_apps);
#ifdef __ORBIS__
    index_items_from_dir(all_apps, APP_PATH("../"), "/mnt/ext0/user/app", cat);
#else
    index_items_from_dir(all_apps, APP_PATH("apps"), "/user/app", cat);
#endif
    log_info("=== %i", all_apps[0].count.token_c);
    //std::vector<item_idx_t*> sort;
    //print_Apps_Array(all_apps);

    switch (op)
    {
    case multi_select_options::TID_ALPHA_SORT:
    {
        std::sort(all_apps.begin() + 1, all_apps.end(), [](item_t a, item_t b)
        { 
            return a.info.id.size() < b.info.id.size(); 
        });
        break;
    }
    case multi_select_options::TITLE_ALPHA_SORT:
    {
        std::sort(all_apps.begin() + 1, all_apps.end(), [](item_t a, item_t b)
        { 
            std::string tmp1 = a.info.name;
            std::string tmp2 = b.info.name;

            std::transform(tmp1.begin(), tmp1.end(), tmp1.begin(), ::tolower);
            std::transform(tmp2.begin(), tmp2.end(), tmp2.begin(), ::tolower);

            return tmp1.compare(tmp2) < 0;
       });
        break;
    }
    case multi_select_options::NA_SORT:
    default:
        break;
    }

    InitScene_4(1920, 1080);
    sceMsgDialogTerminate();
    log_debug("Done reloading # of App: %i, # of Apps added/removed: %i", all_apps[0].count.token_c, all_apps[0].count.token_c - before);
    is_disc_inserted = false;
    inserted_disc = -999;
    g_idx = 1;
}

void itemzcore_add_text(VertexBuffer &vert_buffer, float x, float y, std::string text)
{
    if(vert_buffer.empty()){
        log_debug("VBO is empty");
        return;
    }
    vec2 pen_s = (vec2){0., 0.};
    pen_s.x = x;
    pen_s.y = y;
    texture_font_load_glyphs(sub_font, text.c_str());
    vert_buffer.add_text(sub_font, text, col, pen_s);
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
void InitScene_5(int width, int height)
{
    glFrontFace(GL_CCW);

    fs_ret.status = FS_NOT_STARTED;
    fs_ret.last_v = RANDOM;
    fs_ret.fs_func = NULL;
    fs_ret.last_layout = NULL;
    fs_ret.filter = FS_NONE;

    // reset view_main
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
    cover_o = VertexBuffer("vertex:3f,color:4f");
    // the textuted objects
    cover_t = VertexBuffer("vertex:3f,tex_coord:2f,color:4f"); // owo template
    cover_i = VertexBuffer("vertex:3f,tex_coord:2f,color:4f"); // single diff item

    if (1) /* the single rotating disc icon texture (overlayed) */
    {
        rdisc_i = VertexBuffer("vertex:3f,tex_coord:2f,color:4f"); // rotating disc icon

        vec3 rdisc_vtx[4];
        // keep center dividing by 2.f, CW, squared texture
        rdisc_vtx[0] = (vec3){ .7 /2.,  .7 /2., .0 /2.};  // up R
        rdisc_vtx[1] = (vec3){-.7 /2.,  .7 /2., .0 /2.};  // up L
        rdisc_vtx[2] = (vec3){-.7 /2., -.7 /2., .0 /2.};  // bm L
        rdisc_vtx[3] = (vec3){ .7 /2., -.7 /2., .0 /2.};  // bm R
        rdisc_i.append_to_vbo(&rdisc_vtx[0]);
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

        cover_i.append_to_vbo(&cover_item_v[0]);
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
        cover_t.append_to_vbo(&cover_v[0]);

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

        cover_o.push_back(cover_vtx, 24, cover_idx, 36);

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

    mat4_set_identity(&projection_main);
    mat4_set_identity(&model_main);
    mat4_set_identity(&view_main);

    // reshape
    glViewport(0, 0, width, height);
    mat4_set_perspective(&projection_main, 60.0f, width / (float)height, 1.0, 10.0);
}

extern GLuint shader;

static bool reset_tex_slot(int idx)
{
    bool res = false;
    GLuint tex = all_apps[idx].icon.texture.load();

    if (tex > GL_NULL && tex != fallback_t) // keep icon0 template
    {  // discard icon0.png  
        glDeleteTextures(1, &tex), all_apps[idx].icon.texture = GL_NULL;
        res = true;
    }
        

    return res;
}

void drop_some_coverbox(void)
{
    int count = 0;
    std::lock_guard<std::mutex> lock(disc_lock);

    if (all_apps[0].count.token_c < 100)
    {
        return;
    }

    for (int i = 1; i < all_apps[0].count.token_c + 1; i++)
    {
        if (i > g_idx - pm_r - 1 && i < g_idx + pm_r + 1)
            continue;

        if (reset_tex_slot(i))
            count++;
    }

    if (count > 0)
        log_info("%s discarded %d textures", __FUNCTION__, count);
}

void check_tex_for_reload(item_t & item)
{
    std::string cb_path;
    //std::lock_guard<std::mutex> lock(disc_lock);
    int w, h;
    cb_path = fmt::format("{}/{}.png", APP_PATH("covers"), item.info.id);

    if (!if_exists(cb_path.c_str()) || !is_png_vaild(cb_path.c_str(), &w, &h))
    {
       // log_info("%s doesnt exist", cb_path.c_str());
        item.icon.cover_exists = false;
    }
    else
        item.icon.cover_exists = true;
}

void check_n_load_textures(item_t & item)
{
  //  std::lock_guard<std::mutex> lock(disc_lock);
    if (item.icon.texture.load() == GL_NULL)
    {
        //log_info("%s: %s", __FUNCTION__, all_apps[idx].info.id.c_str());
        std::string cb_path = fmt::format("{}/{}.png", APP_PATH("covers"), item.info.id);
        if (item.icon.cover_exists.load()){
           // log_info("loading cover");
            item.icon.image_data.data = load_png_asset(cb_path.c_str(), item.icon.turn_into_texture, item.icon.image_data);
        }
        else // load icon0 if cover doesnt exist
        {
           // log_info("Loading data for %s", all_apps[idx].info.picpath.c_str());
            item.icon.image_data.data = load_png_asset(item.info.picpath.c_str(), item.icon.turn_into_texture, item.icon.image_data);
           // log_info("Done loading data for %s", all_apps[idx].info.picpath.c_str());
        }
    }
}

bool is_remote_va_launched = false;
std::string vapp_title_launched;

void vapp_launch_event(bool is_launch = true){
    if(is_vapp(all_apps[g_idx].info.id)){
       if(is_launch){
         is_remote_va_launched = true;
         vapp_title_launched = all_apps[g_idx].info.id;
       }
       else{
         is_remote_va_launched = false;
         vapp_title_launched.clear();
       }
    }
}
void itemzCore_Launch_util(layout_t &  l) {
  if (app_status == RESUMABLE) {
    if (l. item_d[l. curr_item].multi_sel.is_active) {
      if (skipped_first_X) {
        int multi_opt = l. item_d[l. curr_item].multi_sel.pos.y + 1; // +1 skip first option
        log_info("ll option: %i ...", multi_opt);
        if (multi_opt == RESUMABLE) {
          #if defined(__ORBIS__)
          if (get_patch_path(all_apps[g_idx].info.id)) {
            is_first_eboot = true;
            trainer_launcher();
          }

          if(Launch_App(all_apps[g_idx].info.id, (bool)(app_status == RESUMABLE), g_idx) == ITEMZCORE_SUCCESS)
          {
            vapp_launch_event();
          }
          else
            log_info("Failed to launch %s", all_apps[g_idx].info.id.c_str());
          #else
          log_info("[PC DEV] *********** RESUMED");
          ani_notify(NOTIFI::NOTI_LEVELS::GAME, "RESUMED", "");
          #endif
        } else if (multi_opt == OTHER_APP_OPEN) {
          #if defined(__ORBIS__)
          Kill_BigApp(app_status);
          vapp_launch_event(false);
          #else
         ani_notify(NOTIFI::NOTI_LEVELS::GAME, "KILLED", "");
          #endif
        }

        skipped_first_X = false;
        (l. item_d[l. curr_item].multi_sel.pos.x == OTHER_APP_OPEN) ? l. item_d[l. curr_item].multi_sel.pos.x = RESUMABLE: l. item_d[l. curr_item].multi_sel.pos.x = OTHER_APP_OPEN;
      } else {
        skipped_first_X = true;
        l. item_d[l. curr_item].multi_sel.pos.x = RESUMABLE;
        // refresh view_main
        GLES2_render_list((view_t) v_curr);
      }
    }
  } else {
    // app_status = (app_status == NO_APP_OPENED) ? RESUMABLE : NO_APP_OPENED;
    if (app_status == NO_APP_OPENED) {
      #if defined(__ORBIS__)

      // vapp func
      if (!is_vapp(all_apps[g_idx].info.id) &&
        !all_apps[g_idx].flags.is_dumpable && Confirmation_Msg(getLangSTR(LANG_STR::CLOSE_GAME)) == NO) return;

      // ItemCore virtual apps arnt actual ps4 apps 
      if (is_if_vapp(all_apps[g_idx].info.id)) {
        log_debug("ItemzCore Virtual App tried to  be launched");
        // smth in the future
        return;
      }

      ret = Launch_App(all_apps[g_idx].info.id, (bool)(app_status == RESUMABLE), g_idx);
      if (!(ret & 0x80000000)){
        vapp_launch_event();
        app_status = RESUMABLE;
      }
      else
        log_error("Launch failed with Error: 0x%X", ret);
      #else
      log_info("[PC DEV] ************ LAUNCHED");
      app_status = RESUMABLE;
      #endif
    } else if (app_status == OTHER_APP_OPEN) {
      #if defined(__ORBIS__)
      Kill_BigApp(app_status);
      vapp_launch_event(false);
      #else
      log_info("[PC DEV] ************ KILLED");
      app_status = NO_APP_OPENED;
      #endif
    }
  }

}

#if defined(__ORBIS__)

static int download_texture(item_t & it) {

  std::string cb_path = fmt::format("{}/{}.png", APP_PATH("covers"), it.info.id);

  log_info("Trying to Download cover for %s", it.info.id.c_str());

  //loadmsg(getLangSTR(LANG_STR::DL_COVERS));
  int w, h = 0;
  if (!if_exists(cb_path.c_str()) || !is_png_vaild(cb_path.c_str(), & w, & h)) // download
  {
    it.icon.cover_exists = false;
    std::string cb_url = fmt::format("https://api.pkg-zone.com/download?cover_tid={}", it.info.id);

    int ret = dl_from_url(cb_url.c_str(), cb_path.c_str());
    if (ret != 0) {
      log_warn("dl_from_url for %s failed with %i", it.info.id.c_str(), ret);
      return ret;
    } else if (ret == 0)
      it.icon.cover_exists = true;
  } else
    it.icon.cover_exists = true;

  return ITEMZCORE_SUCCESS;
}

#endif

static void check_n_draw_textures(item_t & item, int SH_type, vec4 col)
{

//    std::lock_guard<std::mutex> lock(disc_lock);
    if(item.icon.turn_into_texture.load()){
        //log_info("Loadding ....");
       load_png_cover_data_into_texture(item.icon.image_data, item.icon.turn_into_texture, item.icon.texture);
    }  

    if (!item.icon.cover_exists) // craft a coverbox stretching the icon0
    {
        cover_t.render_tex(cb_tex, SH_type, col);

        if (item.icon.texture.load() == GL_NULL)
            cover_i.render_tex(fallback_t, SH_type, col);

        // overlayed stretched icon0
        cover_i.render_tex(item.icon.texture.load(), SH_type, col);
    }
    else // available cover box texture
        cover_t.render_tex(item.icon.texture.load(), SH_type, col);
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
    //         all_apps[g_idx].info.id.c_str(),
    //         all_apps[g_idx].icon.texture);
}

static vec3 set_view_main(view_t vt) // view_main type
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
        //v_curr = ITEM_SETTINGS;
        break;
    }
    default:
        break;
    }

    GLES2_refresh_sysinfo();
    // log_info("%.3f %.3f %.3f", ret.x, ret.y, ret.z);

    return ret;
}

static void O_action_settings(layout_t & l)
{

    switch (l.curr_item)
    {
    case REBUILD_DB_OPTION:
    case OPEN_SHELLUI_MENU:
    case POWER_CONTROL_OPTION:
    case SORT_BY_OPTION:
    {
        if (l.item_d[l.curr_item].multi_sel.is_active &&
         l.item_d[l.curr_item].multi_sel.pos.x != RESET_MULTI_SEL)
        {
            log_info("Operation is Cancelled!");
            goto refresh;
        }
        break;
    }
    default:
        break;
    };


    //log_info("Settings: %d, %i", l.curr_item, numb_of_settings);
    numb_of_settings = NUMBER_OF_SETTINGS;
    ITEMZ_SETTING_TEXT[11] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_12));
    

    /* DELETE SETTINGS view_main */
    setting_p.vbo.clear();
    setting_p.f_rect.clear();
    setting_p.item_d.clear();

    v2 = set_view_main(ITEMzFLOW);
    return;
refresh:
    if (l.item_d[l.curr_item].multi_sel.is_active && l.item_d[l.curr_item].multi_sel.pos.x != RESET_MULTI_SEL)
    {
        l.item_d[l.curr_item].multi_sel.pos.x = RESET_MULTI_SEL;
        l.vbo_s = ASK_REFRESH;
        skipped_first_X = false;
    }
}

int SETTING_V_CURR = -1;
int GAME_PANEL_V_CURR = -1;
multi_select_options::SAVE_Multi_Sel SAVE_CB = multi_select_options::GM_INVAL;

int FS_GP_Callback(std::string filename, std::string fullpath)
{
    log_info("GAME_PANEL_V_CURR %i", GAME_PANEL_V_CURR);

    switch (GAME_PANEL_V_CURR)
    {
    case cf_ops::REG_OPTS::CHANGE_ICON:
    {
        int w, h;
        if (is_png_vaild(fullpath.c_str(), &w, &h) && h == 512 && w == 512)
        {
#if defined(__ORBIS__)
            if (!copyFile(fullpath, all_apps[g_idx].info.picpath, true))
                ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::ICON_CHANGE_FAILED), getLangSTR(LANG_STR::FAILED_TO_COPY));
            else
            {
                ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::ICON_CHANGED_SUCCESS), getLangSTR(LANG_STR::PS4_REBOOT_REQ));
                fmt::print("[ICON] copied {} >> {}", fullpath, all_apps[g_idx].info.picpath);
                all_apps[g_idx].info.picpath = fullpath;
            }
#else
            ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::ICON_CHANGED_SUCCESS), getLangSTR(LANG_STR::PS4_REBOOT_REQ));
#endif
        }
        else
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::ICON_CHANGE_FAILED), getLangSTR(LANG_STR::PNG_NOT_VAILD));
        // log_info("w: %i h: %i", h, w);
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
            ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::IMAGE_APPLIED), "");
            get->setting_bools[has_image] = true;
            get->setting_strings[IMAGE_PATH] = fullpath;
            bg_tex = load_png_asset_into_texture(get->setting_strings[IMAGE_PATH].c_str());
        }
        else
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::PNG_NOT_VAILD), "");

        log_info("IMAGE_PATH %s", get->setting_strings[IMAGE_PATH].c_str());
        break;
    case DUMPER_PATH_OPTION:
        get->setting_strings[DUMPER_PATH] = fullpath;
        ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::DUMPER_PATH_UPDATED), fmt::format("{0:.20}: {1:.20}",  getLangSTR(LANG_STR::NEW_PATH_STRING), fullpath));
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
                 ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::THEME_RESET), getLangSTR(LANG_STR::RELAUCHING_IN_5));
                 relaunch_timer_thread();
           }

           break;
        }
        if(install_IF_Theme(fullpath)){
            ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::THEME_INSTALL_SUCCESS), getLangSTR(LANG_STR::RELAUCHING_IN_5));
            relaunch_timer_thread();
        }
        else
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::THEME_NOT_VAILD), "");
        #endif
        break;
    }
    case FONT_OPTION:
    {
        if (GLES2_fonts_from_ttf(fullpath.c_str())){
            get->setting_strings[FNT_PATH] =  fullpath;
            title_vbo.clear();

            ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::NEW_FONT_STRING), fmt::format("{0:.20}: {1:.20}",  getLangSTR(LANG_STR::NEW_FONT_STRING), filename));
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
            ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::MP3_PLAYING), getLangSTR(LANG_STR::MP3_LOADED));
        else if (status == MP3_STATUS_PLAYING){
            ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::MUSIC_Q), getLangSTR(LANG_STR::PLAYING_NEXT));
        }
#endif
        break;
    }
    case IF_PKG_INSTALLER_OPTION:{
#if defined(__ORBIS__)

if ((ret = pkginstall(fullpath.c_str(), filename.c_str(), true, false)) != ITEMZCORE_SUCCESS){
    if (ret == IS_DISC_GAME)
        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::OPTION_DOESNT_SUPPORT_DISC), fmt::format("{0:#x}",  ret));
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

static void X_action_settings(int action, layout_t & l)
{
    // selected item from active panel
    log_info("execute setting %d -> '%s' for 'ItemzCore'", l.curr_item,
             l.item_d[l.curr_item].info.name.c_str());

    l.vbo_s = ASK_REFRESH;
    SETTING_V_CURR = l.curr_item;
    switch (l.curr_item)
    {
    case POWER_CONTROL_OPTION:
    case SORT_BY_OPTION:
    case REBUILD_DB_OPTION:
    case OPEN_SHELLUI_MENU:
        if (l.item_d[l.curr_item].multi_sel.is_active)
        {
            const char *uri = NULL;
            log_info("[OP]: opt: %i | Skip X?: %s", skipped_first_X, skipped_first_X ? "YES" : "NO");
            if (skipped_first_X)
            {
                if (l.curr_item == REBUILD_DB_OPTION)
                {
                    switch (l.item_d[l.curr_item].multi_sel.pos.y)
                    {
                        case multi_select_options::REBUILD_ALL:
                            log_info("REBUILD ALL");
                            #ifdef __ORBIS__
                            rebuild_db();
                            #endif
                            break;
                        case multi_select_options::REBUILD_DLC:
                            log_info("REBUILD DLC");
                            #ifdef __ORBIS__
                            rebuild_dlc_db();
                            #endif
                            break;
                        case multi_select_options::REBUILD_REACT:
                            log_info("REBUILD REACTPSN");
                            #ifdef __ORBIS__
                            if(Reactivate_external_content(false))
                                ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::ADDCONT_REACTED), "");
                            else
                                ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::ADDCONT_REACT_FAILED), "");

                            #endif
                            break;
                    }
                }
                else if (l.curr_item == OPEN_SHELLUI_MENU)
                {
                    switch (l.item_d[l.curr_item].multi_sel.pos.y)
                    {
                    case multi_select_options::SHELLUI_SETTINGS_MENU:
                        log_info("SETTINGS MENU");
                        uri = "pssettings:play?mode=settings";
                        break;
                    case multi_select_options::SHELLUI_POWER_SAVE:
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
                            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::FAILED_MENU_OPEN), fmt::format("{0:.20}: {1:#x}",  getLangSTR(LANG_STR::ERROR_CODE), ret));
                    }
#endif
                }
                else if (l.curr_item == SORT_BY_OPTION)
                {
                    get->sort_by = (multi_select_options::Sort_Multi_Sel)l.item_d[l.curr_item].multi_sel.pos.y;
                    refresh_apps_for_cf(get->sort_by, get->sort_cat);
                }
                else if (l.curr_item == POWER_CONTROL_OPTION)
                {
#if defined(__ORBIS__)
                    if(!power_settings((Power_control_sel)l.item_d[l.curr_item].multi_sel.pos.y))
                        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::ERROR_POWER_CONTROL), fmt::format("{}", strerror(errno)));
#endif
                }
                skipped_first_X = false;
            }
            else
                skipped_first_X = true;

            if (l.item_d[l.curr_item].multi_sel.pos.x == 0)
            {
                l.item_d[l.curr_item].multi_sel.pos.x = 1; // EDITABLE
            }
            else
                l.item_d[l.curr_item].multi_sel.pos.x = 0;
        }

        break;
    case BACKGROUND_MP3_OPTION:
        StartFileBrowser((view_t)v_curr, setting_p, FS_Setting_Callback, FS_MP3);
        break;
    case DUMPER_PATH_OPTION:
        StartFileBrowser((view_t)v_curr, setting_p, FS_Setting_Callback, FS_FOLDER);
        break; // ps4 keyboard func
    case IF_PKG_INSTALLER_OPTION:
        StartFileBrowser((view_t)v_curr, setting_p, FS_Setting_Callback, FS_PKG);
        break;
    case THEME_OPTION:
        StartFileBrowser((view_t)v_curr, setting_p, FS_Setting_Callback, FS_ZIP);
        break;
    case CHECK_FOR_UPDATE_OPTION:
    {
#if defined(__ORBIS__)
        loadmsg(getLangSTR(LANG_STR::CHECH_UPDATE_IN_PROGRESS));

        if(get->setting_bools[INTERNAL_UPDATE] || (ret = check_update_from_url(ITEMZFLOW_TID)) == IF_UPDATE_FOUND)
        {
            if(get->setting_bools[INTERNAL_UPDATE] || Confirmation_Msg(getLangSTR(LANG_STR::OPT_UPDATE)) == YES)
            {

                loadmsg(getLangSTR(LANG_STR::CHECH_UPDATE_IN_PROGRESS));
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

                if(do_update(get->setting_bools[INTERNAL_UPDATE] ? "http://192.168.123.176" : "https://api.pkg-zone.com"))
                {
                    ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::IF_UPDATE_SUCESS), getLangSTR(LANG_STR::RELAUCHING_IN_5));
                }
                else
                  ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::IF_UPDATE_FAILD), getLangSTR(LANG_STR::OP_CANNED_1));
            }
            else
                ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::OP_CANNED), getLangSTR(LANG_STR::OP_CANNED_1));
        }
        else if (ret == IF_NO_UPDATE)
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_UPDATE_FOUND), "");
        else if (ret == IF_UPDATE_ERROR)
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_INTERNET), getLangSTR(LANG_STR::BE_SURE_INTERNET));

        sceMsgDialogTerminate();
#endif
        break;
    }
    case HOME_MENU_OPTION:
    {
        IPC_Client& ipc = IPC_Client::getInstance();
        get->setting_bools[HomeMenu_Redirection] = get->setting_bools[HomeMenu_Redirection] ? false : true;
#if defined(__ORBIS__)
        if (is_connected_app)
        {
            log_info("Turning Home menu %s", get->setting_bools[HomeMenu_Redirection] ? "ON (ItemzFlow)" : "OFF (Orbis)");

            if (get->setting_bools[HomeMenu_Redirection])
            {
                if (ipc.IPCSendCommand(IPC_Commands::ENABLE_HOME_REDIRECT, ipc_msg) == IPC_Ret::NO_ERROR)
                    ani_notify(NOTIFI::INFO, getLangSTR(LANG_STR::REDIRECT_TURNED_ON), "");
            }
            else
            {
                if (ipc.IPCSendCommand(IPC_Commands::DISABLE_HOME_REDIRECT, ipc_msg) == IPC_Ret::NO_ERROR)
                    ani_notify(NOTIFI::INFO, getLangSTR(LANG_STR::REDIRECT_TURNED_OFF), "");
            }
        }
        else
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::DAEMON_FAILED_NOTIFY), getLangSTR(LANG_STR::DAEMON_FAILED_NOTIFY2));
#endif
        break;
    }
    case SHOW_BUTTON_OPTION:
    {

        get->setting_bools[Show_Buttons] = get->setting_bools[Show_Buttons] ? false : true;
        title_vbo.clear();
        break;
    }
    case COVER_MESSAGE_OPTION:
    {
        get->setting_bools[cover_message] = get->setting_bools[cover_message] ? false : true;
        break;
    }
    case FUSE_IP_OPTION:
    {
       // use_reflection = (use_reflection) ? false : true;
        char out[1024];
        IPC_Client& ipc = IPC_Client::getInstance();
#ifdef __ORBIS__
        // FOR NOW NO KEYPAD BECAUSE THERES NO LETTERS FOR OPTIONAL NFS SHARES
        if(Keyboard(getLangSTR(LANG_STR::SETTINGS_8).c_str(), get->setting_strings[FUSE_PC_NFS_IP].c_str(), &out[0])){
            IPC_Ret ret = IPC_Ret::NO_ERROR;
            loadmsg(getLangSTR(LANG_STR::CONNECTING_TO_FUSE));
            if((ret = ipc.IPCMountFUSE(out, "/hostapp", false)) == IPC_Ret::NO_ERROR){
                get->setting_strings[FUSE_PC_NFS_IP] = out;
                auto it = std::find_if(all_apps.begin(), all_apps.end(), [](const item_t& app) {
                         return app.info.id == APP_HOME_HOST_TID;
                });
                sceMsgDialogTerminate();
                if(it == all_apps.end())
                   refresh_apps_for_cf(get->sort_by, get->sort_cat);

                ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::NFS_CONNTECTED), "/hostapp");
            }
            else if(ret == IPC_Ret::FUSE_FW_NOT_SUPPORTED)
                msgok(MSG_DIALOG::NORMAL, fmt::format("{0:#x} {1:}", (ps4_fw_version() >> 16), getLangSTR(LANG_STR::HEN_IS_NOT_SUPPORTED)));
            else
                msgok(MSG_DIALOG::NORMAL, fmt::format("{0:}\n\nRet: {1:d}", getLangSTR(LANG_STR::NFS_CONNECT_FAILED), (int)ret));
        }
        else
          ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::OP_CANNED), getLangSTR(LANG_STR::OP_CANNED_1));
#endif
        title_vbo.clear();

        break;
    }
    case CHANGE_BACKGROUND_OPTION:
        StartFileBrowser((view_t)v_curr, setting_p, FS_Setting_Callback, FS_PNG);
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
                 ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::THEME_RESET), getLangSTR(LANG_STR::RELAUCHING_IN_5));
                 relaunch_timer_thread();
        }
#endif
        break;
    }
    case DOWNLOAD_COVERS_OPTION:
    
#if defined(__ORBIS__)
            if (Confirmation_Msg(getLangSTR(LANG_STR::DOWNLOAD_COVERS)) == YES)
            {
                
                loadmsg(getLangSTR(LANG_STR::DOWNLOADING_COVERS));
                log_info("Downloading covers...");
                for (auto item: all_apps){
                     int ret = download_texture(item);
                     if(ret != 0 && ret != 404){
                        log_error("Failed to download cover for %s", item.info.name.c_str());
                        sceMsgDialogTerminate();
                        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::FAILED_TO_DOWNLOAD_COVER), fmt::format("{0:d}", ret));
                        goto tex_err;
                     }
                }

                sceMsgDialogTerminate();
                log_info("Downloaded covers");
                ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::COVERS_UP_TO_DATE), "");
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
                ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::SAVE_ERROR), "");
            else
            {
                ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::SAVE_SUCCESS), (get->sort_cat == NO_CATEGORY_FILTER) ?  "" : getLangSTR(LANG_STR::INC_CAT));
                fmt::print("Settings saved to {}", get->setting_strings[INI_PATH]);
                // reload settings
                LoadOptions(get);
            }
        }
        else {
            log_info("nu %i", numb_of_settings);
            StartFileBrowser((view_t)v_curr, setting_p, FS_Setting_Callback, FS_FONT);
        }

        break;
    }
    case NUMB_OF_EXTRAS:
        if (numb_of_settings > NUMBER_OF_SETTINGS) // is advanced settings???
            goto save_setting;
        else
            StartFileBrowser((view_t)v_curr, setting_p, FS_Setting_Callback, FS_FONT);
        
        break;
    default:
        break;
    };
}

void printDirContentRecursively(const std::filesystem::path& path, const std::string& indent = "") {
    try {
        if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
            //std::cerr << "Path does not exist or is not a directory: " << path << std::endl;
            log_error("Path does not exist or is not a directory: %s", path.c_str());
            return;
        }

        for (const auto& entry : std::filesystem::directory_iterator(path)) {
           // std::cout << indent << entry.path().filename() << std::endl;
            log_info("Path: %s", entry.path().filename().string().c_str());
            if (std::filesystem::is_directory(entry.status())) {
                printDirContentRecursively(entry.path(), indent + "    ");
            }
        }
    } catch (std::filesystem::filesystem_error& e) {
       // std::cerr << "Filesystem error: " << e.what() << std::endl;
       log_error("Filesystem error");
    } catch (std::exception& e) {
       log_error("UNKNOWN FS ERROR");
    }
}
void Start_Dump(std::string name, std::string path, multi_select_options::Dump_Multi_Sels opt) {
  #if defined(__ORBIS__)
  std::string patch_dir;
  if (path.find("/mnt/usb") != std::string::npos) {
    if (usbpath() == -1) {
      fmt::print("path is %s However, no USB is connected...", path);
      ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_USB), "");
      return;
    }
  }

  if (if_exists(path.c_str())) {
    if (check_free_space(path.c_str()) >= app_inst_util_get_size(title_id.get().c_str())) {
      if (opt == SEL_FPKG) {
          std::string base_path = fmt::format("{}/{}", path, title_id.get());
          mkdir(base_path.c_str(), 0777);
          tmp = fmt::format("{}/{}_base.pkg", base_path, title_id.get());
          log_info("path is %s", tmp.c_str());
          if (!copyFile(all_apps[g_idx].info.package, tmp, true)){
              ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::FPKG_COPY_FAILED), "");
              return;
          }
          tmp = fmt::format("{}/{}_patch.pkg", base_path, title_id.get());
          log_info("path is %s", tmp.c_str());
          if (does_patch_exist(title_id.get(), patch_dir)){
              if (!copyFile(patch_dir, tmp, true)){
                   ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::FPKG_COPY_FAILED), "");
                   return;
               }
          }

         std::vector < std::tuple < std::string, std::string, std::string >> dlc_info = query_dlc_database(title_id.get());
         for (const auto & result: dlc_info) {
             // std::cout << "PKG: " << std::get < 0 > (result) << " | DLC Name: " << std::get < 1 > (result) << " | Full Content ID: " << std::get < 2 > (result) << std::endl;
              log_info("PKG: %s, | DLC Name: %s, content_id %s", std::get < 0 > (result).c_str(), std::get < 1 > (result).c_str(), std::get < 2 > (result).c_str());
         }

        std::string addcont_path_str;
        std::string user_addcont_dir = "/user/addcont/" + title_id.get();
        std::string ext_addcont_dir = "/mnt/ext0/user/addcont/" + title_id.get();
        std::string disc_addcont_dir = "/mnt/disc/addcont/" + title_id.get();

        if (if_exists(user_addcont_dir.c_str())) {
         addcont_path_str = user_addcont_dir;
        } else if (if_exists(ext_addcont_dir.c_str())) {
           addcont_path_str = ext_addcont_dir;
        } else {
          addcont_path_str = disc_addcont_dir;
        }

        for (const auto & result: dlc_info) {
            std::filesystem::path pkg_path(addcont_path_str + "/" + std::get < 0 > (result));
            log_info("path: %s", pkg_path.string().c_str());
            if (std::filesystem::is_directory(pkg_path)) {
                pkg_path /= "ac.pkg";
                if (std::filesystem::exists(pkg_path) && std::filesystem::is_regular_file(pkg_path)) {
                     tmp = fmt::format("{}/{}.pkg", base_path, std::get<1>(result));
                    if (!copyFile(pkg_path, tmp, true)){
                        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::FPKG_COPY_FAILED), "");
                        break;
                    }
                }
                else{
                   log_error("path: %s is NOT A PKG", pkg_path.string().c_str());
                }
            }
        }

        ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::FPKG_COPY_SUCCESS), "");

        return;
      } 
      else if (opt == SEL_GAME_PATCH) {

           if (!does_patch_exist(title_id.get(), patch_dir)) {
               ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_PATCH_AVAIL), "");
               return;
            }

      } 
      else if (opt == SEL_DLC) {

            int vaild_dlcs = 0, avail_dlcs = 0;
            if(!count_dlc(title_id.get(), vaild_dlcs, avail_dlcs, true )){
               ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_DLC_AVAIL), "");
               return;
            }
      }
      log_info("Enough free space");
      log_info("Setting dump flag to: %i", opt);
      dump = opt;
      if (app_status != RESUMABLE) {
        //auto close app before doing a crit suspend
        IPC_Client & ipc = IPC_Client::getInstance();

        Kill_BigApp(app_status);
        if (opt != SEL_DLC && (ret = ipc.IPCSendCommand(IPC_Commands::CRITICAL_SUSPEND, ipc_msg) != IPC_Ret::NO_ERROR)) {
          log_error("Daemon command failed with: %i", ret);
          msgok(MSG_DIALOG::NORMAL, getLangSTR(LANG_STR::DUMP));
        }
        Launch_App(title_id.get(), true);

      } else {
        log_info("App is already running, Sending Dump signal with opt: %i ...", dump);
        raise(SIGCONT);
      }
      return;
    } else {
      ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NOT_ENOUGH_FREE_SPACE), fmt::format("{0:d} GBs | {1:.10}: {2:d} GBs", check_free_space(path.c_str()), title_id.get(), app_inst_util_get_size(all_apps[g_idx].info.id.c_str())));

      return;
    }
  } else {
    ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::DUMPER_PATH_NO_AVAIL), "");
    return;
  }
  #else
  //log_info("Dumping %s to %s, opt: %d", name, path, opt);
  #endif
}

static void
hostapp_vapp_X_dispatch(int action, layout_t & l)
{
    IPC_Client& ipc = IPC_Client::getInstance();

    fmt::print("execute {} -> '{}' for '{}'", l.curr_item, l.item_d[l.curr_item].info.name, all_apps[g_idx].info.name);
    l.vbo_s = ASK_REFRESH;
    GAME_PANEL_V_CURR = l.curr_item;
    switch (l.curr_item)
    {
       case cf_ops::HOSTAPP_OPTS::LAUNCH_HA:
       itemzCore_Launch_util(l);
       break;
       case cf_ops::HOSTAPP_OPTS::REFRESH_HA:
       log_info("Refreshing Host App");
#ifdef __ORBIS__
            int retry_limit = 0;
            while(ipc.IPCSendCommand(IPC_Commands::RESTART_FUSE_FS, ipc_msg) != IPC_Ret::NO_ERROR && retry_limit < 3){
                loadmsg("...");
                log_info("Awaiting Fuse restart");
                sleep(1);
                retry_limit++;
            }                        
            sceMsgDialogTerminate();
            if(retry_limit >= 3){
                log_error("Fuse restart failed");
                msgok(MSG_DIALOG::NORMAL, fmt::format("{0:}\n\nRet: {1:d}", getLangSTR(LANG_STR::NFS_CONNECT_FAILED),ret));
            }
            else
               ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::NFS_CONNTECTED), "/hostapp");
#endif
       break;
    }
}

static void
X_action_dispatch(int action, layout_t & l)
{
    int ret; 
    int error = -1;
    // avoid not used warning
    ret = -1;
    fmt::print("execute {} -> '{}' for '{}'", l.curr_item, l.item_d[l.curr_item].info.name, all_apps[g_idx].info.name);

    std::string dst;
    std::string src;
    std::string tmp;
    std::string usb;

    fmt::print("all_apps[g_idx].info.id: {} all_apps[g_idx].info.name: {}", all_apps[g_idx].info.id, all_apps[g_idx].info.name);
    usb = fmt::format("/mnt/usb{}/", usbpath());

    l.vbo_s = ASK_REFRESH;
    GAME_PANEL_V_CURR = l.curr_item;
    switch (l.curr_item)
    {
    // ALL THE 3 MULTI SELS
    case cf_ops::REG_OPTS::MOVE_APPS:
    case cf_ops::REG_OPTS::UNINSTALL:
    case cf_ops::REG_OPTS::RESTORE_APPS:
    {
        if (l.item_d[l.curr_item].multi_sel.is_active)
        {
            log_info("Multi sel is active");
            if (skipped_first_X)
            {
                switch (l.curr_item)
                {
                case cf_ops::REG_OPTS::MOVE_APPS:
                {
#if defined(__ORBIS__)
                    if (usbpath() == -1)
                    {
                        log_info("No USB is connected...");
                        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_USB), "");
                        return;
                    }
                    else if (l.item_d[l.curr_item].flags.is_ext_hdd)
                    {
                        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::EXT_MOVE_NOT_SUPPORTED), "");
                        return;
                    }
                    if (l.item_d[l.curr_item].multi_sel.pos.y == multi_select_options::MOVE_APP && check_free_space(usb.c_str()) < app_inst_util_get_size(all_apps[g_idx].info.id.c_str()))
                    {
                        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NOT_ENOUGH_FREE_SPACE), fmt::format("USB: {0:d} GBs | {1:}: {2:d} GBs", check_free_space(usb.c_str()), all_apps[g_idx].info.id, app_inst_util_get_size(all_apps[g_idx].info.id.c_str())));
                        return;
                    }

                    dst = fmt::format("{}/itemzflow/apps/{}/", usb, all_apps[g_idx].info.id);
                    std::filesystem::create_directories(std::filesystem::path(dst));

                    switch (l.item_d[l.curr_item].multi_sel.pos.y)
                    {
                    case multi_select_options::MOVE_APP:
                        src = fmt::format("/user/app/{}/app.pkg", all_apps[g_idx].info.id);
                        dst = fmt::format("{}/itemzflow/apps/{}/app.pkg", usb, all_apps[g_idx].info.id);
                        break;
                    case multi_select_options::MOVE_DLC:
                        src  = fmt::format("/user/addcont/{}/ac.pkg", all_apps[g_idx].info.id);
                        dst = fmt::format("{}/itemzflow/apps/{}/app_dlc.pkg", usb, all_apps[g_idx].info.id);
                        break;
                    case multi_select_options::MOVE_PATCH:
                        src  = fmt::format("/user/patch/{}/patch.pkg", all_apps[g_idx].info.id);
                        dst = fmt::format("{}/itemzflow/apps/{}/app_patch.pkg", usb, all_apps[g_idx].info.id);
                        break;
                    default:
                        break;
                    }
                    if(if_exists(dst.c_str()))
                    {   //ALREADY BACKED UP 
                        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::THE_ITEM_IS_ALREADY_BP), getLangSTR(LANG_STR::OP_CANNED_1));
                        break;
                    }
                    if (!copyFile(src, dst, true))
                        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::MOVE_FAILED), getLangSTR(LANG_STR::OP_CANNED_1));
                    else
                    {
                        loadmsg("Validating Move...");
                        if (MD5_file_compare(src.c_str(), dst.c_str()))
                        {
                            //Backup license files
                            unlink(src.c_str());
                            if (symlink(dst.c_str(), src.c_str()) < 0)
                                log_debug("[MOVE] symlinked %s >> %s, Errno(%s)", src.c_str(), dst.c_str(), strerror(errno));

                            dst = fmt::format("{}/itemzflow/licenses", usb);
                            copy_dir((char*)"/user/license", (char*)dst.c_str());

                            switch (l.item_d[l.curr_item].multi_sel.pos.y)
                            {
                            case MOVE_APP:
                                ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::APP_MOVE_NOTIFY), all_apps[g_idx].info.id);
                                break;
                            case MOVE_DLC:
                                ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::DLC_MOVE_NOTIFY), all_apps[g_idx].info.id);
                                break;
                            case MOVE_PATCH:
                                ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::PATCH_MOVE_NOTIFY), all_apps[g_idx].info.id);
                                break;
                            }
                        }
                        else
                        {
                            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::MOVE_FAILED), getLangSTR(LANG_STR::OP_CANNED_1));
                        }
                    }
                    sceMsgDialogTerminate();
#endif
                    break;
                }
                case cf_ops::REG_OPTS::RESTORE_APPS:
                {
#if defined(__ORBIS__)
                    if (usbpath() == -1)
                    {
                        log_info("No USB is connected...");
                        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NO_USB), "");
                        return;
                    }

                    src = fmt::format("{}/itemzflow/apps/{}/app.pkg", usb, all_apps[g_idx].info.id);
                    int len = (int)CalcFreeGigs(src.c_str());
                    bool app_moved = if_exists(src.c_str());
                    src = fmt::format("{}/itemzflow/apps/{}/app_dlc.pkg", usb, all_apps[g_idx].info.id);
                    bool dlc_moved = if_exists(src.c_str());
                    src = fmt::format("{}/itemzflow/apps/{}/app_patch.pkg", usb, all_apps[g_idx].info.id);
                    bool patch_moved = if_exists(src.c_str());
                    

                    
                    if (!app_moved && !dlc_moved && !patch_moved)
                    {
                        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::APP_NOT_MOVED), "");
                        return;
                    }
                    if (l.item_d[l.curr_item].multi_sel.pos.y == RESTORE_APP && len > check_free_space(usb.c_str()))
                    {
                        ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::NOT_ENOUGH_FREE_SPACE), fmt::format("{0:d} GBs | /user/app: {1:d} GBs", check_free_space(usb.c_str()), len));
                        return;
                    }
                    switch (l.item_d[l.curr_item].multi_sel.pos.y)
                    {
                    case multi_select_options::RESTORE_APP:{
                        src = fmt::format("{}/itemzflow/apps/{}/app.pkg", usb, all_apps[g_idx].info.id);
                        dst = fmt::format("/user/app/{}/app.pkg", all_apps[g_idx].info.id);
                        break;
                    }
                    case multi_select_options::RESTORE_DLC:{
                        src = fmt::format("{}/itemzflow/apps/{}/app_dlc.pkg", usb, all_apps[g_idx].info.id);
                        dst = fmt::format("/user/addcont/{}/ac.pkg", all_apps[g_idx].info.id);
                        break;
                    }
                    case multi_select_options::RESTORE_PATCH:{
                        src = fmt::format("{}/itemzflow/apps/{}/app_patch.pkg", usb, all_apps[g_idx].info.id);
                        dst = fmt::format("/user/patch/{}/patch.pkg", all_apps[g_idx].info.id);
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
                    if ((ret = pkginstall(src.c_str(), all_apps[g_idx].info.id.c_str(), true, true)) != ITEMZCORE_SUCCESS && copy_dir(dst, "/user/license")){
                        if (ret == IS_DISC_GAME)
                            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::OPTION_DOESNT_SUPPORT_DISC), fmt::format("{0:#x}", ret));
                        else
                            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::RESTORE_FAILED), fmt::format("{0:#x}", ret));
                    }
                    else if (ret == 0)
                    {
                        fmt::print("[RESTORE] installed {} >> {}", src, dst);
                        ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::RESTORE_SUCCESS), fmt::format("@ {0:}", dst));
                        sceMsgDialogTerminate();
                    }
#endif
                    break;
                }
                case cf_ops::REG_OPTS::UNINSTALL:
                {
#if defined(__ORBIS__)
                    switch ((multi_select_options::Uninstall_Multi_Sel)get->un_opt)
                    {
                    case UNINSTALL_PATCH:
                    {
                        if (Confirmation_Msg(uninstall_get_str((multi_select_options::Uninstall_Multi_Sel)get->un_opt)) == NO)
                        {
                            break;
                        }
                        if (app_inst_util_uninstall_patch(all_apps[g_idx].info.id.c_str(), &error) && error == 0)
                            ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::UNINSTALL_UPDATE), "");
                        else
                            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::UNINSTAL_UPDATE_FAILED), fmt::format("{0:}: {1:#x} ({2:d})", getLangSTR(LANG_STR::ERROR_CODE), error, error));

                        break;
                    }
                    case UNINSTALL_GAME:
                    {
                        if (Confirmation_Msg(uninstall_get_str((Uninstall_Multi_Sel)get->un_opt)) == NO)
                        {
                            break;
                        }
                        if (app_inst_util_uninstall_game(all_apps[g_idx].info.id.c_str(), &error) && error == 0)
                        {
                            ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::UNINSTALL_GAME1), "");
                            fw_action_to_cf(0x1337);
                            ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::APPS_RELOADED), getLangSTR(LANG_STR::APPS_RELOADED_2));
                            refresh_apps_for_cf(get->sort_by, get->sort_cat);
                        }
                        else
                            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::UNINSTALL_FAILED), fmt::format("{0:}: {1:#x} ({2:d})", getLangSTR(LANG_STR::ERROR_CODE), error, error));

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

            if (l.item_d[l.curr_item].multi_sel.pos.x == 0)
                l.item_d[l.curr_item].multi_sel.pos.x = 1; // EDITABLE
            else
                l.item_d[l.curr_item].multi_sel.pos.x = 0;
        }
        break;
    } // end of huge switch
    case cf_ops::REG_OPTS::DUMP_GAME:
    {
        if (l.item_d[l.curr_item].multi_sel.is_active)
        {
            int last_opt = multi_select_options::SEL_DLC;
            fmt::print("$$$$ item {} is Multi {}", l.curr_item, skipped_first_X);

            if (l.item_d[l.curr_item].flags.is_fpkg)
                last_opt = multi_select_options::SEL_FPKG;

            if (l.item_d[l.curr_item].multi_sel.pos.x == last_opt)
                l.item_d[l.curr_item].multi_sel.pos.x = multi_select_options::SEL_DUMP_ALL;
            else if (l.item_d[l.curr_item].multi_sel.pos.x == multi_select_options::SEL_DUMP_ALL)
                l.item_d[l.curr_item].multi_sel.pos.x++;
            else
                l.item_d[l.curr_item].multi_sel.pos.x--;

            if (skipped_first_X)
            {
              log_info("dumping option: %i ...", get->Dump_opt);
#if defined(__ORBIS__)
              Start_Dump(all_apps[g_idx].info.name, get->setting_strings[DUMPER_PATH], get->Dump_opt);
#else
                Start_Dump(all_apps[g_idx].info.name, "NOT A REAL PATH", get->Dump_opt);
#endif
                skipped_first_X = false;
            }
            else
                skipped_first_X = true;
        }
        break;
    }
    case cf_ops::REG_OPTS::LAUNCH_GAME:
    {
        itemzCore_Launch_util(l);
        break;
    }
    case cf_ops::REG_OPTS::RETAIL_UPDATES: {
      // #if defined(__ORBIS__)
      log_info("getting details");
      bool con_issue = false;
#if defined(__ORBIS__)
      if (all_apps[g_idx].flags.is_fpkg) {
        //msgok(SG_DIALOG:: ,"This feature is only for retail games");
        ani_notify(NOTIFI::ERROR, getLangSTR(LANG_STR::FEATURE_DOES_NOT_SUPPORT_FPKGS), "The Selected Game is an FPKG");
        log_info("This feature is only for retail games");
        break;
      }
#endif
      loadmsg( skipped_first_X ? getLangSTR(LANG_STR::STARTING_DOWNLOAD) : getLangSTR(LANG_STR::FETCHING_UPDATES) );
      if (!skipped_first_X && !Fetch_Update_Details(title_id.get(), all_apps[g_idx].info.name, update_info, con_issue)) {
        ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::NO_PATCHES_FOUND), con_issue ? getLangSTR(LANG_STR::PATCH_CON_FAILED) : getLangSTR(LANG_STR::NO_PATCHES_FOR_GAME));
        skipped_first_X = false;
        sceMsgDialogTerminate();
        break;
      }


      if (l.item_d[l.curr_item].multi_sel.is_active) {
        if (skipped_first_X) {
          skipped_first_X = false;
         // log_info("url: %s, ver: %s", update_info.update_json[update_current_index].c_str(), update_info.update_version[update_current_index].c_str());
          if(installPatchPKG(update_info.update_json[update_current_index].c_str(), title_id.get().c_str(), all_apps[g_idx].info.picpath.c_str()) == 0){
            ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::PATCH_QUEUED), getLangSTR(LANG_STR::PATCH_ADD_TO_DOWNLOADS));
          } else {
            ani_notify(NOTIFI::ERROR, getLangSTR(LANG_STR::PATCH_QUEUE_ISSUE), "");
          }
          //break;
        } else  {
          skipped_first_X = true;
        }
      }
      // suck me
      if (l.item_d[l.curr_item].multi_sel.pos.x == update_info.update_version.size()) {
        l.item_d[l.curr_item].multi_sel.pos.x = 0;
      } else if (l.item_d[l.curr_item].multi_sel.pos.x == 0) {
        l.item_d[l.curr_item].multi_sel.pos.x++;
      } else {
        l.item_d[l.curr_item].multi_sel.pos.x--;
      }

       sceMsgDialogTerminate();
        // #endif
        break;
      }
    case cf_ops::REG_OPTS::TRAINERS:
    {
#if defined(__ORBIS__)
        if (get_patch_path(all_apps[g_idx].info.id)){
        patch_current_index = l.item_d[l.curr_item].multi_sel.pos.y;
        if (skipped_first_X) {
            std::string patch_file = fmt::format("{}/patches/xml/{}.xml", patch_path_base, all_apps[g_idx].info.id);
            std::string patch_title = trs.patcher_title.at(patch_current_index);
            std::string patch_name = trs.patcher_name.at(patch_current_index);
            std::string patch_app_ver = trs.patcher_app_ver.at(patch_current_index);
            std::string patch_app_elf = trs.patcher_app_elf.at(patch_current_index);
            u64 hash_ret = patch_hash_calc(patch_title, patch_name, patch_app_ver, patch_file, patch_app_elf);
            std::string hash_id = fmt::format("{:#016x}", hash_ret);
            std::string hash_file = fmt::format("{}/patches/settings/{}.txt", patch_path_base, hash_id);
            write_enable(hash_file, patch_name);
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
            l.item_d[l.curr_item].multi_sel.pos.x = RESET_MULTI_SEL;
            l.vbo_s = ASK_REFRESH;
            skipped_first_X = false;
            break;
        }
        std::string patch_file = fmt::format("{}/patches/xml/{}.xml", patch_path_base, all_apps[g_idx].info.id);
        pugi::xml_document doc;
        if (!doc.load_file(patch_file.c_str()))
        {
            msgok(MSG_DIALOG::NORMAL, fmt::format("File {} open failed", patch_file));
        }
        pugi::xml_node patches = doc.child("Patch");
        u32 patch_count = 0;
        for (pugi::xml_node patch = patches.child("Metadata"); patch; patch = patch.next_sibling("Metadata"))
        {
            std::string gameTitle = patch.attribute("all_apps[g_idx].info.name").value();
            std::string gameName = patch.attribute("Name").value();
            std::string gameNote = patch.attribute("Note").value();
            std::string gameAuthor = patch.attribute("Author").value();
            std::string gamePatchVer = patch.attribute("PatchVer").value();
            std::string gameAppver = patch.attribute("AppVer").value();
            std::string gameAppElf = patch.attribute("AppElf").value();
            if (all_apps[g_idx].info.version.compare(gameAppver) != 0) continue;
            // don't assign if patch version and app version don't match
            get_metadata1(&trs, gameAppver, gameAppElf, gameTitle, gamePatchVer, gameName, gameAuthor, gameNote);
            patch_count++;
            std::string patch_control_title = fmt::format("{1} {0}", patch_count, getLangSTR(LANG_STR::PATCH_NUM));
            trs.controls_text.push_back(patch_control_title);
        }
        skipped_first_X = true;
        if (l.item_d[l.curr_item].multi_sel.pos.x == trs.controls_text.size()) {
            l.item_d[l.curr_item].multi_sel.pos.x = 0;
        }
        else if (l.item_d[l.curr_item].multi_sel.pos.x == 0) {
            l.item_d[l.curr_item].multi_sel.pos.x++;
        }
        else {
            l.item_d[l.curr_item].multi_sel.pos.x--;
        }
        break;
        } else {
            std::string patchq = fmt::format("{}\n{}", getLangSTR(LANG_STR::NO_PATCHES), getLangSTR(LANG_STR::PATCH_DL_QUESTION));
            int conf = Confirmation_Msg(patchq);
            if(conf == YES){
                dl_patches();
            } else if (conf == NO) {
                msgok(MSG_DIALOG::NORMAL, getLangSTR(LANG_STR::PATCH_DL_QUESTION_NO));
            }
            sceMsgDialogTerminate();
            break;
        }
#endif
        break;
    }
#if defined(__ORBIS__)
    case cf_ops::REG_OPTS::HIDE_APP:
    {

        log_info("App vis b4: %i", all_apps[g_idx].flags.app_vis);
        msgok(MSG_DIALOG::WARNING, getLangSTR(LANG_STR::DB_MOD_WARNING));

        if (!AppDBVisiable(all_apps[g_idx].info.id, VIS_WRTIE, all_apps[g_idx].flags.app_vis ? 0 : 1))
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::UNHIDE_FAILED_NOTIFY), "");
        else
        {
            if (all_apps[g_idx].flags.app_vis)
                ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::UNHIDE_NOTIFY), getLangSTR(LANG_STR::HIDE_SUCCESS_NOTIFY));
            else
                ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::UNHIDE_NOTIFY), getLangSTR(LANG_STR::UNHIDE_SUCCESS_NOTIFY));

            AppVis = all_apps[g_idx].flags.app_vis ^= 1;
        }
        break;
    }
    case cf_ops::REG_OPTS::CHANGE_NAME:
    {
        // SCE Keyboard C Function needs a char array
        msgok(MSG_DIALOG::WARNING, getLangSTR(LANG_STR::DB_MOD_WARNING));
        char buf[255];

        if (!Keyboard(getLangSTR(LANG_STR::CHANGE_APP_NAME).c_str(), all_apps[g_idx].info.name.c_str(), &buf[0]))
            return;

        tmp = std::string(&buf[0]);

        if (change_game_name(tmp, all_apps[g_idx].info.id, all_apps[g_idx].info.sfopath))
        {
            {
                std::lock_guard<std::mutex> lock(disc_lock);
                all_apps[g_idx].info.name= tmp;
            }
            
            ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::TITLE_CHANGED), getLangSTR(LANG_STR::PS4_REBOOT_REQ));
        }
        else
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::FAILED_TITLE_CHANGE), "0xDEADBEEF");

        break;
    }
#endif
    case cf_ops::REG_OPTS::CHANGE_ICON:
        StartFileBrowser((view_t)v_curr, ls_p, FS_GP_Callback, FS_PNG);
        break;
    default:
        break;
    }
}

static void item_page_Square_action(layout_t & l)
{
#if defined(__ORBIS__)
    if (l.item_d[l.curr_item].multi_sel.is_active && l.item_d[l.curr_item].multi_sel.pos.x != RESET_MULTI_SEL)
    {
        switch (l.curr_item)
        {
        case cf_ops::REG_OPTS::TRAINERS:
            if (get_patch_path(all_apps[g_idx].info.id)){
                patch_current_index = l.item_d[l.curr_item].multi_sel.pos.y;
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
                                                    getLangSTR(LANG_STR::PATCH_GAME_TITLE),
                                                    patch_title,
                                                    getLangSTR(LANG_STR::PATCH_NAME),
                                                    patch_name,
                                                    getLangSTR(LANG_STR::PATCH_APP_VER),
                                                    patch_app_ver,
                                                    getLangSTR(LANG_STR::PATCH_AUTHOR),
                                                    patch_author,
                                                    getLangSTR(LANG_STR::PATCH_NOTE),
                                                    patch_note,
                                                    getLangSTR(LANG_STR::PATCH_VER),
                                                    patch_ver,
                                                    getLangSTR(LANG_STR::PATCH_APP_ELF),
                                                    patch_app_elf);
                msgok(MSG_DIALOG::NORMAL, patch_msg);
            }
            break;
        default:
            break;
        }

        return;
    }
#endif
   Favorites favorites;
   if(favorites.loadFromFile(APP_PATH("favorites.dat")))
      log_info("Favorites have beend loaded!");
   else
      log_warn("Failed to load favorites.dat does it exist??");


   all_apps[g_idx].flags.is_favorite ? favorites.removeFavorite(all_apps[g_idx].info.id) : favorites.addFavorite(all_apps[g_idx].info.id);
   all_apps[g_idx].flags.is_favorite = all_apps[g_idx].flags.is_favorite ? false : true;

   favs.clear();

   favorites.saveToFile(APP_PATH("favorites.dat"));

   //fw_action_to_cf(CIR);
   //refresh_apps_for_cf(get->sort_by, get->sort_cat);
   if (get->sort_cat == FILTER_FAVORITES) {
       fw_action_to_cf(CIR);
       refresh_apps_for_cf(get->sort_by, get->sort_cat);
   }


   ani_notify(NOTIFI::GAME, getLangSTR(LANG_STR::FAVORITES), all_apps[g_idx].flags.is_favorite ? getLangSTR(LANG_STR::FAVORITES_ADD) : getLangSTR(LANG_STR::FAVORITES_REMOVED));

    return;
}
static void item_page_Triangle_action(layout_t & l)
{
#if defined(__ORBIS__)
    if (l.item_d[l.curr_item].multi_sel.is_active && l.item_d[l.curr_item].multi_sel.pos.x != RESET_MULTI_SEL)
    {
        switch (l.curr_item)
        {
        case cf_ops::REG_OPTS::TRAINERS:
        {
            int conf = Confirmation_Msg(getLangSTR(LANG_STR::PATCH_DL_QUESTION));
            if(conf == YES){
                dl_patches();
            } else if (conf == NO) {
                msgok(MSG_DIALOG::NORMAL, getLangSTR(LANG_STR::PATCH_DL_QUESTION_NO));
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

static void item_page_O_action(layout_t & l)
{
    if (l.item_d.size() >= l.curr_item && l.item_d[l.curr_item].multi_sel.is_active
        &&l.item_d[l.curr_item].multi_sel.pos.x != RESET_MULTI_SEL)
    {
        l.item_d[l.curr_item].multi_sel.pos.y = l.item_d[l.curr_item].multi_sel.pos.x = RESET_MULTI_SEL;
        l.item_d[l.curr_item].multi_sel.is_active = false;
        switch (l.curr_item)
        {
        case cf_ops::REG_OPTS::UNINSTALL: get->un_opt = multi_select_options::UNINSTALL_GAME;
        case cf_ops::REG_OPTS::TRAINERS:
        case cf_ops::REG_OPTS::DUMP_GAME: get->Dump_opt = multi_select_options::SEL_DUMP_ALL;
        case cf_ops::REG_OPTS::RETAIL_UPDATES:
        case cf_ops::REG_OPTS::LAUNCH_GAME:
        case cf_ops::REG_OPTS::RESTORE_APPS:
        case cf_ops::REG_OPTS::MOVE_APPS:
            goto multi_back;
        default:
            break;
        }
    }
    
    l.is_active = l.is_shown = false;
    ls_p.curr_item = 0;

    setting_p.curr_item = 0;
    ls_p.is_active = false;
    ls_p.vbo_s = ASK_REFRESH;
    v2 = set_view_main(ITEMzFLOW);

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

    update_info.update_title.clear();
    update_info.update_tid.clear();
    update_info.update_version.clear();
    update_info.update_size.clear();
    update_info.update_json.clear();
    update_current_index = 0;

    l.vbo_s = ASK_REFRESH;
    skipped_first_X = false;
}
void fw_action_to_cf(int button)
{
    // ask to refresh vbo to update all_apps[g_idx].info.name
    // avoid not used warning
    IPC_Client& ipc = IPC_Client::getInstance();
    bool is_file_manager = (v_curr == FILE_BROWSER_LEFT || v_curr == FILE_BROWSER_RIGHT);

    ret = -1;

    if (is_file_manager)
    {
        fw_action_to_fm(button);
        return;
    }

    GLES2_UpdateVboForLayout(ls_p);
    GLES2_UpdateVboForLayout(ls_p);
    GLES2_UpdateVboForLayout(setting_p);

    GLES2_refresh_sysinfo();

    if (ani[0].is_running)
        return; // don't race any running ani

    if (v_curr == ITEMzFLOW)
    {
        switch (button)
        { // movement
        case L2:
        if (get->sort_cat != NO_CATEGORY_FILTER){
            get->sort_cat = NO_CATEGORY_FILTER;
            refresh_apps_for_cf(get->sort_by);
            v2 = set_view_main(ITEMzFLOW);
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
            //reset selection pos
            setting_p.curr_item = 0;
            setting_p.f_sele = 0;
            v2 = set_view_main(ITEM_SETTINGS);
    

            //StartFileBrowser((view_t)v_curr, active_p, NULL, FS_NONE);
            break;
        //case 
        case CRO:
        {
            confirm_close = false;
            // Placeholder games are not interactable
            if (all_apps[g_idx].flags.is_ph){
                ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::PLACEHOLDER_NOT_ENTER), "");
                return;
            }

            //reset selection pos
            ls_p.curr_item = 0;
            ls_p.f_sele = 0;

            std::lock_guard<std::mutex> lock(disc_lock);
            title_id.set(all_apps[g_idx].info.id);
            started_epoch =  mins_played = MIN_STATUS_RESET;

            favs.clear();
            // game save clear
            gm_save.is_loaded = false;
            if(is_vapp(all_apps[g_idx].info.id)){ // REFRESH_HOSTAPP=Refresh Hostapp
                if(all_apps[g_idx].info.id == APP_HOME_HOST_TID)
                   gm_p_text[1] = fmt::format("{0:.20}", getLangSTR(LANG_STR::REFRESH_HOSTAPP)); 
            } 
            else
                gm_p_text[1] = fmt::format("{0:.20}", getLangSTR(LANG_STR::DUMP_1));


            // reset game panel to not active so it gets refreshed
            ls_p.is_active = false;
            fmt::print("|{}|: is_fpkg: {} all_apps[g_idx].flags.app_vis: {} {}", all_apps[g_idx].info.name, all_apps[g_idx].flags.is_fpkg, all_apps[g_idx].flags.app_vis, g_idx);
            // Every X press on a Game case, we will check if the game selected is opened or not
            // If the App Status changed while in this view_main the Signal Handler will update the app status and view_main
            // then the render function will redraw text if the app status changed
            // First we check if the game is opened, if not sceLncUtilGetAppId will return -1
            // After we check if the game id is == to the currently opened bigapps id
#if defined(__ORBIS__)
            int id = sceLncUtilGetAppId(all_apps[g_idx].info.id.c_str());
            if ((all_apps[g_idx].info.id == vapp_title_launched && is_remote_va_launched) ||
             ((id & ~0xFFFFFF) == 0x60000000 && id == sceSystemServiceGetAppIdOfBigApp()))
            {
                fmt::print("found app to resume: {0:}, AppId: {1:#x}", all_apps[g_idx].info.id, id);
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
            // set the view_main to the select game
            v2 = set_view_main(ITEM_PAGE);
           // v_curr = ITEM_PAGE;
            break; // in_out
        }
        case TRI:
        { // REFRESH APP
            // Update view_main
            ani[0].is_running = true;
            confirm_close = false;
            refresh_apps_for_cf(get->sort_by, get->sort_cat);
            ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::APPS_RELOADED), getLangSTR(LANG_STR::APPS_RELOADED_2)); 
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
            ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::UPDATED_CATEGORY), category_get_str(get->sort_cat));
            break;
        }
        /*case UP:   gx -= 5.; log_info("gx:%.3f", gx); break;
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
                ani_notify(NOTIFI::INFO, getLangSTR(LANG_STR::BUTTON_CONFIRM_CLOSE), "");
                confirm_close = true;
            }

            drop_some_coverbox();
            v2 = set_view_main(ITEMzFLOW);
        }
            return;
        }
    }
    else if (v_curr == ITEM_PAGE) // move into game_option_panel
    {
        switch (button)
        { // movement
        case LEF:
            layout_update_sele(ls_p, -1);
            break; // l_or_r
        case RIG:
            layout_update_sele(ls_p, +1);
            break;
        case UP:
            layout_update_sele(ls_p, -ls_p.fieldsize.x);
            break;
        case DOW:
            layout_update_sele(ls_p, +ls_p.fieldsize.x);
            break;

        case CRO:
           if(is_vapp(all_apps[g_idx].info.id)){
              if(all_apps[g_idx].info.id == APP_HOME_HOST_TID){
                 hostapp_vapp_X_dispatch(0, ls_p);
               }
            }
            else // reg ps4 app
               X_action_dispatch(0, ls_p);

            break; // in_out

        case SQU:
            item_page_Square_action(ls_p);
            break;

        case CIR:
            item_page_O_action(ls_p);
            break;

        case OPT:
            //eject disc
            if(inserted_disc.load() > 0){
#if __ORBIS__
                if(Confirmation_Msg(getLangSTR(LANG_STR::EJECT_DISK)) == NO){
                   ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::OP_CANNED), "");
                   break;
                }

                if((ret = sceShellCoreUtilRequestEjectDevice("/dev/cd0")) < 0)
                  ani_notify(NOTIFI::ERROR, getLangSTR(LANG_STR::EJECT_DISK_ERROR), fmt::format("{0:#x}", ret));
                else
                  ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::EJECT_DISK_SUCCESS), "");
#endif
            inserted_disc = -1;
            break;
        }
        case TRI:
        {
            item_page_Triangle_action(ls_p);
            /*
            app_status = OTHER_APP_OPEN; // set the app status to no app opened
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::OPT_NOT_PERMITTED), "");
            */
            break; // goto back_to_Store;
        }
        
        case 0x1337:
        {
            log_info("called 0x1337");
            v2 = set_view_main(ITEMzFLOW);
            break;
        }
        }
    }
    else if (v_curr == ITEM_SETTINGS) // move into game_option_panel
    {
        switch (button)
        { // movement
        case LEF:
            layout_update_sele(setting_p, -1);
            break; // l_or_r
        case RIG:
            layout_update_sele(setting_p, +1);
            break;
        case UP:
            layout_update_sele(setting_p, -setting_p.fieldsize.x);
            break;
        case DOW:
            layout_update_sele(setting_p, +setting_p.fieldsize.x);
            break;
            // case OPT:
        case CRO:
            X_action_settings(0, setting_p);
            break; // in_out
        case R1:
        {
           if (numb_of_settings == NUMBER_OF_SETTINGS)
           {
               numb_of_settings += 6; // add 6 to the number of settings to show the extra settings
               ITEMZ_SETTING_TEXT[11] = fmt::format("{0:.30}", getLangSTR(LANG_STR::EXTRA_SETTING_6));
               ITEMZ_SETTING_TEXT[17] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_12));
           }
           else
           {
               numb_of_settings = NUMBER_OF_SETTINGS; // remove 6 to the number of settings to hide the extra settings
               ITEMZ_SETTING_TEXT[11] = fmt::format("{0:.30}", getLangSTR(LANG_STR::SETTINGS_12));
           }

           /* DELETE SETTINGS view_main */
           setting_p.vbo.clear();
           setting_p.f_rect.clear();
           title_vbo.clear();

           setting_p.is_active = false;
           setting_p.item_d.clear();
           break;
        }

        case OPT:
            //eject disc
            if(inserted_disc.load() > 0){
#if __ORBIS__
                if(Confirmation_Msg(getLangSTR(LANG_STR::EJECT_DISK)) == NO){
                   ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::OP_CANNED), "");
                   break;
                }

                if((ret = sceShellCoreUtilRequestEjectDevice("/dev/cd0")) < 0)
                  ani_notify(NOTIFI::ERROR, getLangSTR(LANG_STR::EJECT_DISK_ERROR), fmt::format("{0:#x}", ret));
                else
                  ani_notify(NOTIFI::SUCCESS, getLangSTR(LANG_STR::EJECT_DISK_SUCCESS), "");
#endif
                  inserted_disc = -1;
            }
            else
              goto back_05905;

            break;
        case CIR:
        {
back_05905:
            setting_p.vbo.clear();
            setting_p.is_active = false;
            O_action_settings(setting_p);
            break;
        }
        case SQU:
        {
            if(setting_p.curr_item == BACKGROUND_MP3_OPTION){
                log_info("Disabling Background MP3 ...");
#ifdef __ORBIS__
                Stop_Music();
#endif
                setting_p.vbo.clear();
            }
            else if(numb_of_settings > NUMBER_OF_SETTINGS) {
                log_debug("Killing daemon ...");
                #ifdef __ORBIS__
                if(is_connected_app){
                   
                   ipc.IPCSendCommand(IPC_Commands::CMD_SHUTDOWN_DAEMON, ipc_msg);
                   is_connected_app = false;
                }
                #endif
            }
            break;
        }

        case TRI:
        {
#ifdef __ORBIS__
          if (Confirmation_Msg(getLangSTR(LANG_STR::UPLOAD_LOGS)) == YES) {

              upload_crash_log(false);
            }
            else{
                ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::OP_CANNED), "");
            }
#endif
            break; // goto back_to_Store;
        }
        }
    }
}
void load_button(const char* file_name, GLuint &tex){

    std::string file_path;

    if (tex > GL_NULL)
       return;

    if (get->setting_bools[using_theme])
        file_path = fmt::format("{0:}/theme/{1:}", APP_PATH(""), file_name);
     else
        file_path = fmt::format("{0:}{1:}", asset_path("buttons/"), file_name);

    log_debug("tex: %i", tex);
    tex = load_png_asset_into_texture(file_path.c_str());
    log_debug("load_button: %s | tex: %i", file_path.c_str(), tex);
    

}
extern bool file_options;
//bool fav_list[8] = {false, false, false, false, false, false, false, false};
int center_cover = 0;
void draw_additions(view_t vt)
{
    load_button("btn_x.png", btn_X);
    load_button("btn_o.png", btn_o); 
    load_button("../heart.png", heart_icon);
    load_button("btn_tri.png", btn_tri);
    load_button("btn_options.png", btn_options);
    load_button("btn_down.png", btn_down);
    load_button("btn_up.png", btn_up);
    load_button("btn_left.png", btn_left);
    load_button("btn_right.png", btn_right);
    load_button("btn_sq.png", btn_sq);
    load_button("btn_l1.png", btn_l1);
    load_button("btn_r1.png", btn_r1);
    load_button("btn_l2.png", btn_l2);
    load_button("../debug_item_icon.png", debug_icon);
    bool is_file_manager = (v_curr == FILE_BROWSER_LEFT || v_curr == FILE_BROWSER_RIGHT);


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
                // char *p = strstr( l.item_d[i].info.picpath.c_str(), "storedata" );
                //  draw texture, use passed frect
                if(all_apps[0].count.token_c > 0) // if no apps dont draw x
                   on_GLES2_Render_icon(USE_COLOR, btn_X, 2, r, NULL);

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
                    
                    as_text.clear();
                    remove_music.clear();
                    stop_daemon.clear();
                    upload_logs_text.clear();

                    
                    as_text = VertexBuffer("vertex:3f,tex_coord:2f,color:4f");

                    if (setting_p.curr_item == BACKGROUND_MP3_OPTION &&
                     !get->setting_strings[MP3_PATH].empty())
                    {
                        remove_music = VertexBuffer("vertex:3f,tex_coord:2f,color:4f");
                        render_button(btn_sq, 67., 68., 500., 55., 1.1);
                        itemzcore_add_text(remove_music, 560., 80., getLangSTR(LANG_STR::DISABLE_MUSIC));

                    }
#ifdef __ORBIS__
                    else if (is_connected_app && numb_of_settings != NUMBER_OF_SETTINGS){
#else
                    else if (numb_of_settings != NUMBER_OF_SETTINGS){
#endif                  
                        stop_daemon = VertexBuffer("vertex:3f,tex_coord:2f,color:4f");
                        render_button(btn_sq, 67., 68., 500., 55., 1.1);
                        itemzcore_add_text(stop_daemon, 560., 80., getLangSTR(LANG_STR::SHUTDOWN_DAEMON));
                        stop_daemon.render_vbo(NULL);
                    }
                    else if(setting_p.curr_item != BACKGROUND_MP3_OPTION && !get->setting_strings[MP3_PATH].empty()){
                    
                        // upload crash log option
                        upload_logs_text = VertexBuffer("vertex:3f,tex_coord:2f,color:4f");
                        render_button(btn_tri, 67., 68., 500., 55., 1.1);
                        itemzcore_add_text(upload_logs_text, 560., 80., getLangSTR(LANG_STR::UPLOAD_LOGS));
                        upload_logs_text.render_vbo(NULL);
                    }
                    
                    itemzcore_add_text(as_text, 580., 25., (numb_of_settings == NUMBER_OF_SETTINGS) ? getLangSTR(LANG_STR::ADVANCED_SETTINGS) : getLangSTR(LANG_STR::HIDE_ADVANCED_SETTINGS));

                    as_text.render_vbo(NULL);
                }

                if (vt == ITEM_PAGE)
                { // && is_trainer_page == true
                    render_button(btn_sq, 67., 68., 525., 0., 1.1);
                    
                    if(ls_p.curr_item != cf_ops::REG_OPTS::TRAINERS){

                         if(!favs){
                           favs = VertexBuffer("vertex:3f,tex_coord:2f,color:4f");    
                           itemzcore_add_text(favs,585., 20., all_apps[g_idx].flags.is_favorite ? getLangSTR(LANG_STR::FAVORITES_REMOVE) : getLangSTR(LANG_STR::FAVORITES_DESC));//
                        }

                        favs.render_vbo(NULL);

                    }
                    else if (ls_p.curr_item == cf_ops::REG_OPTS::LAUNCH_GAME)
                    {
                        //started_epoch
                        if(started_epoch.load() > 0){
                           launch_text.clear();
                           launch_text = VertexBuffer("vertex:3f,tex_coord:2f,color:4f");
                           if(launch_text){
                               char buff[21];
                               const time_t t = started_epoch.load();
                               strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&t));
                               //log_info("started_epoch: %ld", (long)t);
                               itemzcore_add_text(launch_text, 660., 45., fmt::format("{} - {:.19}", getLangSTR(LANG_STR::PLAYING_SINCE), buff));
                               launch_text.render_vbo(NULL);
                           }
                       }
                    }
                    else
                    {
                        render_button(btn_tri, 67., 68., 525., 45., 1.1);
                        if(!trainer_btns){
                           trainer_btns = VertexBuffer("vertex:3f,tex_coord:2f,color:4f");      
                           itemzcore_add_text(trainer_btns,585., 65., getLangSTR(LANG_STR::BUTTON_PATCH_TRI_INFO));
                           itemzcore_add_text(trainer_btns,585., 20., getLangSTR(LANG_STR::BUTTON_PATCH_SQUARE_INFO));
                        }
                        //   ftgl_render_vbo(trainer_btns, NULL);
                        trainer_btns.render_vbo(NULL);
                    }
                }
                if (inserted_disc.load() > 0)
                    render_button(btn_options, 84., 145., 25., 90., 1.5);
            }
        }
        else if (is_file_manager)
        {
            title_vbo.clear();

            title_vbo = VertexBuffer("vertex:3f,tex_coord:2f,color:4f");

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

            itemzcore_add_text(title_vbo,80., 110., getLangSTR(LANG_STR::FS_CONTROL_BUTTON_1));
            itemzcore_add_text(title_vbo,80., 70., getLangSTR(LANG_STR::FS_CONTROL_BUTTON_2));
            itemzcore_add_text(title_vbo,80., 155., getLangSTR(LANG_STR::FS_CONTROL_BUTTON_3));

            if (fs_ret.filter != FS_NONE){
                itemzcore_add_text(title_vbo, 365., 125., (fs_ret.filter == FS_FOLDER) ? getLangSTR(LANG_STR::FS_CONTROL_BUTTON_4_1) : getLangSTR(LANG_STR::FS_CONTROL_BUTTON_4));
                itemzcore_add_text(title_vbo, 1550., 50., (fs_ret.filter == FS_FOLDER) ? getLangSTR(LANG_STR::SELECT_A_FOLDER) : getLangSTR(LANG_STR::SELECT_A_FILE));
                itemzcore_add_text(title_vbo, 620., 120., getLangSTR(LANG_STR::FS_CONTROL_BUTTON_6));
            }
            else if (fs_ret.filter == FS_NONE && !file_options){
                itemzcore_add_text(title_vbo, 365., 125., "File Options");
                itemzcore_add_text(title_vbo, 620., 120., getLangSTR(LANG_STR::FS_CONTROL_BUTTON_6));
            }
            else if (fs_ret.filter == FS_NONE && file_options){
                itemzcore_add_text(title_vbo, 365., 125., "Select cunt");
                //itemzcore_add_text(title_vbo, 1550., 50., (fs_ret.filter == FS_FOLDER) ? getLangSTR(LANG_STR::SELECT_A_FOLDER) : getLangSTR(LANG_STR::SELECT_A_FILE));
                itemzcore_add_text(title_vbo, 620., 120., "Select cunt2");
            }

            itemzcore_add_text(title_vbo,370., 65., getLangSTR(LANG_STR::BUTTON_UP_DOWN_INFO));
            itemzcore_add_text(title_vbo,620., 65., getLangSTR(LANG_STR::FS_CONTROL_BUTTON_5));
            //itemzcore_add_text(title_vbo,620., 120., getLangSTR(LANG_STR::FS_CONTROL_BUTTON_6));

            title_vbo.render_vbo(NULL);
        }
    }
}


static void LoadIconsAsync(item_t& item){
    uint64_t fmem = 0;

    check_tex_for_reload(item);
    #if defined(__ORBIS__)
    sceKernelAvailableFlexibleMemorySize(&fmem);
    // calc the progress %
    //fill up the available VRAM with PNGs (to reduce loading times) but keep enough to do other things
    if(fmem < MB(40))
        return;
    #endif

    check_n_load_textures(item);
}

void DrawScene_4(void)
{
    /* animation alpha fading */
    vec4 colo = (1.), ani_c;
    /* animation vectors */
    vec3 ani_p,
        ani_r;
    /* animation timing */
    double ani_ratio = 0.;

    if(mins_played > 0 || mins_played == MIN_STATUS_NA 
    || mins_played == MIN_STATUS_VAPP){
           title_vbo.clear();
    }
    /* animation l/r */
    const vec3 offset = (vec3){1.05, 0., 0.};

    /* delta view_main movement vector */
    vec3 view_main_v = v2 - v1;

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
                CRO op_on view_main_v */
            if (ani[0].handle == &g_idx)
            {                                 // ani ended, move global index by sign!
                g_idx += (xoff > 0) ? 1 : -1; // L:-1, R:+1
                // bounds checking on item count
                {
                   std::lock_guard<std::mutex> lock(disc_lock);
                   if (g_idx < 1)
                      g_idx = all_apps[0].count.token_c;
                   if (g_idx > all_apps[0].count.token_c)
                      g_idx = 1;
                }

            }
            if (ani[0].handle == &v_curr)
            { // ani ended: now finally switch the view_main
                v1 = v2;

                v_curr = up_next;
            }
            // detach
            ani.clear();
            ani[0].is_running = false;

            xoff = 0.; // ani_flag, pm_flag
            // refresh all_apps[g_idx].info.name
            title_vbo.clear();
        }
        // the (delta!) fraction
        view_main_v *= ani_ratio;

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
            view_main_v.x, view_main_v.y, view_main_v.z);
#endif
    }
    else
        xoff = 0.; // no ani, no offset

    // update time
    g_Time = u_t;
    // add start point to view_main vector
    view_main_v += v1;

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
        pixelshader_render(); // use PS_symbols shader
    else
    { /* bg image: fullscreen frect (normalized coordinates) */
        vec4 r = (vec4){-1., -1., 1., 1.};
        // see if its even loaded
        if (bg_tex > GL_NULL)
            on_GLES2_Render_icon(USE_COLOR, bg_tex, 2, r, NULL);
    }

    int i, j, c = 0;
    /* loop outer to inner, care overlap */
    for (i = (all_apps[0].count.token_c > 1) ? pm_r : 0; i > 0; i--) // ...3, 2, 1
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
            mat4_set_identity(&model_main);
            mat4_translate(&model_main, 0, 0, 0);
            // move to "floor" plane
            mat4_translate(&model_main, 0, 1.25 / 2., 0);
            // rotations
            mat4_rotate(&model_main, rot.x, 1, 0, 0);
            mat4_rotate(&model_main, rot.y, 0, 1, 0);
            mat4_rotate(&model_main, rot.z, 0, 0, 1);
            // set -DEFAULT_Z_CAMERA
            mat4_translate(&model_main, pos.x, pos.y, pos.z);
            if (1) // ani_view_main
            {
                mat4_set_identity(&view_main);
                mat4_translate(&view_main, view_main_v.x, view_main_v.y, view_main_v.z);
            }

            /* don't USE */ // glEnable( GL_DEPTH_TEST );
            {
                glUniformMatrix4fv(glGetUniformLocation(sh_prog1, "model"), 1, 0, model_main.data);
                glUniformMatrix4fv(glGetUniformLocation(sh_prog1, "view"), 1, 0, view_main.data);
                glUniformMatrix4fv(glGetUniformLocation(sh_prog1, "projection"), 1, 0, projection_main.data);
                glUniform4f(glGetUniformLocation(sh_prog1, "Color"), colo.r, colo.g, colo.b, colo.a);
            }
            cover_o.render(GL_TRIANGLES);
        }
        glUseProgram(0);
        //
        /* index texture from all_apps array */
        
        int tex_idx = g_idx + j;
        //log_info("tex_idx: %d j: %d", tex_idx, j);
        // check bounds: skip first
        if (tex_idx < 1)
            tex_idx += all_apps[0].count.token_c;
        else if (tex_idx > all_apps[0].count.token_c)
            tex_idx -= all_apps[0].count.token_c;

        if (Download_icons)
        {
           

#if 1

                if (get->setting_bools[cover_message] && Confirmation_Msg(getLangSTR(LANG_STR::DOWNLOAD_COVERS)) == YES)
                {
                
                    loadmsg(getLangSTR(LANG_STR::DOWNLOADING_COVERS));
                    log_info("Downloading covers...");

                    for (auto item: all_apps){
                        download_texture(item);
                    }

                  //  get->setting_bools[cover_message] = false;
                  //  SaveOptions(get);

                    sceMsgDialogTerminate();
                    log_info("Downloaded covers");
                }
                else
                  log_info("Download covers canceled");
            
#endif
#if 1
                int i = 0;
                #if defined(__ORBIS__)
                progstart("Pre-loading Covers...");
                #endif
                for (item_t &item : all_apps)
                {
                    LoadIconsAsync(item);
                    check_n_draw_textures(item, +1, colo);

                    if(item.info.id.empty())
                       log_error("wtf");
                    #if defined(__ORBIS__)
                    ProgressUpdate(((i * 100) / all_apps.size()), "Pre-loading Covers...");
                    #endif
                    i++;
                }
                sceMsgDialogTerminate();
#endif

            log_info("all_apps[0].count.token_c: %i", all_apps[0].count.token_c);
            Download_icons = false;
        }
        
        sceMsgDialogTerminate();
        // rdisc_i
        mat4 rdisc_model_main;
        // load textures (once)
        check_n_load_textures(all_apps[tex_idx]);
        // draw coverbox, if not avail craft coverbox from icon
        check_n_draw_textures(all_apps[tex_idx], +1, colo);
        // log_info("000000000000");
        
        //log_info("tex_idx: %d, inserted_disc.load() %i", tex_idx, inserted_disc.load());
        if (tex_idx == inserted_disc.load()) // single diff item
        {
            rdisc_rz += 1.; // rotation on Z
            // save for restore later
            mat4 model_main_b = model_main;
            mat4_set_identity(&model_main);
            mat4_translate(&model_main, 0, 0, 0);
    //      circle shadow does not rotates(?)
            mat4_rotate(&model_main, rdisc_rz, 0, 0, 1);
            // move to "floor" plane
            mat4_translate(&model_main, 0, 1. /2., 0);
            // move to final pos
            mat4_translate(&model_main, pos.x, pos.y, pos.z /* a little out from cover */ + .025f);

            // draw a fake shadow
            vec4 sh_col = vec4 {0., 0., 0., .75};
            // we need a new fragment shader / review_main the fragment shaders sources to clean the mess
            // actually texture is drawn plain, color does not change nothing
            rdisc_i.render_tex(rdisc_tex, /* rdisc_texture */ -2, sh_col);

            // re-position
            mat4_set_identity(&model_main);
            mat4_translate(&model_main, 0, 0, 0);
            mat4_rotate(&model_main, rdisc_rz, 0, 0, 1);
            // move to "floor" plane
            mat4_translate(&model_main, 0, 1. /2., 0);
            // move to final pos
            mat4_translate(&model_main, pos.x, pos.y, pos.z /* a little out from cover */ + .050f);

            // draw
            rdisc_i.render_tex(rdisc_tex, /* rdisc_texture */ +1, colo);

            if (1)
            { /* reflect on the Y, actual position matters! */
                mat4_scale(&model_main, 1, -1, 1);
                // fill blue
                sh_col = vec4 {0., 0., 1., .9 * colo.a};
                // rdisc_i
                rdisc_model_main = model_main;
               // render_tex(rdisc_tex, /* rdisc_texture */ -2, rdisc_i, sh_col);
            }
            // restore from saved
            model_main = model_main_b;
           // glUniformMatrix4fv(glGetUniformLocation(sh_prog1, "model_main"), 1, 0, model_main.data);
        }
        // log_info("000000000000");
        // reflection option
        if (use_reflection)
        { /* reflect on the Y, actual position matters! */
            mat4_scale(&model_main, 1, -1, 1);

            // draw objects
            glUseProgram(sh_prog1);
            glUniformMatrix4fv(glGetUniformLocation(sh_prog1, "model"), 1, 0, model_main.data);
            glUniform4f(glGetUniformLocation(sh_prog1, "Color"), .3, .3, .3, .1 * colo.a);
            cover_o.render(GL_TRIANGLES);

            glUseProgram(0);

            // draw coverbox reflection, if not avail craft coverbox from icon
            check_n_draw_textures(all_apps[tex_idx], -1, colo);
            if (tex_idx == inserted_disc.load())
            {
                // fill blue, (shader 1 don't use)
                vec4 sh_col = vec4 {1., 1., .8, .7 * colo.a};
                // rdisc_i
                model_main = rdisc_model_main;
                rdisc_i.render_tex(rdisc_tex, /* rdisc_texture */ -3, sh_col);
            }
        }
        /* set all_apps[g_idx].info.name */
        // log_info("000000000000");
        if (!j             // last drawn one, the selected in the center
            && !title_vbo) // we cleaned VBO
        {
            std::string tmpstr;

            //       vec2 pen = (vec2){ (float)l.bound_box.x - 20., resolution.y - 140. };
            //
            item_t &li = all_apps[tex_idx];
           // log_info("tex_idx: %i", tex_idx);
            //
            title_vbo = VertexBuffer("vertex:3f,tex_coord:2f,color:4f");
            // NAME, is size checked
            tmpstr = fmt::format("{0:.25}{1:} {2:}", li.info.name, (li.info.name.length() > 25) ? "..." : "", li.flags.is_ext_hdd ? "(Ext. HDD)" : "");
            if (v_curr == ITEM_SETTINGS)
                tmpstr = getLangSTR(LANG_STR::ITEMZ_SETTINGS);

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
                title_vbo.add_text(titl_font, tmpstr, col, pen_game_options);
            }
            else if (v_curr == ITEMzFLOW) // fill the vbo
                title_vbo.add_text(titl_font, tmpstr, col, pen);
            else if (v_curr == ITEM_SETTINGS)
                title_vbo.add_text(titl_font, tmpstr, col, setting_pen);

            // log_info("%i",__LINE__);
// log_info("000000000000");
            //
            // ID
            // we need to know Text_Length_in_px in advance, so we call this:
            texture_font_load_glyphs(sub_font, li.info.id.c_str());
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
                    //itemzcore_add_text(title_vbo,80., 115., getLangSTR(LANG_STR::BUTTON_X_INFO));
                    itemzcore_add_text(title_vbo,80., 20., getLangSTR(LANG_STR::BUTTON_X_INFO));
                    itemzcore_add_text(title_vbo,80., 65., getLangSTR(LANG_STR::BUTTON_O_INFO));
                    // if (is_trainer_page)
                    itemzcore_add_text(title_vbo,390., 100., getLangSTR(LANG_STR::BUTTON_LEFT_RIGHT_INFO));
                    itemzcore_add_text(title_vbo,370., 42., getLangSTR(LANG_STR::BUTTON_UP_DOWN_INFO));

                    itemzcore_add_text(title_vbo,inserted_disc.load() > 0 ? 115 : 40., 110., inserted_disc.load() > 0 ? getLangSTR(LANG_STR::EJECT_DISK) : getLangSTR(LANG_STR::NO_DISC_INSERTED) );
                }

                pen_game_options.x += 10.;

                if(li.flags.is_vapp)
                  tmpstr = fmt::format("{0:} ({1:}) (VAPP)", li.info.id, li.info.version);
                else
                  tmpstr = fmt::format("{0:} {2:} ({1:})", li.info.id, li.flags.is_fpkg ? "FPKG" : getLangSTR(LANG_STR::RETAIL), li.info.version);
                // fill the vbo
                title_vbo.add_text(sub_font, tmpstr, c, pen_game_options);

                fmt::print("mins_played: {}", mins_played.load());

                tmpstr = getLangSTR(LANG_STR::FETCH_STATS);

                if(mins_played.load() > 0){
                    if(mins_played.load() < 59){
                        tmpstr = fmt::format("{0:d} {1:}", mins_played.load(), getLangSTR(LANG_STR::STATS_MINS));
                    }
                    else if(mins_played < 1440){
                        tmpstr = fmt::format("{0:d} {1:} {2:d} {3:}", mins_played.load()/60, getLangSTR(LANG_STR::STATS_HOURS), mins_played.load()%60, getLangSTR(LANG_STR::STATS_MINS));
                    }
                    else if(mins_played >= 1440){
                        tmpstr = fmt::format("{0:d} {1:} {2:d} {3:} {4:d} {5:}", mins_played.load()/1440, getLangSTR(LANG_STR::STATS_DAYS), mins_played.load()%1440/60, getLangSTR(LANG_STR::STATS_HOURS), mins_played.load()%1440%60, getLangSTR(LANG_STR::STATS_MINS));
                    }
                    //fmt::print("cpp msg: {}", tmpstr);
                    mins_played = MIN_STATUS_SEEN;
                       
                }//
                else if(mins_played == MIN_STATUS_NA){
                    tmpstr = getLangSTR(LANG_STR::NO_STATS_AVAIL);
                    mins_played = MIN_STATUS_SEEN;
                }
                else if(mins_played == MIN_STATUS_VAPP){
                     tmpstr = getLangSTR(LANG_STR::VAPP_MENU);
                     mins_played = MIN_STATUS_SEEN;
                } 
                log_info("str: %s", tmpstr.c_str());
                itemzcore_add_text(title_vbo,100., 920., tmpstr);
            }
            else if (v_curr == ITEMzFLOW)
            { // fill the vbo{
                // 1692 y 61
                //add_text(title_vbo, sub_font, li.info.id, & c, &pen);
                title_vbo.add_text(sub_font, li.info.id, c, pen);
                if (get->setting_bools[Show_Buttons])
                {
                    
                    itemzcore_add_text(title_vbo,1475., 61., getLangSTR(LANG_STR::SETTINGS));
                    itemzcore_add_text(title_vbo,1435., 110., getLangSTR(LANG_STR::BUTTON_TRIANGLE));
                    if (!confirm_close)
                        itemzcore_add_text(title_vbo,1660., 61., getLangSTR(LANG_STR::BUTTON_CLOSE));

                    itemzcore_add_text(title_vbo, 1660., 110., getLangSTR(LANG_STR::CHANGE_CATEGORY));
                }

                if (confirm_close)
                    itemzcore_add_text(title_vbo,1660., 61., getLangSTR(LANG_STR::BUTTON_CONFIRM_CLOSE));

                if(get->sort_cat != NO_CATEGORY_FILTER){
                    //pen.y -= 32;
                    itemzcore_add_text(title_vbo, 1440., 175., getLangSTR(LANG_STR::RESET_CAT));
                    itemzcore_add_text(title_vbo, 780, 50, fmt::format("{0:} - {1:}", getLangSTR(LANG_STR::SHOWING_CATEGORY), category_get_str(get->sort_cat)));
                }
            }
            else if (v_curr == ITEM_SETTINGS)
            { // fill the vbo{
                std::string ud_str;
                // THIS DOESNT NOT NEED TRANSLATED BECAUSE ITS NEVER SUPPOSED TO BE USED BY USERS
                // ONLY THE DEVS OF THIS PROJECT USE THIS
                get->setting_bools[INTERNAL_UPDATE] ? ud_str = "Internal Update Mode Enabled" : ud_str = getLangSTR(LANG_STR::SETTINGS);
                //log_info( "ud_str: %s", (numb_of_settings == NUMBER_OF_SETTINGS) ? ud_str.c_str() : getLangSTR(LANG_STR::ADVANCED_SETTINGS).c_str());
                setting_pen.x += 10.;
                texture_font_load_glyphs(sub_font, (numb_of_settings == NUMBER_OF_SETTINGS) ? ud_str.c_str() : getLangSTR(LANG_STR::ADVANCED_SETTINGS).c_str());
                title_vbo.add_text(sub_font, (numb_of_settings == NUMBER_OF_SETTINGS) ? ud_str : getLangSTR(LANG_STR::ADVANCED_SETTINGS), c, setting_pen);
                if (get->setting_bools[Show_Buttons])
                {
                    // itemzcore_add_text(title_vbo,80., 115., getLangSTR(LANG_STR::BUTTON_X_INFO));
                    itemzcore_add_text(title_vbo, 80., 20., getLangSTR(LANG_STR::BUTTON_X_INFO));
                    itemzcore_add_text(title_vbo,80., 65., getLangSTR(LANG_STR::BUTTON_O_INFO));
                    itemzcore_add_text(title_vbo,390., 100., getLangSTR(LANG_STR::BUTTON_LEFT_RIGHT_INFO));
                    itemzcore_add_text(title_vbo,370., 42., getLangSTR(LANG_STR::BUTTON_UP_DOWN_INFO));
                    itemzcore_add_text(title_vbo,inserted_disc.load() > 0 ? 115 : 40., 110., inserted_disc.load() > 0 ? getLangSTR(LANG_STR::EJECT_DISK) : getLangSTR(LANG_STR::NO_DISC_INSERTED));
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
    // log_info("000000000000");
    if (!i)
    {
        j = 0;
        if (all_apps[0].count.token_c >= 1)
        {
            // log_info("********** numb %i", all_apps[0].count.token_c);
            // log_info("000000000000");
            goto draw_item;
        }
       // else
       //    log_info("no apps found");
    }
// log_info("000000000000");
    if (all_apps[0].count.token_c < 1 && !title_vbo)
    {
        title_vbo = VertexBuffer("vertex:3f,tex_coord:2f,color:4f");
        if(v_curr == ITEMzFLOW){
        itemzcore_add_text(title_vbo, 830, 150, getLangSTR(LANG_STR::NO_APPS_FOUND));
        if (get->setting_bools[Show_Buttons])
        {
            itemzcore_add_text(title_vbo, 1475., 61., getLangSTR(LANG_STR::SETTINGS));
            itemzcore_add_text(title_vbo, 1435., 110., getLangSTR(LANG_STR::BUTTON_TRIANGLE));
            if (!confirm_close)
                itemzcore_add_text(title_vbo, 1660., 61., getLangSTR(LANG_STR::BUTTON_CLOSE));

            itemzcore_add_text(title_vbo, 1660., 110., getLangSTR(LANG_STR::CHANGE_CATEGORY));
        }

        if (confirm_close)
            itemzcore_add_text(title_vbo, 1660., 61., getLangSTR(LANG_STR::BUTTON_CONFIRM_CLOSE));

        itemzcore_add_text(title_vbo, 1440., 175., getLangSTR(LANG_STR::RESET_CAT));
        itemzcore_add_text(title_vbo, 780, 50, fmt::format("{0:} - {1:}", getLangSTR(LANG_STR::SHOWING_CATEGORY), category_get_str(get->sort_cat)));
        }
        else if (v_curr == ITEM_SETTINGS)
        {

            itemzcore_add_text(title_vbo, (float)(resolution.y / 2.1), (float)(resolution.y - 190.), getLangSTR(LANG_STR::SETTINGS));
            if (get->setting_bools[Show_Buttons])
            {
                itemzcore_add_text(title_vbo, 80., 20., getLangSTR(LANG_STR::BUTTON_X_INFO));
                itemzcore_add_text(title_vbo, 80., 65., getLangSTR(LANG_STR::BUTTON_O_INFO));
                itemzcore_add_text(title_vbo, 390., 100., getLangSTR(LANG_STR::BUTTON_LEFT_RIGHT_INFO));
                itemzcore_add_text(title_vbo, 370., 42., getLangSTR(LANG_STR::BUTTON_UP_DOWN_INFO));
                itemzcore_add_text(title_vbo, 580., 25., getLangSTR(LANG_STR::ADVANCED_SETTINGS));
            }
        }
    }

all_drawn:
// log_info("000000000000");
    glDisable(GL_BLEND);
    glUseProgram(0);
    draw_additions((view_t)v_curr);

    if (old_curr != v_curr)
        GLES2_refresh_sysinfo();

    if (v_curr != ITEMzFLOW){
        GLES2_render_list((view_t)v_curr); // v2
        // log_info("v_curr: %i", v_curr);
    }
// log_info("000000000000");
    switch (v_curr) // draw more things, per view_main
    {
    case ITEMzFLOW:
        if (pr_dbg)
        {
            log_info("%d,\t%s\t%s", g_idx,
                     all_apps[g_idx].info.name.c_str(),
                     all_apps[g_idx].info.id.c_str());
        }
        break;
        // add menu
    case ITEM_PAGE:
    {
        if (old_curr != v_curr)
        {
            ls_p.vbo_s = ASK_REFRESH;
        }
        break;
    }
    case ITEM_SETTINGS:
    {
        // log_info("%i", __LINE__);
        break;
    }
    }
    // texts out of VBO
    title_vbo.render_vbo(NULL);
    old_curr = v_curr;
    pr_dbg = 0;
    // log_info("000000000000");
}

/*
    this is the main renderloop
    note: don't need to be cleared or swapped any frames !
    this one is drawing each rendered frame!
*/extern std::atomic_bool is_idle;
void Render_ItemzCore(void)
{
    bool is_file_manager = (v_curr == FILE_BROWSER_LEFT || v_curr == FILE_BROWSER_RIGHT);
    // update the
    // v_curr = FILE_BROWSER;
    on_GLES2_Update(u_t);
    GLES2_ani_update(u_t);

    if(!is_idle.load()){
      if (!is_file_manager){
           DrawScene_4();
           // log_info("000000000000");
      }
      else {
          draw_additions((view_t)v_curr);
          GLES2_render_list((view_t)v_curr);
        }
    }

    GLES2_Draw_sysinfo(is_idle.load());
    GLES2_ani_draw();
}

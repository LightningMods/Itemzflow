/*
    samples below init and draw 3 different panels
*/

#include "defines.h"
#include "GLES2_common.h"
#include <atomic>
#define FS_START_1 "/"
#if defined(__ORBIS__)
#include "shaders.h"
#define FS_START_2 "/data"
#else
#include "pc_shaders.h"
#define FS_START_2 "/home/lm"
#endif
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include "feature_classes.hpp"
#if defined(__ORBIS__)
#include "patcher.h"
#endif

GLuint ic = GL_NULL;
extern VertexBuffer title_vbo, test_v;
extern bool skipped_first_X;
    // the Settings
    extern ItemzSettings set,
    *get;

save_entry_t gm_save;
vec3 p_off = (0);
vec2 p1_pos;
GameStatus old_status;
extern std::atomic_int AppVis;
char text_str_tmp[64];
extern std::vector<std::string> ITEMZ_SETTING_TEXT;

extern vec2 resolution;
extern int numb_of_settings;
extern std::vector<std::string> gm_p_text;
char selected_text[64];
extern int DL_CO;
extern bool is_dumpable;
extern int curr_file_index;

// ------------------------------------------------------- global variables ---
static VertexBuffer rects_buffer;
static GLuint shader = GL_NULL,
mz_shader= GL_NULL,
curr_Program;  // the current one
static mat4 model_menu, view_menu, projection_menu;
//static GLuint g_TimeSlot = 0;
static vec2 resolution2;

// from main.c
extern double u_t;
extern vec2 p1_pos;  // unused yet
#if defined(__ORBIS__)
extern struct _trainer_struct trs;
#endif

using namespace multi_select_options;
void draw_rect(vec4 &color, float x, float y, float w, float h){
    vec4 r;
    vec2 p = (vec2){x, y};
    vec2 s = (vec2){w, h};
    s += p;
    r.xy = px_pos_to_normalized(&p),
    r.zw = px_pos_to_normalized(&s);
    // gles render the frect
    std::vector<vec4> v;
    v.push_back(r);
    ORBIS_RenderFillRects(USE_COLOR, color, v, 1);
}

VertexBuffer vbo_from_rect(vec4& rect)
{
    VertexBuffer vbo = VertexBuffer("vertex:3f,color:4f");
    vec4   color = { 1, 0, 0, 1 };
    float r = color.r, g = color.g, b = color.b, a = color.a;
    /* VBO is setup as: "vertex:3f, color:4f */
    vertex_t vtx[4] = { { rect.x, rect.y, 0,   r,g,b,a },
                        { rect.x, rect.w, 0,   r,g,b,a },
                        { rect.z, rect.w, 0,   r,g,b,a },
                        { rect.z, rect.y, 0,   r,g,b,a } };
    // two triangles: 2 * 3 vertex
    GLuint   idx[6] = { 0, 1, 2,    0, 2, 3 };
    vbo.push_back(vtx, 4, idx, 6);

    return vbo;
}
// io loads borders, then save UVs
vec4 get_rect_from_index(const int idx, const layout_t& l, vec4* io)
{   // output vector, in px
    vec4 r = (0);
    // item position and size, fieldsize, borders, in px
    vec2 p = (0), // l.bound_box.xy,
        s = l.bound_box.zw,
         fs = (vec2){(float)l.fieldsize.x, (float)l.fieldsize.y},
         borders = (0);
    // override, load vector
    if (io) borders = io->xy;
    // 1st is up, then decrease and draw going down
    p.y += s.y;
    // size minus total borders
    s -= borders * (fs - 1.);
    s /= fs;
    // compute position
    p.x += (idx % (int)fs.x) * (s.x + borders.x),
        p.y -= floor(idx / fs.x) * (s.y + borders.y);
    // fill output vector: pos, size
    r.xy = p, r.zw = s;
    // we draw upperL-bottomR, flip size.h sign
    r.w *= -1.;

    if (io)
    {   // convert to normalized coordinates
        io->xy = r.xy / resolution,
            io->zw = (r.zw + r.xy) / resolution;
    }

    return r; // in px size!
}

void GLES2_layout_init(int req_item_count, layout_t& l)
{
    l.page_sel.x = 0;
    // dynalloc for max requested number of items
    l.item_d.clear();
    l.item_c = req_item_count;
    l.item_d.resize(l.item_c);
    l.is_active = true;
}

/* also, different panel have a different selector */
static void GLES2_render_selector(view_t v, vec4 &rect)
{
    std::vector<vec4> r;
    r.push_back(rect);
    if (v == ITEMzFLOW || v == ITEM_PAGE || v == ITEM_SETTINGS)
    {   // gles render the selected c
        ORBIS_RenderFillRects(USE_COLOR, grey, r, 1);
        // gles render the glowing blue box
        ORBIS_RenderDrawBox(USE_UTIME, sele, rect);
    }

}

// multi-select options
using namespace multi_select_options;

std::string Sort_get_str(Sort_Multi_Sel num) {
 
    sprintf(&text_str_tmp[0], "OUT_OF_INDEX_ERROR (%i)", num);

    switch (num) {
    case multi_select_options::NA_SORT:
        return getLangSTR(LANG_STR::NA_SORT1);
    case multi_select_options::TID_ALPHA_SORT:
        return getLangSTR(LANG_STR::TID_ALPHA_SORT1);
    case multi_select_options::TITLE_ALPHA_SORT:
        return getLangSTR(LANG_STR::TITLE_ALPHA_SORT1);
    default:
        return &text_str_tmp[0];
    }
}
std::string orbis_menu_get_str(ShellUI_Multi_Sel num) 
{
    switch (num) {
    case SHELLUI_SETTINGS_MENU:
        return getLangSTR(LANG_STR::SETTINGS);
    case SHELLUI_POWER_SAVE:
        return getLangSTR(LANG_STR::PSV_MENU);
    default:
        return "OUT_OF_INDEX_ERROR";
    }
}
std::string power_menu_get_str(Power_control_sel num)
{
    switch (num) {
    case POWER_OFF_OPT:
        return getLangSTR(LANG_STR::SHUTDOWN_STR);
    case RESTART_OPT:
        return getLangSTR(LANG_STR::RESTART_STR);
    case SUSPEND_OPT:
        return getLangSTR(LANG_STR::SUSPEND_STR);
    default:
        return "OUT_OF_INDEX_ERROR";
    }
}

std::string uninstall_get_str(Uninstall_Multi_Sel num) {

    switch (num) {
    case UNINSTALL_GAME:
        return getLangSTR(LANG_STR::UNINSTALL_GAME1);
    case UNINSTALL_PATCH:
        return getLangSTR(LANG_STR::UNINSTALL_PATCH1);
    case UNINSTALL_DLC:
        return getLangSTR(LANG_STR::UNINSTALL_DLC1);
    default:
        return "OUT_OF_INDEX_ERROR";
    }
}

std::string save_game_get_str(SAVE_Multi_Sel num) {

    if(gm_save.numb_of_saves > gm_save.ui_requested_save)
       gm_save.ui_requested_save++;
    else
        gm_save.ui_requested_save = 0;
        
    switch (num) {
    case BACKUP_GAME_SAVE:
        return getLangSTR(LANG_STR::BACKUP_SAVE);
    case RESTORE_GAME_SAVE:
        return getLangSTR(LANG_STR::IMPORT_SAVE);
    case DELETE_GAME_SAVE:
        return getLangSTR(LANG_STR::SAVE_DELETE);
    default:
        return "OUT_OF_INDEX_ERROR";
    }
}

std::string dumper_opt_get_str(Dump_Multi_Sels num) {
    //0 is DUMP ALL
    switch (num) {
    case SEL_DUMP_ALL:
        return getLangSTR(LANG_STR::SEL_DUMP_ALL1);
    case SEL_DUMP_VALID:
        return getLangSTR(LANG_STR::SEL_DUMP_VALID1);
    case SEL_BASE_GAME:
        return getLangSTR(LANG_STR::SEL_BASE_GAME1);
    case SEL_GAME_PATCH:
        return getLangSTR(LANG_STR::SEL_GAME_PATCH1);
    case SEL_REMASTER:
        return getLangSTR(LANG_STR::SEL_REMASTER1);
    case SEL_DLC:
        return getLangSTR(LANG_STR::SEL_DLC1);
    case SEL_FPKG:
        return getLangSTR(LANG_STR::BACKUP_FPKG);
    default:
        return "OUT_OF_INDEX_ERROR";
    }

}

std::string restore_app_get_str(RESTORE_OPTIONS num)
{
    switch (num)
    {
    case RESTORE_APP:
        return getLangSTR(LANG_STR::RESTORE_APP_OPT_1);
    case RESTORE_DLC:
        return getLangSTR(LANG_STR::RESTORE_APP_OPT_2);
    case RESTORE_PATCH:
        return getLangSTR(LANG_STR::RESTORE_APP_OPT_3);
    default:
        return "OUT_OF_INDEX_ERROR";
    }
}

std::string move_app_get_str(MOVE_OPTIONS num)
{
    switch (num)
    {
    case MOVE_APP:
        return getLangSTR(LANG_STR::MOVE_APP_OPT_1);
    case MOVE_DLC:
        return getLangSTR(LANG_STR::MOVE_APP_OPT_2);
    case MOVE_PATCH:
        return getLangSTR(LANG_STR::MOVE_APP_OPT_3);
    default:
        return "OUT_OF_INDEX_ERROR";
    }
}

std::string control_app_get_str(GameStatus num)
{
    switch (num)
    {
    case OTHER_APP_OPEN:
        return getLangSTR(LANG_STR::CLOSE_GAME);
    case RESUMABLE:
        return getLangSTR(LANG_STR::GAME_RESUME);
    case NO_APP_OPENED:
        return getLangSTR(LANG_STR::LAUNCH_GAME);
    default:
        return "OUT_OF_INDEX_ERROR";
    }
}

std::string category_get_str(Sort_Category num)
{
    switch (num)
    {
    case FILTER_FAVORITES:
        return getLangSTR(LANG_STR::FAVORITES);
    case FILTER_RETAIL_GAMES:
        return getLangSTR(LANG_STR::RETAIL_GAMES);
    case FILTER_FPKG:
        return  getLangSTR(LANG_STR::FPKG_STR);
    case FILTER_GAMES:
        return getLangSTR(LANG_STR::GAMES);
    case FILTER_HOMEBREW:
        return getLangSTR(LANG_STR::HOMEBREW);
    default:
        return getLangSTR(LANG_STR::NONE);
    }
}
// REBUILD_DB_OPT_1=Rebuild Internal Database
// REBUILD_DB_OPT_2=Rebuild DLC (Inc. ext hdd)
std::string rebuild_db_get_str(Rebuild_db_Multi_Sel num)
{
    switch (num)
    {
    case REBUILD_ALL:
        return getLangSTR(LANG_STR::REBUILD_DB_OPT_1);
    case REBUILD_DLC:
        return getLangSTR(LANG_STR::REBUILD_DB_OPT_2);
    case REBUILD_REACT:
        return getLangSTR(LANG_STR::REBUILD_DB_OPT_3);
    default:
        return "OUT_OF_INDEX_ERROR";
    }
}
//CATEGORY=Category
//SHOWING_CATEGORY=Showing Category
//RETAIL_GAMES=Retail Games
//GAMES=Games
//HOMEBREW=Homebrew
//FPKG=FPKGs
//NONE=None
//NO_TRAINERS=No Trainers Found
//
std::string trainer_get_str(int num)
{
#if defined(__ORBIS__)
    if(trs.controls_text.empty())
        return "No Trainers Found";

    return fmt::format("{0:}", trs.controls_text.at(num));
#else
    return std::string();
#endif
}

static void print_option_info(layout_t& l, vec2 &pen, std::string line_1, std::string line_2, bool is_settings) {

    vec4 c = col * .75f;
    vec2 info_text = pen;

    if (is_settings) {
        info_text.x = 520.;
        info_text.y = 180.;
    }
    else {
        info_text.x = 90.;
        info_text.y = 200.;
    }

    if (!line_1.empty())
         l.vbo.add_text( sub_font, fmt::format("{0:.80}", line_1), c, info_text);
    
    if (!line_2.empty())
    {
        if (is_settings) {
            info_text.x = 520.;
            info_text.y = 150.;
        }
        else {
            info_text.x = 90.;
            info_text.y = 170.;
        }
        l.vbo.add_text(sub_font, fmt::format("{0:.80}", line_2), c, info_text);
    }
}
/* default way to deal with text, for layout_t */
static void layout_compose_text(view_t vw, int idx, vec2 &pen, bool save_text)
{
#if 0
    log_info("%s %p, %d", __FUNCTION__, l, save_text);
#endif
    layout_t &l = getLayoutbyView(vw);
    GameStatus app_state = app_status.load();

    std::string tmp = "";

    vec4 c = col * .75f;
    // default indexing, item_t* AOS
    item_t &li = l.item_d[idx];

    if (vw == ITEM_PAGE)//|| l == setting_p)
    {
        vec2 bk = pen;
        bk.x += 10,
            bk.y += 50;
        vec2 sub_ops = { bk.x, bk.y };

        l.vbo.add_text( sub_font, li.info.name, col, bk);

        /* special option */
        vec2 tp = pen;
        //vec4 rr = {300., 300., 100., 100.};

        switch (idx)
        {
        case cf_ops::REG_OPTS::LAUNCH_GAME:
        if(app_state != RESUMABLE) break;
        if (!li.multi_sel.is_active){
            li.multi_sel.pos = (ivec4){0, 0, 0, 2};
            li.multi_sel.is_active = true;
            log_info("$$$$$  multi_sel is active");
        }
        case cf_ops::REG_OPTS::RESTORE_APPS: // 3
        case cf_ops::REG_OPTS::MOVE_APPS:    // 3
        case cf_ops::REG_OPTS::GAME_SAVE: // 3
            if (!li.multi_sel.is_active)
            {
                li.multi_sel.pos = (ivec4){0, 0, 0, 3};
                li.multi_sel.is_active = true;
            }
        case cf_ops::REG_OPTS::TRAINERS:
#if defined(__ORBIS__)
           if (!li.multi_sel.is_active)
           {
               li.multi_sel.pos = (ivec4){0, 0, 0, trs.controls_text.size()};
               li.multi_sel.is_active = true;
           }
#else
            if (!li.multi_sel.is_active)
            {
                li.multi_sel.pos = (ivec4){0, 0, 0, 0};
                li.multi_sel.is_active = true;
            }
#endif
        case cf_ops::REG_OPTS::UNINSTALL:
            if (!li.multi_sel.is_active)
            {
                li.multi_sel.pos = (ivec4){0, 0, 0, 2};
                li.multi_sel.is_active = true;
            }
        case cf_ops::REG_OPTS::DUMP_GAME: {
            if (!li.multi_sel.is_active)
            {
                li.multi_sel.pos = (ivec4){0, 0, 0, 6};
                li.multi_sel.is_active = true;
            }
            //set active for all above too
            //li.multi_sel.is_active = true;
            if (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE)
            {
                if (idx == cf_ops::REG_OPTS::DUMP_GAME){
                    if (all_apps[g_idx].flags.is_fpkg){
                       if(!all_apps[g_idx].flags.is_dumpable){
                          li.multi_sel.pos.w = 1;
                          get->Dump_opt = SEL_FPKG;
                          tmp = dumper_opt_get_str(SEL_FPKG);
                          break;
                        }
                        else
                           li.multi_sel.pos.w = 7;
                    }
                    else {
                        if(!all_apps[g_idx].flags.is_fpkg){
                           li.multi_sel.pos.w = 6;
                           if (li.multi_sel.pos.y == SEL_FPKG)
                               li.multi_sel.pos.y--;
                        }
                        else{
                            li.multi_sel.pos.y = SEL_RESET;
                            tmp = getLangSTR(LANG_STR::OPT_NOT_PERMITTED);
                            break;
                        }
                    } 
                }
                // show '< X >' around Option
                switch (idx){
                    case cf_ops::REG_OPTS::DUMP_GAME:{
                        tmp = fmt::format("< {0:.20} >", dumper_opt_get_str((multi_select_options::Dump_Multi_Sels)li.multi_sel.pos.y));
                        get->Dump_opt = (Dump_Multi_Sels)li.multi_sel.pos.y;
                        break;
                    }
                    case cf_ops::REG_OPTS::UNINSTALL:{
                        tmp = fmt::format("< {0:.20} >", uninstall_get_str((multi_select_options::Uninstall_Multi_Sel)li.multi_sel.pos.y));
                        get->un_opt = (multi_select_options::Uninstall_Multi_Sel)li.multi_sel.pos.y;
                        break;
                    }
                    case cf_ops::REG_OPTS::GAME_SAVE:{
#if defined(__ORBIS__)
      if(!gm_save.is_loaded || gm_save.ui_requested_save != gm_save.ui_current_save){
        if(!(gm_save.is_loaded  = GameSave_Info(&gm_save, (gm_save.ui_requested_save == -1) ? 0 : gm_save.ui_requested_save))){ 
               log_error("Fetching Save Info failed");
               tmp =  asset_path("no_save.png");
               gm_save.numb_of_saves = 0;
        }
        else{
              tmp = fmt::format("/user/home/{0:x}/savedata_meta/user/{1:}/{2:}_icon0.png", gm_save.userid, gm_save.title_id, gm_save.dir_name);
              if(!if_exists(tmp.c_str()))
                tmp = asset_path("no_save.png");
        }
        gm_save.icon_path =  tmp;

        if(ic > GL_NULL){
           glDeleteTextures(1, &ic), ic = GL_NULL;
        } // load icon
        ic = load_png_asset_into_texture(gm_save.icon_path.c_str());
        gm_save.ui_current_save = gm_save.ui_requested_save;
        fmt::print("SaveData Icon: {0:} && gm_save.is_loaded: {1:}", gm_save.icon_path, gm_save.is_loaded);
      }
#else
      gm_save.is_loaded = false;
#endif
                        vec4 color = (vec4){.8164, .8164, .8125, 1.};
                        vec2 pen;
                        pen.x = 800., pen.y = 750.;
                        tmp = fmt::format("{0:} {1:}", gm_save.numb_of_saves, (gm_save.numb_of_saves > 1) ? getLangSTR(LANG_STR::FOUND_SAVE_MESSAGE_OVER_1) : getLangSTR(LANG_STR::FOUND_SAVE_MESSAGE) );
                        l.vbo.add_text( sub_font, tmp, color, pen);

                        pen.y = 710., pen.x = 1040.;
                        l.vbo.add_text( main_font, all_apps[g_idx].info.name, color, pen);
                        // ONLY CONTINUE IF SAVE IS LOADED
                        if(!gm_save.is_loaded){
                           pen.y = 680., pen.x = 1040.;
                           tmp = fmt::format("{0:}: {1:#x}", getLangSTR(LANG_STR::USER_LANG), gm_save.userid);
                           l.vbo.add_text( main_font, tmp, color, pen);
                           tmp = fmt::format(" < {} >", getLangSTR(LANG_STR::IMPORT_SAVE));
                           li.multi_sel.pos.y = RESTORE_GAME_SAVE;
                           break;
                        }

                        pen.y = 680., pen.x = 1040.;
                        l.vbo.add_text( main_font, gm_save.mtime, color, pen);
                        pen.y = 650., pen.x = 1040.;
                        tmp = fmt::format("{0:}: {1:#x}", getLangSTR(LANG_STR::USER_LANG), gm_save.userid);
                        l.vbo.add_text( main_font, tmp, color, pen);
                        pen.y = 620., pen.x = 1040.; //KB to Bytes to String
                        tmp = fmt::format("{} | {} Blocks", calculateSize(gm_save.size * 1000), gm_save.blocks);
                        l.vbo.add_text( main_font, tmp, color, pen);
                        pen.y = 570., pen.x = 790.;
                        l.vbo.add_text( main_font, fmt::format("{0:.45}", gm_save.detail), color, pen);
                        pen.y = 595., pen.x = 1040.;
                        l.vbo.add_text( main_font, fmt::format("{0:.45}", gm_save.main_title), color, pen);
                        pen.y = 500., pen.x = 1265.;
                        l.vbo.add_text( main_font, fmt::format("{}/{}", gm_save.ui_requested_save, gm_save.numb_of_saves), color, pen);

                        tmp = fmt::format("< {0:.20} >", save_game_get_str((multi_select_options::SAVE_Multi_Sel)li.multi_sel.pos.y));
                        break;
                    }//move_app_get_str((MOVE_OPTIONS)li.multi_sel.pos.y)
                    case cf_ops::REG_OPTS::RESTORE_APPS:{
                        tmp = fmt::format("< {0:.20} >", restore_app_get_str((multi_select_options::RESTORE_OPTIONS)li.multi_sel.pos.y));
                        break;
                    }
                    case cf_ops::REG_OPTS::MOVE_APPS: {
                        tmp = fmt::format("< {0:.20} >", move_app_get_str((multi_select_options::MOVE_OPTIONS)li.multi_sel.pos.y));
                        break;
                    }
                    case cf_ops::REG_OPTS::LAUNCH_GAME:{
                        tmp = fmt::format("< {0:.20} >", control_app_get_str((GameStatus)(li.multi_sel.pos.y+1)));
                        break;
                    }
                    case cf_ops::REG_OPTS::TRAINERS:
                    {
#if defined(__ORBIS__)
                        patch_current_index = li.multi_sel.pos.y;
                        li.multi_sel.pos.w = trs.controls_text.size();
                        vec4 color = (vec4){.8164, .8164, .8125, 1.};
                        vec2 pen;
                        pen.x = 800., pen.y = 750.;
                        l.vbo.add_text( sub_font, fmt::format("{0}", trs.patcher_title.at(patch_current_index)), color, pen);
                        pen.y = 710., pen.x = 790.;
                        l.vbo.add_text( main_font, fmt::format("{1} {0}", trs.patcher_name.at(patch_current_index), getLangSTR(LANG_STR::PATCH_NAME)), color, pen);
                        pen.y = 680., pen.x = 790.;
                        l.vbo.add_text( main_font, fmt::format("{1} {0}", trs.patcher_app_ver.at(patch_current_index), getLangSTR(LANG_STR::PATCH_APP_VER)), color, pen);
                        pen.y = 650., pen.x = 790.;
                        l.vbo.add_text( main_font, fmt::format("{1} {0}", trs.patcher_author.at(patch_current_index), getLangSTR(LANG_STR::PATCH_AUTHOR)), color, pen);
                        pen.y = 620., pen.x = 790.;
                        l.vbo.add_text( main_font, fmt::format("{1} {0:.40}", trs.patcher_note.at(patch_current_index), getLangSTR(LANG_STR::PATCH_NOTE)), color, pen);
                        pen.y = 590., pen.x = 790.;
                        l.vbo.add_text( main_font, fmt::format("{1} {0}", trs.patcher_patch_ver.at(patch_current_index), getLangSTR(LANG_STR::PATCH_VER)), color, pen);
                        pen.y = 560., pen.x = 790.;
                        l.vbo.add_text( main_font, fmt::format("{1} {0}", trs.patcher_app_elf.at(patch_current_index), getLangSTR(LANG_STR::PATCH_APP_ELF)), color, pen);
                        pen.y = 530., pen.x = 790.;
                        l.vbo.add_text( main_font, fmt::format("{1} {0}", trs.patcher_enablement.at(patch_current_index), getLangSTR(LANG_STR::PATCH_ENABLED)), color, pen);
                        log_debug("index: %i size: %i state %i ", patch_current_index, trs.controls_text.size(), trs.patcher_enablement.at(patch_current_index));
                        tmp = fmt::format("< {0:.20} / {1} >", trainer_get_str(patch_current_index), trs.controls_text.size());
                        break;
#endif
                    }
                }
                // compute rectangle position and size in px
                vec4 r = get_rect_from_index(idx, l, NULL);
                // start at panel position, move to center
                tp = pen;
                tp.x = (l.bound_box.x + r.z / 2.) + 300.;
                // we need to know Text_Length_in_px in advance, so we call this:
                texture_font_load_glyphs(main_font, tmp.c_str());
                tp.x -= tl / 2.;
                pen = tp;
            }
            else
                tmp = getLangSTR(LANG_STR::SELECT_INTERACT);

            break;
        }
        case cf_ops::REG_OPTS::CHANGE_ICON: { tmp = fmt::format("{0:.20}{1:}", all_apps[g_idx].info.picpath, (all_apps[g_idx].info.picpath.size() > 20) ? "..." : ""); break; }
#if defined(__ORBIS__)
        case cf_ops::REG_OPTS::HIDE_APP: { tmp =  (AppVis ?  getLangSTR(LANG_STR::HIDE_APP) : getLangSTR(LANG_STR::SHOW_APP)); break;  }
#endif
        case cf_ops::REG_OPTS::CHANGE_NAME: { tmp = fmt::format("{0:.20}{1:}", all_apps[g_idx].info.name, (all_apps[g_idx].info.name.size() > 20) ? "..." : ""); break; }
        }

            if (tmp.size() > 3)
            {
                sub_ops.y -= 40;
                l.vbo.add_text( main_font, tmp, c, sub_ops);
            }

            if (idx == 0 || (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE))
            {
                // print options info
                switch (l.curr_item)
                {//RESTORE_NOTICE
                case cf_ops::REG_OPTS::CHANGE_NAME:
                    print_option_info(l, pen, getLangSTR(LANG_STR::NAME_INFO), getLangSTR(LANG_STR::IF_NOT_EFFECTED), false);
                    break;
                case cf_ops::REG_OPTS::MOVE_APPS:
                    print_option_info(l, pen, getLangSTR(LANG_STR::MOVE_INFO_TEXT), getLangSTR(LANG_STR::IF_NOT_EFFECTED), false);
                    break;
                case cf_ops::REG_OPTS::RESTORE_APPS:
                    print_option_info(l, pen, getLangSTR(LANG_STR::RESTORE_INFO_TEXT), getLangSTR(LANG_STR::RESTORE_NOTICE), false);
                    break;
                case cf_ops::REG_OPTS::CHANGE_ICON:
                    print_option_info(l, pen, getLangSTR(LANG_STR::ICON_INFO), getLangSTR(LANG_STR::PNG_NOTE), false);
                    break;
                case cf_ops::REG_OPTS::LAUNCH_GAME:
                    if (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE)
                    {
                        // show '< X >' around Option
                        if (app_state == RESUMABLE)
                            print_option_info(l, pen, getLangSTR(LANG_STR::GAME_CONTROL_INFO), getLangSTR(LANG_STR::SUSPEND_IF), false);
                        else
                            print_option_info(l, pen, getLangSTR(LANG_STR::CLOSE_OPEN_APP_INFO), "", false);
                    }
                    else if (app_state == NO_APP_OPENED)
                        print_option_info(l, pen, getLangSTR(LANG_STR::LAUNCH_INFO), getLangSTR(LANG_STR::SUSPEND_IF), false);
                    else if (app_state == OTHER_APP_OPEN)
                        print_option_info(l, pen, getLangSTR(LANG_STR::CLOSE_OPEN_APP_INFO), "", false);
                    else if (app_state == RESUMABLE)
                        print_option_info(l, pen, getLangSTR(LANG_STR::GAME_CONTROL_INFO), getLangSTR(LANG_STR::SUSPEND_IF), false);

                     break;
                case cf_ops::REG_OPTS::TRAINERS:
                    print_option_info(l, pen, getLangSTR(LANG_STR::TRAINER_INFO), "", false);
                    break;
                case cf_ops::REG_OPTS::HIDE_APP:
                    print_option_info(l, pen, getLangSTR(LANG_STR::HIDE_OPT_INFO), getLangSTR(LANG_STR::HIDE_INFO), false);
                    break;
                case cf_ops::REG_OPTS::DUMP_GAME:
                {
                    print_option_info(l, pen, getLangSTR(LANG_STR::DUMP_OPT_INFO), "", false);
                    if (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE)
                    {
                        // show '< X >' around Option
                        switch (li.multi_sel.pos.y)
                        {
                        case SEL_DUMP_ALL:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::SEL_DUMP_ALL_INFO), false);
                            break;
                        case SEL_BASE_GAME:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::SEL_BASE_GAME_INFO), false);
                            break;
                        case SEL_GAME_PATCH:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::SEL_GAME_PATCH_INFO), false);
                            break;
                        case SEL_REMASTER:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::SEL_REMASTER_INFO), false);
                            break;
                        case SEL_DLC:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::SEL_DLC_INFO), false);
                            break;
                        case SEL_FPKG:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::BACKUP_FPKG_INFO), false);
                            break;
                        default:
                            break;
                        }
                    }

                    break;
                }

                case cf_ops::REG_OPTS::UNINSTALL:
                {
                    print_option_info(l, pen, getLangSTR(LANG_STR::UNINSTALL_NOTE), "", false);
                    if (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE)
                    {
                        // show '< X >' around Option
                        switch (li.multi_sel.pos.y)
                        {
                        case UNINSTALL_GAME:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::UNINSTALL_GAME_INFO), false);
                            break;
                        case UNINSTALL_PATCH:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::UNINSTALL_PATCH_INFO), false);
                            break;
                        default:
                            break;
                        }
                    }
                    break;
                }
                case cf_ops::REG_OPTS::GAME_SAVE:{
                    print_option_info(l, pen, getLangSTR(LANG_STR::GAME_SAVE), "", false);
                    if (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE)
                    {
                        // show '< X >' around Option
                        switch (li.multi_sel.pos.y)
                        {
                        case multi_select_options::BACKUP_GAME_SAVE:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::BACKUP_GAME_INFO), false);
                            break;
                        case multi_select_options::RESTORE_GAME_SAVE:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::RESTORE_GAME_INFO), false);
                            break;
                        case multi_select_options::DELETE_GAME_SAVE:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::DELETE_GAME_INFO), false);
                            break;
                        default:
                            break;
                        }
                    }

                    break;

                }

                default:
                    break;
                }
            }
            return;
    }
    else if (vw == ITEM_SETTINGS) {
        // append to vbo
        vec2 tp = pen;
        tp.y += 40.;
        l.vbo.add_text( sub_font, li.info.name, col, tp);

        /*
           DUMPER_PATH_OPTION,
    REBUILD_DB_OPTION,
    HOME_MENU_OPTION,
    SORT_BY_OPTION,
    FTP_SERVICE_OPTION,
    FTP_ON_STARTUP_OPTION,
    BACKGROUND_MP3_OPTION,
    SAVE_ITEMZFLOW_SETTINGS
    */
    //snprintf(&tmp[0], 63, format, get->HomeMenu_Redirection ? getLangSTR(LANG_STR::ON2) : getLangSTR(LANG_STR::OFF2)); break;
        switch (idx)
        {
        case OPEN_SHELLUI_MENU:
           if (!li.multi_sel.is_active)
           {
               li.multi_sel.pos = (ivec4){0, 0, 0, 2};
               li.multi_sel.is_active = true;
           }
        case REBUILD_DB_OPTION:
        case POWER_CONTROL_OPTION:
        case SORT_BY_OPTION:
        {
            /* special option */
            if (!li.multi_sel.is_active)
            {
                li.multi_sel.pos = (ivec4){0, 0, 0, 3};
                li.multi_sel.is_active = true;
            }

            if (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE)
            {
                // show '< X >' around Option and set sort by opt
                switch (idx)
                {
                case POWER_CONTROL_OPTION:
                    tmp = fmt::format("< {0:.35} >", power_menu_get_str((Power_control_sel)li.multi_sel.pos.y));
                    break;
                case SORT_BY_OPTION: tmp =  fmt::format("< {0:.35} >",Sort_get_str((Sort_Multi_Sel)li.multi_sel.pos.y));  break;
                case OPEN_SHELLUI_MENU:
                    tmp = fmt::format("< {0:.35} >", orbis_menu_get_str((ShellUI_Multi_Sel)li.multi_sel.pos.y));
                    break;
                case REBUILD_DB_OPTION:
                    tmp = fmt::format("< {0:.35} >", rebuild_db_get_str((Rebuild_db_Multi_Sel)li.multi_sel.pos.y));
                    break;
                default:
                    break;
                }

                // we need to know Text_Length_in_px in advance, so we call this:
                texture_font_load_glyphs(main_font, tmp.c_str());
                // compute rectangle position and size in px
                tp = pen;
                vec4 r = get_rect_from_index(idx, l, NULL);
                log_info("idx %i, r: %f %f %f %f", idx, r.x, r.y, r.z, r.w);
                if (numb_of_settings <= NUMBER_OF_SETTINGS)
                {
                    // start at panel position, move to center
                    tp.x = l.bound_box.x + r.z / 2.;
                    tp.x -= tl / 2.;
                }
                else {// workaround for advanced settings
                  if(idx % 3 == 0) // last column
                     tp.x = (l.bound_box.x + l.bound_box.x + r.z - 20.);
                   else if(idx % 3 == 1){
                       tp.x = l.bound_box.x + r.z + 20.;
                       //tp.x -= tl;
                   }
                }
                
                pen = tp;
            }
            else{
                switch (idx)
                {
                case SORT_BY_OPTION: tmp = Sort_get_str((Sort_Multi_Sel)get->sort_by);  break;
                case REBUILD_DB_OPTION:
                case OPEN_SHELLUI_MENU:
                default:
                    tmp = getLangSTR(LANG_STR::SELECT_INTERACT);
                    break;
                }
            }

            break;
        }
        case DUMPER_PATH_OPTION:
            tmp = fmt::format("{0:.34}{1:}", get->setting_strings[DUMPER_PATH], (get->setting_strings[DUMPER_PATH].size() > 34) ? "..." : "");
            break;
        case COVER_MESSAGE_OPTION:
            get->setting_bools[cover_message] ? tmp = "ON" : tmp = "OFF";
            break;
        case FUSE_IP_OPTION:
           // use_reflection ? tmp = "ON" : tmp = "OFF";
            tmp = get->setting_strings[FUSE_PC_NFS_IP];
            break;
        case DOWNLOAD_COVERS_OPTION:
            tmp = getLangSTR(LANG_STR::X_DL_COVER);
            break;
        case RESET_THEME_OPTION:
        case CHANGE_BACKGROUND_OPTION:
        case THEME_OPTION:
        case CHECK_FOR_UPDATE_OPTION:
        case IF_PKG_INSTALLER_OPTION:
            tmp = getLangSTR(LANG_STR::SELECT_INTERACT);
            break;
        case HOME_MENU_OPTION: tmp = (get->setting_bools[HomeMenu_Redirection] ? getLangSTR(LANG_STR::ON2) : getLangSTR(LANG_STR::OFF2)); break;
        case FONT_OPTION:
            if(numb_of_settings > NUMBER_OF_SETTINGS)
               tmp = fmt::format("{0:.34}{1:}", get->setting_strings[FNT_PATH], (get->setting_strings[FNT_PATH].size() > 34) ? "..." : "");
            break;
        case SHOW_BUTTON_OPTION: tmp = (get->setting_bools[Show_Buttons] ? getLangSTR(LANG_STR::ON2) : getLangSTR(LANG_STR::OFF2));
            break;
        case BACKGROUND_MP3_OPTION: tmp = fmt::format("{0:.34}{1:}", get->setting_strings[MP3_PATH], (get->setting_strings[MP3_PATH].size() > 34) ? "..." : ""); break;
        default: break;
        }
             // append to vbo
            l.vbo.add_text( main_font, tmp, c, pen);
            if (idx == 0 || (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE))
            {
                // print options info
                switch (l.curr_item)
                {
            
                case REBUILD_DB_OPTION:
                    print_option_info(l, pen, getLangSTR(LANG_STR::REBUILD_DB_OPTION_DESC), "", true);
                    if (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE)
                    {
                        // show '< X >' around Option
                        switch (li.multi_sel.pos.y)
                        {
                        case REBUILD_DLC:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::REBUILD_DB_DESC_2), true);
                            break;
                        case REBUILD_REACT:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::REBUILD_DB_DESC_3), true);
                            break;
                        case REBUILD_ALL:
                        default:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::REBUILD_DB_DESC_1), true);
                            break;
                        }
                    }
                    break;
                case FONT_OPTION:
                    if (numb_of_settings > NUMBER_OF_SETTINGS)
                        print_option_info(l, pen, getLangSTR(LANG_STR::TTF_OPTION_DESC), "", true);
                    break;
                case HOME_MENU_OPTION:
                    print_option_info(l, pen, getLangSTR(LANG_STR::HOME_MENU_OPTION_DESC), "", true);
                    break;
                case DUMPER_PATH_OPTION:
                    print_option_info(l, pen, getLangSTR(LANG_STR::DUMPER_PATH_DESC), "", true);
                    break;
                case CHECK_FOR_UPDATE_OPTION:
                    print_option_info(l, pen, getLangSTR(LANG_STR::CHECK_UPDATE_INFO), "", true);
                    break;
                case SORT_BY_OPTION:
                    print_option_info(l, pen, getLangSTR(LANG_STR::SORT_BY_OPTION_DESC), "", true);
                    if (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE)
                    {
                        // show '< X >' around Option
                        switch (li.multi_sel.pos.y)
                        {
                        case SORT_TID:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::SORT_ALPHA_TID_INFO), true);
                            break;
                        case SORT_ALPHABET:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::SORT_ALPHA_NAME_INFO), true);
                            break;
                        default:
                            print_option_info(l, pen, "", getLangSTR(LANG_STR::SORT_RANDOM_INFO), true);
                            break;
                        }
                    }
                    break;
                case THEME_OPTION:
                    print_option_info(l, pen, getLangSTR(LANG_STR::THEME_INSTALLER_INFO), "", true);
                    break;
                case SHOW_BUTTON_OPTION:
                    print_option_info(l, pen, getLangSTR(LANG_STR::SHOW_BUTTON_OPTION_DESC), "", true);
                    break;
                case OPEN_SHELLUI_MENU:
                    print_option_info(l, pen, getLangSTR(LANG_STR::OPEN_DEBUG_MENU_DESC), "", true);
                    break;
                case BACKGROUND_MP3_OPTION:
                    print_option_info(l, pen, getLangSTR(LANG_STR::BACKGROUND_MP3_OPTION_DESC), "", true);
                    break;
                case IF_PKG_INSTALLER_OPTION:
                    print_option_info(l, pen, getLangSTR(LANG_STR::IF_PKG_INSTALLER_INFO), "", true);
                    break;
                case POWER_CONTROL_OPTION:
                    print_option_info(l, pen, getLangSTR(LANG_STR::POWER_CONTROL_INFO), "", true);
                    break;
                case FUSE_IP_OPTION:
                     print_option_info(l, pen, getLangSTR(LANG_STR::FUSE_OPTION_DESC), getLangSTR(LANG_STR::FUSE_OPTION_DESC_1), true);
                     break;
                default:
                    break;
                }
            }
    }
    else
        l.vbo.add_text( sub_font, tmp, col, pen);
}
/* default way to deal with text, for layout_t */
static void hostapp_vapp_compose_text(view_t vw, int idx, vec2& pen, bool save_text)
{
#if 0
    log_info("%s %p, %d", __FUNCTION__, l, save_text);
#endif
    layout_t &l = getLayoutbyView(vw);
    std::string tmp = "";
    GameStatus app_state = app_status.load();

    vec4 c = col * .75f;
    // default indexing, item_t* AOS
    item_t &li = l.item_d[idx];

    if (vw != ITEM_PAGE) return;

        vec2 bk = pen;
        bk.x += 10,
            bk.y += 50;
        vec2 sub_ops = { bk.x, bk.y };

        l.vbo.add_text( sub_font, li.info.name, col, bk);

        /* special option */
        vec2 tp = pen;
        switch (idx)
        {
        case cf_ops::HOSTAPP_OPTS::LAUNCH_HA:
        if(app_state != RESUMABLE) break;
        if (!li.multi_sel.is_active){
            li.multi_sel.pos = (ivec4){0, 0, 0, 2};
            li.multi_sel.is_active = true;
            log_info("$$$$$  multi_sel is active");
        }

            //set active for all above too
            //li.multi_sel.is_active = true;
            if (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE)
            {
                // show '< X >' around Option
                switch (idx){
                    case cf_ops::HOSTAPP_OPTS::LAUNCH_HA:{
                        tmp = fmt::format("< {0:.20} >", control_app_get_str((GameStatus)(li.multi_sel.pos.y+1)));
                        break;
                    }
                }
                // compute rectangle position and size in px
                vec4 r = get_rect_from_index(idx, l, NULL);
                // start at panel position, move to center
                tp = pen;
                tp.x = (l.bound_box.x + r.z / 2.) + 300.;
                // we need to know Text_Length_in_px in advance, so we call this:
                texture_font_load_glyphs(main_font, tmp.c_str());
                tp.x -= tl / 2.;
                pen = tp;
            }
            else
                tmp = getLangSTR(LANG_STR::SELECT_INTERACT);

            break;
        }

            if (tmp.size() > 3)
            {
                sub_ops.y -= 40;
                l.vbo.add_text( main_font, tmp, c, sub_ops);
            }

            if (idx == 0 || (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE))
            {
                // print options info
                switch (l.curr_item)
                {//RESTORE_NOTICE
                case cf_ops::HOSTAPP_OPTS::REFRESH_HA:
                    print_option_info(l, pen, getLangSTR(LANG_STR::REFRESH_HOSTAPP_DESC), "", false);
                    break;
                case cf_ops::HOSTAPP_OPTS::LAUNCH_HA:
                    if (li.multi_sel.is_active && li.multi_sel.pos.x == FIRST_MULTI_LINE)
                    {
                        // show '< X >' around Option
                        if (app_state == RESUMABLE)
                            print_option_info(l, pen, getLangSTR(LANG_STR::GAME_CONTROL_INFO), getLangSTR(LANG_STR::SUSPEND_IF), false);
                        else
                            print_option_info(l, pen, getLangSTR(LANG_STR::CLOSE_OPEN_APP_INFO), "", false);
                    }
                    else if (app_state == NO_APP_OPENED)
                        print_option_info(l, pen, getLangSTR(LANG_STR::LAUNCH_INFO), getLangSTR(LANG_STR::SUSPEND_IF), false);
                    else if (app_state == OTHER_APP_OPEN)
                        print_option_info(l, pen, getLangSTR(LANG_STR::CLOSE_OPEN_APP_INFO), "", false);
                    else if (app_state == RESUMABLE)
                        print_option_info(l, pen, getLangSTR(LANG_STR::GAME_CONTROL_INFO), getLangSTR(LANG_STR::SUSPEND_IF), false);

                     break;
                default:
                    break;
                }
            }
            return;
    
}

/*
    last review_menu:
    - correct positioning, in px
    - cache normalized rectangles
    - uses cached texture from atlas + UVs
    - uses *active_panel* selector
    - renders different selector, per layout_t
    - this is actually drawing 4 layouts:
      left, icon, cf_game_option, download panels
*/
std::string path;
void  GLES2_render_layout_v2(view_t vw)
{
    layout_t &l = getLayoutbyView(vw);
    if ( !l.is_shown || !l.is_active) return;
    bool is_file_browser = (vw == FILE_BROWSER_LEFT || vw == FILE_BROWSER_RIGHT);
    //  log_info("%s %p %d", __FUNCTION__, l.vbo, l.vbo_s);

    int field_s, f_sele, rem, i_paged;

    vec2 p, pen, s,          // item position and size, in px
        tp,            // used for text position, in px
        b1 = { 30, 30 }, // border between rectangles, in px
        b2 = { 10, 10 }; // border between text and rectangle, in px
    vec4 r,             // normalized (float) rectangle
        selection_box,
        tv = 0.;       // temporary vector //tv.xy = b1 * (l.fieldsize - 1.);

    // normalized custom color, like Settings
    vec4 c = (vec4){ 41., 41., 41., 256. } / 256.;

    if (vw == ITEMzFLOW || vw == ITEM_PAGE || vw == ITEM_SETTINGS) /* draw a bg rectangle from bound_box + borders */
    {
        p = (vec2){ l.bound_box.x - b1.x,     l.bound_box.y + b1.y },
            s = (vec2){ l.bound_box.z + b1.x * 2, -l.bound_box.w - b1.y * 2 };
        s += p;
        r.xy = px_pos_to_normalized(&p),
            r.zw = px_pos_to_normalized(&s);
        // temporary vector: use for custom color, normalize 0.f-1.f
        tv = (vec4){ 41., 41., 100., 128. } / 256.;
        // gles render the single rectangle
        std::vector<vec4> v;
        v.push_back(r);
        ORBIS_RenderFillRects(USE_COLOR, tv, v, 1);
    }
    if (vw == ITEM_PAGE && ls_p.curr_item == cf_ops::REG_OPTS::GAME_SAVE && skipped_first_X)
    {
        vec4 cc = (vec4){41., 41., 41., 256.} / 256.;
        draw_rect(cc, 770., 800., 600., -320.);
        if(ic > GL_NULL){
           render_button(ic, 128., 228., 800., 600., 1.);
        }
    }
    else if (vw == ITEM_PAGE && ls_p.curr_item == cf_ops::REG_OPTS::TRAINERS && skipped_first_X)
    {
        vec4 cc = (vec4){41., 41., 41., 256.} / 256.;
        draw_rect(cc, 770., 800., 600., -320.);
    }

    if (!is_file_browser)
    {
        /* destroy VBO */
        if (l.vbo_s == ASK_REFRESH)
        {
            l.vbo.clear();
        }
    }

if (!l.vbo) // we cleaned vbo ?
{
    l.vbo = VertexBuffer("vertex:3f,tex_coord:2f,color:4f");
    l.vbo_s = EMPTY;
}
    /* compute normalized rectangle we will draw, once! */
if (l.vbo_s < CLOSED && !is_file_browser)
{
    int curr_page = l.page_sel.x;

    // get global item index: first one for current page
    int g_idx = curr_page * (l.fieldsize.x * l.fieldsize.y);

    if (l.f_rect.empty())
         l.f_rect.resize(l.fieldsize.x * l.fieldsize.y);
    
     // draw the field, according to passed layout_t
    for (int i = 0; i < l.f_size; i++)
    {
        int loop_idx = g_idx + i;

        if (loop_idx > l.item_c - 1) break;

    // temporary vector: use to load borders and get UVs
    tv.xy = b1;
    // compute position and size in px, then fill normalized UVs
    r = get_rect_from_index(i, l, &tv);
    p = r.xy, s = r.zw;    // swap r w/ p, s and reuse r below
    p += l.bound_box.xy;  // add origin: it's bound box position
    p.y -= l.bound_box.w; // move back down by *minus* size.h(!)
    tp = p + b2;           // update pen for text positioning
    tp.y += s.y;           // relate b2 to BotLef of current rectangle
    s += p;                // turn size into a second point
    // convert to normalized coordinates
    r.xy = px_pos_to_normalized(&p),
    r.zw = px_pos_to_normalized(&s);
    r.yw = r.wy;      // flip on Y axis
    l.f_rect[i] = r; // save the normalized rectangle
    // to refresh main header/title screen
    bool save_text = false;
    // if item is the selected one save frect for optional work
    if (i == l.f_sele)
    {
        selection_box = r,
        save_text = true;
    }
    // texts: check for its vbo
    if (l.vbo && l.vbo_s < CLOSED)
    { /* draw over item: text pos, in px */
       if(vw == ITEM_PAGE  && !is_vapp(title_id.get()))
        layout_compose_text(vw, loop_idx, tp, save_text);
       else if (vw == ITEM_PAGE  && is_vapp(title_id.get())){
         if(title_id.get() == APP_HOME_HOST_TID){
            hostapp_vapp_compose_text(vw, loop_idx, tp, save_text);
         }
       }
       else
        layout_compose_text(vw, loop_idx, tp, save_text);
    }
} /// EOFor each item

}

/* use cached normalized f_rects */
if (!l.f_rect.empty() && !is_file_browser)
{ // read from cached normalized rectangle for more work
    selection_box = l.f_rect[l.f_sele];

    // basic
    if (vw == ITEMzFLOW  || vw == ITEM_PAGE || vw == ITEM_SETTINGS) // draws the option boxes behind the text
    { // draw all the rectangle array by count, at once
        ORBIS_RenderFillRects(USE_COLOR, c, l.f_rect, l.f_size);
    }
}

    if (is_file_browser)
    {
        field_s = l.fieldsize.x * l.fieldsize.y,
        f_sele = l.item_sel.y * l.fieldsize.x + l.item_sel.x,
        i_paged = l.page_sel.x * field_s;

        /* if last page can't fill the field... */
        rem = l.item_c - i_paged + /* first title */ 1;
        // decrease the field size!
        if (rem < field_s)
            field_s = rem;

        if (field_s < 1)
            field_s = 1;
            
        int i;
        // iterate items we are about to draw
        for (i = 0; i < field_s; i++)
        {
            item_t &li = l.item_d[i_paged + i];

            /* compute origin position for this item */
            s = (vec2){l.bound_box.z / l.fieldsize.x,
                       l.bound_box.w / l.fieldsize.y};
            s -= b1; // consider borders!
            p = (vec2){(float)(l.bound_box.x + (i % (int)l.fieldsize.x) * (s.x + b1.x /* px border */)),
                       (float)(l.bound_box.y - (i + 1) / l.fieldsize.x * (s.y + b1.y /* px border */))};
            // .xy point additions:
            p += p_off.xy; // add offset: global displacing
            s += p;        // turn size into a second point
            r.xy = px_pos_to_normalized(&p);
            r.zw = px_pos_to_normalized(&s);
            // save for pixelshader
            vec4 test_r = (vec4){p.x, p.y, s.x, s.y};

            // panel is active, highlight with filled rectangle
            if (l.is_active)
            { // custom color, like Settings
                vec4 c = (vec4){41., 41., 41., 256.} / 256.;
                // gles render the single iterated rectangle
                std::vector<vec4> rr;
                rr.push_back(c);
                ORBIS_RenderFillRects(USE_COLOR, c, rr, 1);

                // is item the selected one ?
                if (i == f_sele)
                { // save frect for optional glowing selection box or...
                    selection_box = r;
                    // indirect positioning
                    p1_pos = r.xy;
                    // a VBO for pixelshader
                    if (!test_v)
                         test_v = vbo_from_rect(test_r);
                }
            }

            // draw outline rectangle for this item[i]
            ORBIS_RenderDrawBox(USE_COLOR, grey, r);

            // texts: check for its vbo
            if (l.vbo && l.vbo_s < CLOSED)
            {
                // build filepath
                std::string out;
                /* draw over item */
                out = fmt::format("{0:.30}{1:}", li.info.id, li.info.id.size() > 30 ? "..." : "");


                // update pen
                pen = p;
                pen += b2; // consider borders!

                // the very first reserved_index is the title and retative path
                if (!(i_paged + i))
                { // append to VBO
                    l.vbo.add_text( titl_font, out, col, pen);
                    vec4 c = col * .75f;
                    l.vbo.add_text( sub_font, fmt::format(", {} items", li.count.token_c), c, pen);
                }
                else
                { // append to VBO
                
                    li.flags.is_all_pkg_enabled ? pen.x += 64, pen.y += 9 : pen.x;
                    l.vbo.add_text( sub_font, out, col, pen);
                    //log_info("%s %s", li.info.id.c_str(), li.is_all_pkg_enabled ? "enabled" : "disabled");
                    path = (l.item_d[0].info.id.size() > 1 ? l.item_d[0].info.id : "") + "/" + li.info.id;
                    if(!li.flags.is_all_pkg_enabled)
                        get_stat_from_file(out, path);
                    else{
                        out = getLangSTR(LANG_STR::INSTALL_ALL_PKGS_DEC);
                        pen.y -= 9;
                    }

                    // we need to know Text_Length_in_px in advance, so we call this:
                    texture_font_load_glyphs(main_font, out.c_str());
                    // we know 'tl' now, right align
                    pen.x = p.x + l.bound_box.z - tl - b2.x - 22;
                    // append to VBO
                    vec4 c = col * .75f;
                    l.vbo.add_text( main_font, out, c, pen);
                }
            }
        }
        r = selection_box;
        // panel is active, show selector
        if (l.is_active){ 
            // render new selector
            std::vector<vec4> sl;
            sl.push_back(r);
            ORBIS_RenderFillRects(USE_COLOR, grey, sl, 1);
            // gles render the glowing blue box
            ORBIS_RenderDrawBox(USE_UTIME, sele, r);
        }
    }
    // close text vbo
    if (l.vbo_s < CLOSED){ l.vbo_s = CLOSED;refresh_atlas(); }
        /* elaborate more on the selected item: */
        r = selection_box;

        // panel is active, show selector
        if ( !is_file_browser )
        { /* also, different panel have different selector */
             GLES2_render_selector(vw, r);
        }

        l.vbo.render_vbo(NULL);

        //  log_info("%s %p", __FUNCTION__, l);
}

static void bound_check(int* idx, int Min, int Max)
{
    if (*idx < Min) *idx = Max;
    if (*idx > Max) *idx = Min;
}

/* update field_size for layout: eventually reduce
   field_size to match remaining item count */
void layout_update_fsize(layout_t& l)
{
    l.f_size = l.fieldsize.x * l.fieldsize.y;
    int count = l.item_c;
    // reduce field_size
    if (count < l.f_size)
    {
        l.f_size = count;
        l.vbo_s = ASK_REFRESH;
    }
}

int setting_on = -1;
extern bool is_sub_enabled;
/* each keypress updates linear and vector indexes */
void layout_update_sele(layout_t& l, int movement)
{
    // unconditionally ask to refresh!
    l.vbo_s = ASK_REFRESH;

    // check current item: is Multi-Sel?
    if (l.item_d[l.curr_item].multi_sel.is_active)
    {
        //log_info("Multi-Sel status: %d", l.item_d[l.curr_item].multi_sel.pos.x);
        if (l.item_d[l.curr_item].multi_sel.pos.x == FIRST_MULTI_LINE) // EDITABLE
        {
            // pass movement to Multi-Sel
            int res = l.item_d[l.curr_item].multi_sel.pos.y += movement;

            bound_check(&res, l.item_d[l.curr_item].multi_sel.pos.z,
                l.item_d[l.curr_item].multi_sel.pos.w - 1);
            // copy back result into destination!
            l.item_d[l.curr_item].multi_sel.pos.y = res;
            //printf("%d\n", l.item_d[ l.curr_item ].multi_sel.pos.y);

            return;
        }
    }

    // update all related to this one!
    int idx = l.curr_item + movement;

    //  if (idx < 0) idx = l.item_c - 1, l.vbo_s = ASK_REFRESH;
    //  if (idx > l.item_c - 1) idx = 0, l.vbo_s = ASK_REFRESH;
    bound_check(&idx, 0, l.item_c - 1);
    // update vector and linear index
    l.item_sel = (ivec2){ idx % l.fieldsize.x,
                           idx / l.fieldsize.x };
    l.curr_item = idx;
//    log_info("l %i\n", idx);
    l.f_size = l.fieldsize.x * l.fieldsize.y;

    if (l.f_size)
    {
        l.f_sele = idx % l.f_size;
    }
    else // guard the div by zero!
    {
        l.f_sele = 0,
            l.page_sel = (ivec2)(0);
    }
    layout_update_fsize(l);
}

int numb_of_settings = NUMBER_OF_SETTINGS;
void GLES2_render_list(view_t v)
{
    layout_t &l = getLayoutbyView(v);
    bool is_file_manager = (v == FILE_BROWSER_LEFT || v == FILE_BROWSER_RIGHT);

    if (v == ITEM_PAGE) {
        if (!l.is_active || old_status != app_status.load())
        {
            log_info("init layout for %s", title_id.get().c_str());
            if(!is_vapp((title_id.get()))){
                GLES2_layout_init(10, ls_p);
                l.bound_box = (vec4){ 120, resolution.y - 200,  620, 600 };
                l.fieldsize = (ivec2){ 2, 5 };
            }
            else if(title_id.get() == APP_HOME_HOST_TID){
                GLES2_layout_init(2, l);
                //l.bound_box = (vec4){ 120, resolution.y - 200,  620, 100 };
                //l.fieldsize = (ivec2){ 2, 1 };
                l.bound_box = (vec4){ 120, resolution.y - 200,  620, 600 };
                l.fieldsize = (ivec2){ 2, 5 };
            }

            switch (app_status.load())
            {
            case RESUMABLE:
                gm_p_text.at(0) = getLangSTR(LANG_STR::GAME_CONTROLS);
                break;
            case APP_NA_STATUS:
            case NO_APP_OPENED:
                gm_p_text.at(0) = getLangSTR(LANG_STR::LAUNCH_GAME);
                break;
            case OTHER_APP_OPEN:
                gm_p_text.at(0) = getLangSTR(LANG_STR::CLOSE_GAME);
                break;
            }
            layout_fill_item_from_list(l, gm_p_text);
            log_info("app_status: %i | old_status: %i", app_status.load(), old_status);
            old_status = app_status.load(); // update status
        }
    }
    else if (v == ITEM_SETTINGS) {
        if (!setting_p.is_active) {
            GLES2_layout_init(numb_of_settings, l);
            //X Y | W , H
            l.bound_box = (vec4){resolution.y / 2, resolution.y - 230, (numb_of_settings > NUMBER_OF_SETTINGS) ? 1200 : 800, 600};
            // First: amount in 1 column, 2nd: Amount allowed in 1 column?
            l.fieldsize = (ivec2){(numb_of_settings > NUMBER_OF_SETTINGS) ? 3 : 2, 6 };
            layout_fill_item_from_list(l, ITEMZ_SETTING_TEXT);
            
        }
    }
    else if (is_file_manager){
        vec2 size = {(resolution.x - 50 * 3) / 2, // we draw two coloumns: L/R, so 3 (*50px) border
                     (resolution.y - 50 * 2)};    // we center whole rectangle, so 2 (*50px) border

        if (fs_ret.status != FS_NOT_STARTED && fs_ret.status >= FS_DONE)
        {
            v_curr = fs_ret.last_v;
            if (fs_ret.status == FS_DONE &&
                !fs_ret.selected_file.empty() && fs_ret.fs_func && !fs_ret.selected_full_path.empty())
            {
                //Start the stored function pointer
                fs_ret.fs_func(fs_ret.selected_file, fs_ret.selected_full_path);
            }


            fm_lp.vbo.clear();
            fm_rp.vbo.clear();
            fm_lp.item_d.clear(), fm_rp.item_d.clear();
            fm_rp.fs_is_active = fm_lp.fs_is_active = fm_lp.is_shown = fm_rp.is_shown = fm_lp.is_active = fm_rp.is_active = false;
            //active_p = fs_ret.last_layout;
            title_vbo.clear();
            fs_ret.status = FS_NOT_STARTED;
            return;
        }

        if (!fm_lp.is_active || !fm_rp.is_active) // dynalloc and init this panel
        {
            if(get->setting_bools[Show_Buttons])
                fm_rp.bound_box = fm_lp.bound_box = (vec4){50, resolution.y - 50, size.x, size.y - 150};
            else
                fm_rp.bound_box = fm_lp.bound_box = (vec4){50, resolution.y - 50, size.x, size.y};

            fm_rp.bound_box.x += fm_lp.bound_box.z + 50;

            index_items_from_dir_v2(FS_START_1, fm_lp.item_d);
            // if no usb, then we have to show the internal storage
            if(usbpath() != -1)
               index_items_from_dir_v2(fmt::format("/mnt/usb{0:d}", usbpath()), fm_rp.item_d);
            else
               index_items_from_dir_v2(FS_START_2, fm_rp.item_d);

            fm_lp.item_c = fm_lp.item_d[0].count.token_c, fm_rp.item_c = fm_rp.item_d[0].count.token_c;

            fm_rp.fieldsize = fm_lp.fieldsize = (ivec2){1, FM_ENTRIES};
            fm_rp.is_shown = fm_rp.is_active = true,
            fm_lp.is_shown = fm_lp.is_active  = true;
            fm_lp.fs_is_active = true, fm_rp.fs_is_active = true;
            //active_p = fm_lp;
        }
        GLES2_render_layout_v2(FILE_BROWSER_LEFT);
        GLES2_render_layout_v2(FILE_BROWSER_RIGHT);
    }
   // v_curr = v;


    if (!is_file_manager){  
        l.is_shown = true;
        GLES2_render_layout_v2(v);
    }
        // we can also free data once vbo is done
        // destroy_item_t(&l.item_d);
}

/*-------------------- global variables ---*/

// ---------------------------------------------------------------- display ---
void pixelshader_render()
{
    curr_Program = shader;
    glUseProgram(curr_Program);
    {
        // enable alpha channel
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glUniform2f(glGetUniformLocation(curr_Program, "resolution"), resolution2.x, resolution2.y);
        // mouse position and actual cumulative time
        glUniform2f(glGetUniformLocation(curr_Program, "mouse"), p1_pos.x, p1_pos.y);
        glUniform1f(glGetUniformLocation(curr_Program, "time"), u_t); // notify shader about elapsed time
        // ft-gl style: MVP
        glUniformMatrix4fv(glGetUniformLocation(curr_Program, "model"), 1, 0, model_menu.data);
        glUniformMatrix4fv(glGetUniformLocation(curr_Program, "view"), 1, 0, view_menu.data);
        glUniformMatrix4fv(glGetUniformLocation(curr_Program, "projection"), 1, 0, projection_menu.data);

        //vertex_buffer_render2(rects_buffer, GL_TRIANGLES);
        rects_buffer.render(GL_TRIANGLES);

        glDisable(GL_BLEND);
    }
    glUseProgram(0);
}

// ---------------------------------------------------------------- reshape ---
static void reshape(int width, int height)
{
    glViewport(0, 0, width, height);
    mat4_set_orthographic(&projection_menu, 0, width, 0, height, -1, 1);
}

// ------------------------------------------------------ from source shaders ---
/* ============================= Pixel shaders ============================= */
static GLuint CreateProgramFE(int program_num, std::string path = "")
{
    GLuint programID;
    const char* data = NULL;

    switch (program_num)
    {
    case 0:
        programID = BuildProgram(p_vs1, p_fs0, p_vs1_length, p_fs0_length);
        break;
    case 1:
        programID = BuildProgram(p_vs1, p_fs1, p_vs1_length, p_fs1_length);
        break;
    case 2:
        programID = BuildProgram(p_vs1, p_fs2, p_vs1_length, p_fs2_length);
        break;
    case 3:
        programID = BuildProgram(p_vs1, p_fs3, p_vs1_length, p_fs3_length);
        break;
    case 4:
        programID = BuildProgram(p_vs1, p_fs5, p_vs1_length, p_fs5_length);
        break;
    case 5:
        log_debug("Next (LOADING)");
        programID = -1;
        data = (const char*)orbisFileGetFileContent(path.c_str()); 
        if(data == NULL){
            log_debug("Error: can't load shader.bin");
            ani_notify(NOTIFI::WARNING, "Unable to load Background Shader", "No One should see this");
            break;
        }
        else{
           programID = BuildProgram(p_vs1, data, p_vs1_length, _orbisFile_lastopenFile_size);
           free((void*)data);
        }
        break;
    default:
        programID = BuildProgram(p_vs1, p_fs2, p_vs1_length, p_fs2_length);
        break;
    }

    if (!programID) { log_error("program creation failed!, switch: %i", program_num); sleep(2); }

    return programID;
}
// ------------------------------------------------------------------- init ---

typedef struct {
    float x, y, z;    // position (3f)
    float r, g, b, a; // color    (4f)
} pixel_vertex_t;

void pixelshader_init(int width, int height)
{
    resolution2 = (vec2){ width, height };
    rects_buffer = VertexBuffer("vertex:3f,color:4f");
    vec4   color = { 1, 0, 0, 1 };
    float r = color.r, g = color.g, b = color.b, a = color.a;
       int  x0 = (int)(0);
    int  y0 = (int)(0);
    int  x1 = (int)(width);
    int  y1 = (int)(height);

    /* VBO is setup as: "vertex:3f, color:4f */
    pixel_vertex_t vtx[4] = { { x0,y0,0,  r,g,b,a },
                        { x0,y1,0,  r,g,b,a },
                        { x1,y1,0,  r,g,b,a },
                        { x1,y0,0,  r,g,b,a } };
    // two triangles: 2 * 3 vertex
    GLuint   idx[6] = { 0, 1, 2,    0, 2, 3 };
    //vertex_buffer_push_back(rects_buffer, vtx, 4, idx, 6);
    rects_buffer.push_back(vtx, 4, idx, 6);

/* compile, link and use shader set->using_sb */

if(mz_shader == GL_NULL)
   mz_shader = CreateProgramFE(0);

if ((shader == GL_NULL && !get->setting_bools[has_image] && get->setting_bools[using_sb] && if_exists(get->setting_strings[SB_PATH].c_str())))
{
    fmt::print("[THEME] Loading Shader from {}....", get->setting_strings[SB_PATH]);
    shader = CreateProgramFE(5, get->setting_strings[SB_PATH]);
}
else if ((!get->setting_bools[has_image] && !get->setting_bools[using_sb] && shader == GL_NULL) || (shader == GL_NULL && !get->setting_bools[using_theme]))
{
    shader = CreateProgramFE(4); // test emb2
}
 // feedback
log_info("[%s] program_id=%d (0x%08x)", __FUNCTION__, mz_shader, mz_shader);
log_info("[%s] program_id=%d (0x%08x)", __FUNCTION__, shader, shader);

mat4_set_identity(&projection_menu);
mat4_set_identity(&model_menu);
mat4_set_identity(&view_menu);

reshape(width, height);
}

void pixelshader_fini(void)
{
    rects_buffer.clear();

    if (shader)    glDeleteProgram(shader), shader = 0;
    if (mz_shader) glDeleteProgram(mz_shader), mz_shader = 0;
}


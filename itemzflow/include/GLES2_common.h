#pragma once
#include <time.h>
/// from demo-font.c
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <freetype-gl.h>
#include <defines.h>
#include <unordered_map>
#ifdef __ORBIS__
#include <user_mem.h>
#endif
#include "stb_image.h"
#include <atomic>
#include <memory>
#include "log.h"
#include <mutex>
#include <string>


typedef enum vbo_status
{
    EMPTY,
    APPEND,
    CLOSED,
    ASK_REFRESH,
    IN_ATLAS,
    IN_ATLAS_APPEND,
    IN_ATLAS_CLOSED
} vbo_status;

struct multi_t
{
    ivec4 pos = (ivec4){0, 0, 0, 0};
    bool is_active = false;
};
struct STBIImageDeleter {
  void operator()(unsigned char *ptr) const { stbi_image_free(ptr); }
};


struct AppIcon {
  struct ImageData {
    std::unique_ptr<unsigned char, STBIImageDeleter> data = nullptr;
    int w = 0, h = 0, comp = 0;
    bool tried_once = false;

    ImageData() = default;

    ImageData(const ImageData &other)
        : w(other.w), h(other.h), comp(other.comp) {
      if (other.data) {
        data.reset();
      }
    }

    ImageData &operator=(const ImageData &other) {
      if (this != &other) {
        w = other.w;
        h = other.h;
        comp = other.comp;

        if (other.data) {
           data.reset();
        }
      }
      return *this;
    }

    ~ImageData() = default;

    void load(const char *filepath, std::atomic<bool> &is_loaded) {

     if(tried_once){
      //   log_debug("Already tried to load %s", filepath);
         return;
     }
     // data = load_png_asset(filepath, is_loaded, *this);
      unsigned char *image =  stbi_load(filepath, &w, &h, &comp, STBI_rgb_alpha);
      if (image == nullptr) {
        log_debug("%s(%s) FAILED!", __FUNCTION__, filepath);
        tried_once = true;
        return;
      }
      is_loaded = true;
      data = std::unique_ptr<unsigned char, STBIImageDeleter>(image);
    }
  };

  std::atomic<GLuint> texture; // The item icon
  vec4 uv;                     // Normalized p1, p2
  std::atomic<bool> cover_exists, turn_into_texture;
  ImageData image_data;

  AppIcon() : texture(0), cover_exists(false), turn_into_texture(false) {
    uv = {0, 0, 0, 0};
    image_data = ImageData(); // Default construct image_data
  }

  // Custom copy constructor
  AppIcon(const AppIcon &other)
      : texture(other.texture.load()), uv(other.uv),
        cover_exists(other.cover_exists.load()),
        turn_into_texture(other.turn_into_texture.load()),
        image_data(other.image_data) {}

  // Custom copy assignment operator
  AppIcon &operator=(const AppIcon &other) {
    if (this == &other) {
      return *this;
    }

    texture = other.texture.load();
    uv = other.uv;
    cover_exists = other.cover_exists.load();
    turn_into_texture = other.turn_into_texture.load();
    image_data = other.image_data;

    return *this;
  }
};

struct ItemFlags {
    bool is_ext_hdd = false;
    bool is_fpkg = true;
    bool usbvapp_not_found = false;
    bool is_vapp = false;
    bool is_dumpable = false;
    bool app_vis = true;
    bool is_ph = false;
    bool is_favorite = false;
    bool is_all_pkg_enabled = false;
};

struct ItemData {
    std::string id;
    std::string name;
    std::string image;
    std::string package;
    std::string version;
    std::string picpath;
    std::string sfopath;
    std::string vapp_path;
};

struct ItemCount {
    int token_c = 0;
    int HDD_count = 0;
};

struct ItemExtraData {
    std::unordered_map<std::string, std::string> extra_sfo_data;
};

struct item_t {
    ItemData info;
    ItemCount count;
    AppIcon icon;
    multi_t multi_sel;
    ItemExtraData extra_data;
    ItemFlags flags;
};

#include "feature_classes.hpp"

struct layout_t
{
    vec4        bound_box;
    ivec2       fieldsize,
                item_sel,
                page_sel;
    int         retries;
   ThreadSafeVector<item_t> item_d;
   //item_t* item_d = NULL;
    int         item_c,
                curr_item,
                f_size,
                f_sele;
    std::vector<vec4> f_rect;

    //vec4 f_rect;    // field rectangle array
    // each layout will hold its ft-gl vertex buffer object
    //vertex_buffer_t* vbo = NULL;
    VertexBuffer vbo;
    vbo_status   vbo_s;
    bool is_shown, is_active, fs_is_active;
    std::recursive_mutex mtx;

    layout_t() : retries(0), item_c(0), curr_item(0), f_size(0), f_sele(0),
             vbo_s(EMPTY), is_shown(false), is_active(false), fs_is_active(false)
    {
        bound_box = {0, 0, 0, 0};
        fieldsize = {0, 0};
        item_sel = {0, 0};
        page_sel = {0, 0};
    }
};


struct _ent {
    std::string fname, fname2;
    uint32_t sz;
    int lvl;
};

typedef enum {
    ITEMZCORE_GENERAL_ERROR = 0xDEADBEEF,
    ITEMZCORE_PRX_RESOLVE_ERROR = 0xCCCCCCC,
    ITEMZCORE_EINVAL = 0xEEEEEEE,
    ITEMZCORE_QUEUE_FULL = 0xAAAAAAA,
    ITEMZCORE_NULL_BUFFER = 0x123123,
    ITEMZCORE_SUCCESS = 0
} ITEMZCORE_ERRORS;

typedef struct
{
    char* name;
} entry_t;

enum SH_type
{
    USE_COLOR,
    USE_UTIME,
    CUSTOM_BADGE,
    NUM_OF_PROGRAMS
};

typedef enum token_name
{
    ID,
    NAME,
    IMAGE,
    PACKAGE,
    VERSION,
    PICPATH,
    SFO_PATH,
    SIZE,
    AUTHOR,
    APPTYPE,
    PV,
    TOTAL_NUM_OF_TOKENS,
    // should match NUM_OF_USER_TOKENS
} token_name;

typedef enum
{
    NO_CATEGORY_FILTER,
    FILTER_FAVORITES,
    FILTER_RETAIL_GAMES,
    FILTER_FPKG,
    FILTER_GAMES,
    FILTER_HOMEBREW,
    NUMB_OF_CATEGORIES
} Sort_Category;

struct IconPosition {
    int offset;
    float x;
    float y;
};


#define ORBIS_LIBC_MALLOC_MANAGED_SIZE_VERSION (0x0001U)

#ifndef SCE_LIBC_INIT_MALLOC_MANAGED_SIZE
#define SCE_LIBC_INIT_MALLOC_MANAGED_SIZE(mmsize) do { \
	mmsize.size = sizeof(mmsize); \
	mmsize.version = ORBIS_LIBC_MALLOC_MANAGED_SIZE_VERSION; \
	mmsize.reserved1 = 0; \
	mmsize.maxSystemSize = 0; \
	mmsize.currentSystemSize = 0; \
	mmsize.maxInuseSize = 0; \
	mmsize.currentInuseSize = 0; \
} while (0)
#endif

typedef struct LibcMallocManagedSize {
    uint16_t size;
    uint16_t version;
    uint32_t reserved1;
    size_t maxSystemSize;
    size_t currentSystemSize;
    size_t maxInuseSize;
    size_t currentInuseSize;
} LibcMallocManagedSize;


namespace multi_select_options {
typedef enum {
    NA_SORT = 0,
    TID_ALPHA_SORT,
    TITLE_ALPHA_SORT,
} Sort_Multi_Sel;

typedef enum {
    POWER_OFF_OPT = 0,
    RESTART_OPT,
    SUSPEND_OPT,
} Power_control_sel;

typedef enum {
    MOVE_APP = 0,
    MOVE_DLC,
    MOVE_PATCH
} MOVE_OPTIONS;

typedef enum
{
    RESTORE_APP = 0,
    RESTORE_DLC,
    RESTORE_PATCH
} RESTORE_OPTIONS;

typedef enum {
    UNINSTALL_GAME = 0,
    UNINSTALL_PATCH,
    UNINSTALL_DLC,
} Uninstall_Multi_Sel;

typedef enum {
    MOVE_DIR = 0,
    COPY_DIR,
    DELETE_DIR,
} IO_Options;

typedef enum {
    SHELLUI_SETTINGS_MENU = 0,
    SHELLUI_POWER_SAVE
} ShellUI_Multi_Sel;

typedef enum {
    REBUILD_ALL = 0,
    REBUILD_DLC,
    REBUILD_REACT,
} Rebuild_db_Multi_Sel;

typedef enum {
    GM_INVAL = -1,
    BACKUP_GAME_SAVE = 0,
    RESTORE_GAME_SAVE,
    DELETE_GAME_SAVE
} SAVE_Multi_Sel;

typedef enum {
    SEL_DUMP_ALL = 0,
    SEL_DUMP_VALID,
    SEL_BASE_GAME,
    SEL_GAME_PATCH,
    SEL_REMASTER,
    SEL_DLC,
    SEL_FPKG,
    SEL_RESET = 999,
    SEL_RUNNING_NOW = 696969
} Dump_Multi_Sels;
};

#define FIRST_MULTI_LINE 1
#define RESET_MULTI_SEL 0


typedef struct {
    const char* label; // Label to be emphasized
    std::string value; // Value to display
    float x, y;        // Position
} TextInfo;

namespace NOTIFI{
typedef enum {
    INFO,
    WARNING,
    SUCCESS,
    ERROR,
    GAME,
    KOFI,
    PROFILE,
    NUM_OF_NOTIS
} NOTI_LEVELS;
};
//NOTIFI::NOTI_LEVELS 

#define FIRST_MULTI_LINE 1
#define RESET_MULTI_SEL 0

typedef enum {
    DUMPER_PATH_OPTION,
    CHECK_FOR_UPDATE_OPTION,
    SORT_BY_OPTION,
    HOME_MENU_OPTION,
    REBUILD_DB_OPTION,
    THEME_OPTION,
    BACKGROUND_MP3_OPTION,
    FUSE_IP_OPTION,
    OPEN_SHELLUI_MENU,
    IF_PKG_INSTALLER_OPTION,
    POWER_CONTROL_OPTION,
    SAVE_ITEMZFLOW_SETTINGS
} Settings;

typedef enum
{
    DOWNLOAD_COVERS_OPTION = 12,
    RESET_THEME_OPTION = 13,
    SHOW_BUTTON_OPTION = 14,
    CHANGE_BACKGROUND_OPTION = 15,
    KILL_ON_CLOSE  = 16,
    FONT_OPTION = 11,
    NUMB_OF_EXTRAS = 17,

} Advanced_Settings;

#define ARRAYSIZE(a) ((sizeof(a) / sizeof(*(a))) / ((size_t)(!(sizeof(a) % sizeof(*(a))))))

typedef enum view_t
{
    RANDOM = -1,
    ITEMzFLOW = 0,
    ITEM_PAGE = 1,
    ITEM_SETTINGS = 2,
    FILE_BROWSER_LEFT = 3,
    FILE_BROWSER_RIGHT = 4,
} view_t;
void GLES2_render_list(enum view_t v);

struct CVec
{
    uint32_t sz, cur;       // In BYTES
    void* ptr;
};

enum views
{
    ON_ITEMzFLOW = 8,    // 8
  
};


typedef struct retry_t
{
    GLuint tex;      // the real texture
    bool exists;
} retry_t;


namespace cf_ops {
typedef enum
{
    LAUNCH_GAME,
    DUMP_GAME,
    UNINSTALL,
    RETAIL_UPDATES,
    TRAINERS,
    HIDE_APP,
    CHANGE_ICON,
    CHANGE_NAME,
    MOVE_APPS,
    RESTORE_APPS,
   
} REG_OPTS;

typedef enum
{
    LAUNCH_HA,
    REFRESH_HA,  
} HOSTAPP_OPTS;


typedef enum
{
    LAUNCH_USBVAPP,
    CHANGE_DIR_USBVAPP,
    COPY_DIR_LOCAL_HA,
    OVERRIDE_TID,
    SCAN_FOR_APPS,

} USBVAPP_OPTS;

typedef enum
{
    LAUNCH_VAPP,
    IO_Options,
    REMOVE_VAPP,
    SHOW_APP_INFO,

} VAPP_OPTS;


};

std::string getFolderName(const std::string &fullPath);

// menu entry strings
#define  LPANEL_Y  (5)

#if defined(__ORBIS__)

#define UP   (111)
#define DOW  (116)
#define LEF  (113)
#define RIG  (114)
#define CRO  ( 53)
#define CIR  ( 54)
#define TRI  ( 28)
#define SQU  ( 39)
#define OPT  ( 32)
#define L1   ( 10)
#define R1   ( 11)
#define L2   (222)
#else

#define UP   (98)
#define DOW  (104)
#define LEF  (100)
#define RIG  (102)
#define CRO  ( 53)
#define CIR  ( 54)
#define TRI  ( 28)
#define SQU  ( 39)
#define OPT  ( 32)
#define L1   ( 10)
#define R1   ( 27)
#define L2   ( 12)
#endif

    extern GLuint fallback_t;

// for Settings (options_panel)
extern bool use_reflection,
use_pixelshader;

int count_availables_json(void);
void refresh_atlas(void);

// from GLES2_rects.c
void on_GLES2_Update1(double time);

// from GLES2_textures.c
void on_GLES2_Init_icons(int view_w, int view_h);
void on_GLES2_Update(double time);
//void on_GLES2_Render_icons(int num);
//void on_GLES2_Render_icon(enum SH_type SL_program, GLuint texture, int num, vec4* frect, vec4* uv);

void on_GLES2_Render_box(vec4* frect);
void on_GLES2_Final(void);

// from pixelshader.c
void pixelshader_render();
void pixelshader_init(int width, int height);
void pixelshader_fini(void);

void ORBIS_RenderSubMenu(int num);

// GLES2_font.c
bool GLES2_fonts_from_ttf(const char* path);
void GLES2_init_submenu(void);
void ftgl_render_vbo(vertex_buffer_t* vbo, vec3* offset);

void escalate_priv(void);
int Start_IF_internal_Services(const char* launch_q);
int pingtest(int libnetMemId, int libhttpCtxId, const char* src);

int df(std::string mountPoint, std::string &out);

int get_item_index(layout_t& l);
// from GLES2_scene_v2.c
void GLES2_layout_init(int req_item_count, layout_t &l);

VertexBuffer vbo_from_rect(vec4& rect);

void GLES2_UpdateVboForLayout(layout_t& l);

int layout_fill_item_from_list(layout_t &l, std::vector<std::string> &i_list);

void fw_action_to_cf(int button);

void GLES2_render_menu(int unused);

// from GLES2_scene.c
void init_ItemzCore(int w, int h);
void Render_ItemzCore(void);
void fw_action_to_cf(int button);
//void X_action_dispatch(int action, layout_t &l);

/// pthreads used in GLES2_q.c

/* normalized rgba colors */
extern vec4 sele, // default for selection (blue)
grey, // rect fg
white,
col; // text fg

extern ivec4  menu_pos;
extern double u_t;

extern std::string completeVersion;

void GLES2_Draw_sysinfo(bool is_idle);
void GLES2_refresh_sysinfo(void);

// disk free percentages
extern double dfp_hdd, dfp_ext, dfp_now;
extern layout_t ls_p, setting_p;

// GLES2_layout
void layout_update_fsize(layout_t& l);
void layout_update_sele(layout_t& l, int movement);
void GLES2_Refresh_for_settings();
void layout_set_active(layout_t& l);
void swap_panels(void);
void  GLES2_render_layout_v2(view_t vw);


// scene_v2.c
void set_cmp_token(const int index);
int struct_cmp_by_token(const void* a, const void* b);

/// from shader-common.c
GLuint BuildProgram(const char* vShader, const char* fShader, int vs_size, int fs_size);
/* a double side (L/R) panel filemanager */
extern layout_t fm_lp, fm_rp;

int  es2init_text(int width, int height);
void render_text(void);
void es2sample_end(void);

/// from png.c
typedef struct {
    /* for image data */
    const int width;
    const int height;
    const int size;
    const GLenum gl_color_format;
    const void* data;
} RawImageData;

extern float tl;
bool is_all_pkg_enabled(int panel);

/* GLES2_panel.h */
extern texture_font_t *main_font, // small
                       *sub_font, // left panel
                      *titl_font; // bigger

// from GLES2_ani.c
RawImageData get_raw_image_data_from_png(const void *png_data, const int png_data_size, const char *fp);
void release_raw_image_data(const RawImageData *data);
GLuint load_texture(const GLsizei width, const GLsizei height,
    const GLenum  type, const GLvoid* pixels);
// higher level helper
GLuint load_png_asset_into_texture(const char* relative_path);
GLuint load_png_data_into_texture(const char* data, int size);
bool is_png_vaild(const char *relative_path, int *width, int *height);
extern vec2 tex_size; // last loaded png size as (w, h)
int writeImage(char* filename, int width, int height, int *buffer, const char* title);
/// from timing.c
float get_time_ms(std::chrono::time_point<std::chrono::system_clock> &last_time);
bool if_exists(const char* path);

extern int left_page_pos;
extern int right_page_pos;

void GLES2_ani_update(double time);
void GLES2_ani_fini(void);
void GLES2_ani_draw(void); 

void on_GLES2_Render_icon(enum SH_type SL_program, GLuint texture, int num, vec4 &frect, vec4 *uv);
void GLES2_ani_init(int width, int height);
int ani_notify(NOTIFI::NOTI_LEVELS lvl, std::string message, std::string detail);
int index_token_from_sfo(std::string *title, std::string *apptype, std::string *version, std::string path, int lang);;
    /// from ls_dir()
int check_stat(const char *filepath);

entry_t* get_item_entries(const char* dirpath, int* count);
void free_item_entries(entry_t* e, int num);

// from my_rects.c
vec2 px_pos_to_normalized(vec2* pos);


// from lines_and_rects
void es2rndr_lines_and_rect(void);
void es2init_lines_and_rect(int width, int height);
void es2fini_lines_and_rect(void);


/// GLES2_rects.c
void ORBIS_RenderFillRects_init(int width, int height);
void ORBIS_RenderFillRects_rndr(void);
void ORBIS_RenderDrawStar(const vec2 *coords, int size);
void ORBIS_RenderFillRects_fini(void);
void ORBIS_RenderDrawLines(const vec2* points, int count);
void ORBIS_RenderFillRects(enum SH_type SL_program, const vec4 &rgba, std::vector<vec4> &rects, int count);
void GLES2_DrawFillingRect(std::vector<vec4> &r, vec4 &c,  const double percentage);
    void ORBIS_RenderDrawBox(enum SH_type SL_program, const vec4 &rgba, vec4 &r);

void check_tex_for_reload(item_t& item);
void check_n_load_textures(item_t& item);
void InitScene_4(int width, int height);
void InitScene_5(int width, int height);
void O_action_dispatch(void);

void realloc_app_retries();
void index_items_from_dir(ThreadSafeVector<item_t> &out_vec, std::string dirpath, std::string dirpath2, Sort_Category category);
void get_stat_from_file(std::string &out_str, std::string &filepath_str);
std::string category_get_str(Sort_Category num);
std::string power_menu_get_str(multi_select_options::Power_control_sel num);
std::string uninstall_get_str(multi_select_options::Uninstall_Multi_Sel num);
void itemzcore_add_text(VertexBuffer &vert_buffer,float x, float y, std::string text);
void DrawWhiteStar();
void flip_frame(void);
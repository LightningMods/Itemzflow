/*
    effects on GLSL

    2020, masterzorag
*/

#include <stdio.h>
#include <string.h>
#include "defines.h"
#include <pthread.h>
#if defined(__ORBIS__)
#include "shaders.h"
#else
#include "pc_shaders.h"
#endif
#include <string>
#include <vector>

extern texture_atlas_t *atlas;
extern vertex_buffer_t *buffer;

std::vector<struct _ent> cv;

extern pthread_mutex_t notifcation_lock;

// ------------------------------------------------------- typedef & struct ---
typedef struct {
    float x, y, z;    // position (3f)
    float s, t;
    float r, g, b, a; // color    (4f)
} vertex_t;

extern GLuint btn_X;

extern int selected_icon; // from main.c
int last_num = 0;
    // ------------------------------------------------------- global variables ---
    // shader and locations
    static GLuint program = 0; // default program
static GLuint shader_fx  = 0; // text_ani.[vf]
static float  g_Time     = 0.f;
static GLuint meta_Slot  = 0;
static mat4   model, view, projection;
// ---------------------------------------------------------------- reshape ---
static void reshape(int width, int height)
{
    glViewport(0, 0, width, height);
    mat4_set_orthographic( &projection, 0, width, 0, height, -1, 1);
}

// ---------------------------------------------------------- animation ---
static fx_entry_t *ani;          // the fx info

static vertex_buffer_t *line_buffer,
                       *vbo = NULL;

// bool flag
static bool ani_is_running = false;
static NOTI_LEVELS level;
static GLuint game_noti = GL_NULL, warning_icon = GL_NULL, 
Success_tex = GL_NULL, Info_tex = GL_NULL, kofi = GL_NULL, profile_pic = GL_NULL;
// callback when done looping all effect status
static void ani_post_cb(void)
{
   // log_info("%s()", __FUNCTION__);
    O_action_dispatch();
}
#define MAX_QUEUE 10
#define MAX_MESSAGES 12


int rear = -1;
int front = -1;

/*
    default bg_rectangle position and size, in px
    size = 300 - 0, 1000 - 900 = 300, 100 px
*/
static vec4 notify_rect, // x1,y1  x2,y2 in px
            notify_icon;

static int max_width = 0;
// ---------------------------------------------------------------- display ---
static void render_ani(NOTI_LEVELS level, int text_num, int type_num )
{
    // we already clean in main render loop!

    fx_entry_t *ani = &fx_entry[0];
    program         = shader_fx;

    if(ani->t_now >= ani->t_life) // looping animation status
    {
        switch(ani->status) // setup for next fx
        {
            case ANI_CLOSED :  ani->status = ANI_IN,      ani->t_life  =   .5f;  break;
            case ANI_IN     :  ani->status = ANI_DEFAULT, ani->t_life  =  1.5f;  break;
            case ANI_DEFAULT:  ani->status = ANI_OUT,     ani->t_life  =   .5f;  break;
            case ANI_OUT    :  ani->status = ANI_CLOSED,  ani->t_life  =  0.2f,
            /* CLOSED reached: looped once all status */  ani_is_running =  false; 
            ani_post_cb();  break;
        }
        ani->t_now  = 0.f; // yes, it's using time
    }
/*
    log_info("program: %d [%d] fx state: %.1f, frame: %3d/%3.f %.3f\r", 
            program, t_n,
                     ani->status /10.,
                     ani->life,
                     fmod(ani->status /10. + type_num /100., .02));
*/  

    /* background my_notify */
    notify_rect = (vec4) {  0, 850,  300, 950 }; // x1,y1,  x2,y2, in px!
    notify_icon = (vec4) { 10, 860,   90, 940 };

    vec4 rect, // we fill this vector with Xs, move horiz.
        rgba; 

    switch (level)
    {
    case NOTIFI_INFO: //rgba = (vec4){ 0, 0, 0, 0.7 };
       // break;
    case NOTIFI_SUCCESS: //rgba = (vec4){ 0, 150, 0, 0.5 };
       // break;
    case NOTIFI_ERROR://  rgba = (vec4){ 1, 0, 0, 0.3 };
       // rgba = (vec4){ 0, 0, 0, 0.7 };
       // break;
    default:
        rgba = (vec4){ 0, 0, 0, 0.7 };
        break;
    }

    // enlarge rectangle to fit text lenght
    if(notify_rect.z - notify_rect.x - 100 < max_width) notify_rect.z = notify_rect.x + 100 + max_width + 10;
    notify_rect.z += 110;

    vec2 t1, t2;
    // we just animate on the X, normalize and pack 4 Xs in a vec4, save the 4 Ys
    t1 = notify_rect.xy;
    t2 = px_pos_to_normalized(&t1);  rect.x = t2.x;  notify_rect.y = t2.y;
    t1 = notify_rect.zw;
    t2 = px_pos_to_normalized(&t1);  rect.y = t2.x;  notify_rect.w = t2.y;
    t1 = notify_icon.xy;
    t2 = px_pos_to_normalized(&t1);  rect.z = t2.x;  notify_icon.y = t2.y;
    t1 = notify_icon.zw;
    t2 = px_pos_to_normalized(&t1);  rect.w = t2.x;  notify_icon.w = t2.y;

    vec3 pa    = (vec3) {0.15, 0., 1.};      // pos + alpha vector offset: as in glsl code
    double
         frame = ani->t_now / ani->t_life;   // frame: normalized 0-1
    vec3 s     = pa * frame;                 // compute step
    //printf("s:   %d %.4f, %.2f %.2f %.2f\n", ani->status, frame, s.x, s.y, s.z);
    switch(ani->status)
    {
        case ANI_CLOSED : rect -= pa.xxxx;            rgba.a  = 0.;           break;
        case ANI_IN:      rect += (s.xxxx - pa.xxxx); rgba.a *= frame;        break;
        case ANI_DEFAULT: break;
        case ANI_OUT:     rect -= s.xxxx;             rgba.a *= (1. - frame); break;

    }
    /* draw animated background rectangle */
    notify_rect.xz = rect.xy; // grab x1,x2
    ORBIS_RenderFillRects(USE_COLOR, &rgba, &notify_rect, 1);

    /* draw the icon */
    notify_icon.xz = rect.zw; // grab x1,x2

    switch (level)
    {
    case NOTIFI_SUCCESS: on_GLES2_Render_icon(USE_COLOR, Success_tex/*texnum*/, 2/*tex_unit*/, &notify_icon, NULL);
                    break;
    case NOTIFI_ERROR:  on_GLES2_Render_icon(USE_COLOR, btn_X/*texnum*/, 2/*tex_unit*/, &notify_icon, NULL);
                     break;
    case NOTIFI_WARNING:  on_GLES2_Render_icon(USE_COLOR, warning_icon/*texnum*/, 2/*tex_unit*/, &notify_icon, NULL);
        break;
    case NOTIFI_GAME:  on_GLES2_Render_icon(USE_COLOR, game_noti/*texnum*/, 2/*tex_unit*/, &notify_icon, NULL);
        break;
    case NOTIFI_KOFI:  on_GLES2_Render_icon(USE_COLOR, kofi/*texnum*/, 2/*tex_unit*/, &notify_icon, NULL);
        break;
    case NOTIFI_PROFILE:  on_GLES2_Render_icon(USE_COLOR, profile_pic/*texnum*/, 2/*tex_unit*/, &notify_icon, NULL);
        break;
    case NOTIFI_INFO:
    default:
        on_GLES2_Render_icon(USE_COLOR, Info_tex/*texnum*/, 2/*tex_unit*/, &notify_icon, NULL);
        break;
    }

    /* draw animated text */
    glUseProgram   ( program );
    glActiveTexture( GL_TEXTURE0 );
    glBindTexture  ( GL_TEXTURE_2D, atlas->id ); // rebind glyph atlas
    glDisable      ( GL_CULL_FACE );
    glEnable       ( GL_BLEND );
    glBlendFunc    ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    {
        glUniform1i       ( glGetUniformLocation( program, "texture" ),    0 );
        glUniformMatrix4fv( glGetUniformLocation( program, "model" ),      1, 0, model.data);
        glUniformMatrix4fv( glGetUniformLocation( program, "view" ),       1, 0, view.data);
        glUniformMatrix4fv( glGetUniformLocation( program, "projection" ), 1, 0, projection.data);
        glUniform4f       ( glGetUniformLocation( program, "meta"), 
                ani->t_now,
                ani->status /10., // we use float on SL, switching fx state
                ani->t_life,
                type_num    /10.);
        if(1) /* draw whole VBO (storing all added texts) */
        {
            vertex_buffer_render( vbo, GL_TRIANGLES );
        }
    }
    glDisable( GL_BLEND );
    // we already swap frame in main render loop!
}

/* wrapper from main */
void GLES2_ani_test( fx_entry_t *_ani )
{
    render_ani(level, 1, selected_icon %MAX_ANI_TYPE );
    //for (int i = 0; i < itemcount; ++i)
    {
    /*  render_text_extended( item_entry, fx_entry );
        read as: draw this VBO using this effect! */
//        render_ani( 0, TYPE_0 );
//        render_ani( 1, TYPE_1 );
//        render_ani( 2, TYPE_2 );
//        render_ani( 3, TYPE_3 );
    }
}

// --------------------------------------------------------- custom shaders ---
static GLuint CreateProgram( void )
{
    GLuint programID = BuildProgram(ani_vs, ani_fs, ani_vs_length, ani_fs_length); // shader_common.c
    if (!programID)
    {
        log_error( "failed!"); sleep(3);
    }
    // feedback
    log_error( "ani program_id=%d (0x%08x)", programID, programID);
    return programID;
}


// libfreetype-gl pass last composed Text_Length in pixel, we use to align text!
extern float tl;


// ------------------------------------------------------------------- main ---
void GLES2_ani_init(int width, int height)
{
    vec4 color  = { 200., 0., 200., 1. };
    line_buffer = vertex_buffer_new( "vertex:3f,color:4f" );

#if 0 // rects
    vec2 pen = { .5, .5 };
for (int i = 0; i < 10; ++i)
{
    int  x0 = (int)( pen.x + i * 2 );
    int  y0 = (int)( pen.y + 20 );
    int  x1 = (int)( x0 + 64 );
    int  y1 = (int)( y0 - 64 );

    GLuint indices[6] = {0,1,2,  0,2,3}; // (two triangles)

    /* VBO is setup as: "vertex:3f, vec4 color */ 
    log_info("%d %d %d %d", x0, y0, x1, y1);
    vertex_t vertices[4] = { { x0,y0,0,  color },
                             { x0,y1,0,  color },
                             { x1,y1,0,  color },
                             { x1,y0,0,  color } };
    vertex_buffer_push_back( line_buffer, vertices, 4, indices, 6 );
    pen.x   += 72.;
    pen.y   -= 32.;
    color.g -= i * 0.1; // to show some difference: less green
}
#else

    if (warning_icon == GL_NULL)
        warning_icon = load_png_asset_into_texture(asset_path("notis/warning.png"));
    if (game_noti == GL_NULL)
        game_noti = load_png_asset_into_texture(asset_path("notis/game.png"));
    if (Success_tex == GL_NULL)
        Success_tex = load_png_asset_into_texture(asset_path("notis/success.png"));
    if (Info_tex == GL_NULL)
        Info_tex = load_png_asset_into_texture(asset_path("notis/info.png"));
    if (kofi == GL_NULL)
        kofi = load_png_asset_into_texture(asset_path("notis/kofi.png"));
    if (profile_pic == GL_NULL)
        profile_pic = load_png_asset_into_texture(asset_path("notis/profile.png"));

 vec2 pen, origin;

    origin.x = width /2 + 100;
    origin.y = height/2 - 200;
//    add_text( text_buffer, big, "g", &black, &origin );
//vec4 black = { 0,0,0,1 };
    // title
    pen.x = 50;
    pen.y = height - 50;
//    add_text( text_buffer, title, "Glyph metrics", &black, &pen );

    float r = color.r, g = color.g, b = color.b, a = color.a;
    // lines
    vertex_t vertices[] =
        {
            // Baseline
            {10, 10, 0, r, g, b, a},
            {100, 100, 0, r, g, b, a},

            // Top line
            {origin.x, origin.y + 100, 0, r, g, b, a},
            {origin.x + 100, origin.y + 100, 0, r, g, b, a},
            // Bottom line
            {origin.x, origin.y, 0, r, g, b, a},
            {origin.x + 100, origin.y, 0, r, g, b, a},
            // Left line at origin
            {origin.x, origin.y + 100, 0, r, g, b, a},
            {origin.x, origin.y, 0, r, g, b, a},
            // Right line at origin
            {origin.x + 100, origin.y + 100, 0, r, g, b, a},
            {origin.x + 100, origin.y, 0, r, g, b, a},

            // Left line
            {(float)(width / 2 - 20 / 2), (float)(.3 * height),0, r, g, b, a},
            {(float)(width / 2 - 20 / 2), (float)(.9 * height), 0, r, g, b, a},

            // Right line
            {(float)(width / 2 + 20 / 2), (float)(.3 * height), 0, r, g, b, a},
            {(float)(width / 2 + 20 / 2), (float)(.9 * height), 0, r, g, b, a},

            // Width
            {(float)(width / 2 - 20 / 2), (float)(0.8 * height), 0, r, g, b, a},
            {(float)(width / 2 + 20 / 2), (float)(0.8 * height), 0, r, g, b, a},

            // Advance_x
            {(float)(width / 2 - 20 / 2 - 16), (float)(0.2 * height), 0, r, g, b, a},
            {(float)(width / 2 - 20 / 2 - 16 + 16), (float)(0.2 * height), 0, r, g, b, a},

            // Offset_x
            {(float)(width / 2 - 20 / 2 - 16), (float)(0.85 * height), 0, r, g, b, a},
            {(float)(width / 2 - 20 / 2), (float)(0.85 * height), 0, r, g, b, a},

            // Height
            {(float)(0.3 * width / 2), origin.y + 18 - 24, 0, r, g, b, a},
            {(float)(0.3 * width / 2), origin.y + 18, 0, r, g, b, a},

            // Offset y
            {(float)(0.8 * width), origin.y + 18, 0, r, g, b, a},
            {(float)(0.8 * width), origin.y, 0, r, g, b, a},

        };
    GLuint indices [] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,
                         13,14,15,16,17,18,19,20,21,22,23,24,25 };
    vertex_buffer_push_back( line_buffer, vertices, 26, indices, 26 );
#endif

    /* shader program is custom, so
    compile, link and use shader */
    shader_fx = CreateProgram();


    if(!shader_fx) { log_error("program creation failed"); }

    /* init ani effect */
    ani         = &fx_entry[0];
    ani->status = ANI_CLOSED,
    ani->t_now  = 0.f;  // set actual time
    ani->t_life = 2.f;  // duration in seconds

    mat4_set_identity( &projection );
    mat4_set_identity( &model );
    mat4_set_identity( &view );
    // attach our "custom infos"
    meta_Slot = glGetUniformLocation(shader_fx, "meta");

    reshape(width, height);
}

void GLES2_ani_update(double now)
{
    if(!ani_is_running) return;

    ani->t_now += now - g_Time;
    g_Time      = now;
}

void GLES2_ani_fini( void )
{
    //texture_atlas_delete(atlas),  atlas  = NULL;
    //vertex_buffer_delete(buffer), buffer = NULL;
    if(shader_fx) glDeleteProgram(shader_fx), shader_fx = 0;
}

int ani_get_status(void)
{
    return ani_is_running;
}

void ani_reset(void)
{
    ani_is_running = false;
}

static void add_new_queue_item(NOTI_LEVELS lvl, std::string message, std::string detail) {

    struct _ent tmp_cv;
    // fname 1 is message, 2 is detail
    tmp_cv.fname = message;
    tmp_cv.fname2 = detail;
    // Message level
    tmp_cv.lvl = lvl;
    tmp_cv.sz = tmp_cv.fname.length() + tmp_cv.fname2.length() + 1;
    // Append current queue
    cv.push_back(tmp_cv);
    // if controller dc msg, don't print alld etails
    if(message != getLangSTR(CON_DIS)){
       fmt::print("[ANI][NOTIFY] Message: {}", tmp_cv.fname);
       fmt::print("[ANI][NOTIFY] Detail: {}", tmp_cv.fname2);
       fmt::print("[ANI][NOTIFY] MSG LVL: {}", tmp_cv.lvl);
       fmt::print("[ANI][NOTIFY] Added to the Queue #{0:d} in line", cv.size());
    }
    else
      log_info("Pad DC'ed");
}
// trigger start if not already flagged running
int ani_notify(NOTI_LEVELS lvl, std::string message, std::string detail)
{
    // Check if message is empty
    if ((message.empty() && detail.empty()) || lvl > NUM_OF_NOTIS) {
        log_info("[ANI][NOTIFY] Message is EINVAL");
        return ITEMZCORE_EINVAL;
    }

    pthread_mutex_lock(&notifcation_lock);

    int QueueTotal = cv.size();

    if (QueueTotal > MAX_QUEUE) {
        log_info("[ANI][NOTIFY] Queue is full %i", cv.size());
        if (QueueTotal < MAX_MESSAGES) {
          //Fill new Queue item
          add_new_queue_item(NOTIFI_ERROR, "Queue is Full", "Full");
        }
        else
            log_info("[ANI][NOTIFY] MAX_MESSAGES reached!");

        pthread_mutex_unlock(&notifcation_lock);
        return ITEMZCORE_QUEUE_FULL;
    }

    add_new_queue_item(lvl, message, detail);

    pthread_mutex_unlock(&notifcation_lock);

    return ITEMZCORE_SUCCESS;
}

// draw one effect loop
void GLES2_ani_draw(void)
{

    pthread_mutex_lock(&notifcation_lock);

    uint32_t entCt = cv.size();

    if (vbo && ani_is_running) {
        render_ani(level, 1, 1);
    }
    else {

        if (last_num >= entCt) {
            last_num = 0;
            cv.clear();
        }
        else {
            log_info("deleting vbo");
            if (vbo) vertex_buffer_delete(vbo), vbo = NULL;
            ani_is_running = true;

            level = (NOTI_LEVELS)cv.at(last_num).lvl;

            if (!vbo) // build text message
            {
                max_width = 0;
                vbo = vertex_buffer_new("vertex:3f,tex_coord:2f,color:4f");
                texture_font_load_glyphs(sub_font, cv.at(last_num).fname.c_str()); // set textures, update 'tl'
                if (tl > max_width) max_width = tl;

                vec4 col = (1.); // full rgba white
                // text position, 1st line
                vec2 pen = (vec2){ 100, 905 };

                add_text(vbo, sub_font, cv.at(last_num).fname, &col, &pen);  // set vertexes
                if (!cv.at(last_num).fname2.empty()) // second line
                {
                    pen.x = 100,
                        pen.y -= 35;
                    texture_font_load_glyphs(main_font, cv.at(last_num).fname2.c_str()); // set textures, update 'tl'
                    if (tl > max_width) max_width = tl;

                    add_text(vbo, main_font, cv.at(last_num).fname2, &col, &pen);  // set vertexes
                }
                refresh_atlas();
                last_num++;
            }
        }
    }

    pthread_mutex_unlock(&notifcation_lock);
}

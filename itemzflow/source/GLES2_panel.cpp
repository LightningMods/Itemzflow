/*
    GLES2 scene

    my implementation of an interactive menu, from scratch.
    (shaders
     beware)

    2020, masterzorag
*/

#include <stdio.h>
#include <string.h>

#include "defines.h"
#if defined(__ORBIS__)
#include "shaders.h"
#else
#include "pc_shaders.h"
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif
#include "GLES2_common.h"

extern char selected_text[64];
extern GLuint glsl_Program[4];
extern vec2 resolution;
extern int v_curr;
extern GLuint bg_tex;
/* common text */

extern GLuint debug_icon;

static VertexBuffer t_vbo; // for sysinfo

vec3 clk = (vec3){ 0., 0, 15 }; // timer(now, prev, limit in seconds)

// disk free percentages
double dfp_hdd  = -1.,
       dfp_ext  = -1.,
       dfp_fmem = -1.;

static std::atomic_bool refresh_clk(true);
/* upper right sytem_info, updates
   internally each 'clk.z' seconds */
#define SHOW_FMEM 0

void switchTextPositions(vec2 &pen, vec2 origin) {
    vec4 c = col * .75f;
    uint32_t numb = 70;
    size_t fmem = 1000000;
    std::string tmp;

    // Define the 4 screen corners for 1920x1080 screen size
        vec2 corners[4] = {
        { 26, 150 },
        { 1400, 150 },
        { 26, 990 },
        { 1400, 990 }
    };

    // Get the current time
    time_t currentTime = time(0);
    // Calculate the number of 5 minute intervals that have passed since the program started
    int intervals = currentTime / (5 * 60);
    // Use the remainder of the above calculation to determine the current corner
    int currentCorner = intervals % 4;

    // Set the pen position to the current corner
    pen = corners[currentCorner];
    log_debug("Current corner: %d", currentCorner);

    // Add text to the current corner
    tmp = fmt::format("{0:} {1:x}, {2:d}°C, {3:x}", getLangSTR(LANG_STR::SYS_VER), ps4_fw_version(), numb, fmem);
    // we need to know Text_Length_in_px in advance, so we call this:
    texture_font_load_glyphs( main_font, tmp.c_str() ); 
    // we know 'tl' now, right align
    pen.y -= 32;
    pen.x = corners[currentCorner].x;
    // fill the vbo
    t_vbo.add_text( main_font, tmp, c, pen);
    tmp = fmt::format("{0:}: {1:}", getLangSTR(LANG_STR::ITEMZ_VER), completeVersion);
    // we need to know Text_Length_in_px in advance, so we call this:
    texture_font_load_glyphs( main_font, tmp.c_str() ); 
    // we know 'tl' now, right align
    pen.y -= 32;
    pen.x = corners[currentCorner].x;
    // fill the vbo
    t_vbo.add_text( main_font, tmp, c, pen);
    pen.y -= 32;
    pen.x = corners[currentCorner].x;
    texture_font_load_glyphs(sub_font, get->setting_strings[THEME_NAME].c_str());
    if(currentCorner == 3 || currentCorner == 1){
        int detail_size = get->setting_strings[THEME_NAME].size() + get->setting_strings[THEME_AUTHOR].size() + get->setting_strings[THEME_VERSION].size();
        if(detail_size > 35)
           pen.x -= tl;
    }
    t_vbo.add_text( sub_font, get->setting_strings[THEME_NAME], c, pen);
    texture_font_load_glyphs(sub_font, get->setting_strings[THEME_AUTHOR].c_str());
    pen.x += 12;
    t_vbo.add_text( sub_font, get->setting_strings[THEME_AUTHOR], c, pen);
    texture_font_load_glyphs(sub_font, get->setting_strings[THEME_VERSION].c_str());
    pen.x += 12;
    t_vbo.add_text( sub_font, get->setting_strings[THEME_VERSION], c, pen);
    pen.y -= 40;
    pen.x = corners[currentCorner].x;

}
//float debug_x = 55.;
void GLES2_Draw_sysinfo(bool is_idle)
{  
    uint32_t numb = 70;
    size_t fmem = 1000000;
    bool is_file_manager = (v_curr == FILE_BROWSER_LEFT || v_curr == FILE_BROWSER_RIGHT);
    
    if (!is_idle && is_file_manager){
        if(right_page_pos < 10 && is_all_pkg_enabled(0)){
            render_button(debug_icon, 64., 64., 990., 860., 1);
            //log_debug("rendering debug icon");
        }
        if(left_page_pos < 10 && is_all_pkg_enabled(1)){
           render_button(debug_icon, 64., 64., 55., 860., 1);
           //log_debug("rendering debug icon");
        }
       // log_debug("left_page_pos = %d, right_page_pos = %d is_all_pkg_enabled %i", left_page_pos, right_page_pos, is_all_pkg_enabled(1));
    }
    std::string usb = fmt::format("/mnt/usb{}/", usbpath());
    std::string tmp;
    if(is_idle){
        /* background image, or pixelshader */
       if (bg_tex == GL_NULL && !get->setting_bools[has_image])
           pixelshader_render(); // use PS_symbols shader
       else{ /* bg image */
         vec4 r = (vec4){-1., -1., 1., 1.};
         // see if its even loaded
         if (bg_tex > GL_NULL)
            on_GLES2_Render_icon(USE_COLOR, bg_tex, 2, r, NULL);
       }
    }
    // update time
    clk.x += u_t - clk.y;
    clk.y  = u_t;
    // time limit passed!
    if(clk.x / clk.z > 1. || refresh_clk.load()) // by time or requested
    {   // destroy VBO
        t_vbo.clear();
        // reset clock
        clk.x = 0., refresh_clk = false;
    }

    if( ! t_vbo ) // refreshzzzz
    {   
        vec2 pen,
             origin   = resolution,
             border   = (vec2)(50.);
             t_vbo    = VertexBuffer( "vertex:3f,tex_coord:2f,color:4f" );
        // add border, in px
              origin -= border;
              pen     = origin;
        vec4 c = col * .75f;


        if (is_idle){
          switchTextPositions(pen, origin);
#if defined (__ORBIS__)
        std::string ip;
        if(!get_ip_address(ip)){
            texture_font_load_glyphs( sub_font, "0.0.0.0");
            t_vbo.add_text( sub_font, "0.0.0.0", col, pen);
        }
        else{
            texture_font_load_glyphs( sub_font, ip.c_str());
            t_vbo.add_text(sub_font, ip, col, pen);
        }
#else
        texture_font_load_glyphs( sub_font, "0.0.0.0");
        t_vbo.add_text( sub_font, "0.0.0.0", col, pen);
#endif
        refresh_atlas();
        return;
        }
        /* get systime */
        time_t     t  = time(NULL);
        struct tm *tm = localtime(&t);
        char s[65];
        strftime(s, sizeof(s), "%A, %B %e %Y, %H:%M", tm); // custom date string
//      log_info("%s", s);
        tmp = s;
        // we need to know Text_Length_in_px in advance, so we call this:
        texture_font_load_glyphs( sub_font, tmp.c_str() );
        // we know 'tl' now, right align
        pen.x -= tl;
        // fill the vbo
        t_vbo.add_text( sub_font, tmp, col, pen);
        //we dont want all the text on FS view
        if (!is_idle && is_file_manager){
            t_vbo.render_vbo(NULL);
            return;
        }

#if defined(__ORBIS__)
        sceKernelGetCpuTemperature(&numb);
        if (numb > 90)
            ani_notify(NOTIFI::WARNING, getLangSTR(LANG_STR::REALLY_HOT), getLangSTR(LANG_STR::REALLY_HOT_2));

        sceKernelAvailableFlexibleMemorySize(&fmem);
#endif
        tmp = fmt::format("{0:} {1:x}, {2:d}°C, {3:x}", getLangSTR(LANG_STR::SYS_VER), ps4_fw_version(), numb, fmem);
        // we need to know Text_Length_in_px in advance, so we call this:
        texture_font_load_glyphs( main_font, tmp.c_str() ); 
        // we know 'tl' now, right align
        pen.x  = origin.x - tl,
        pen.y -= 32;
        // fill the vbo
        t_vbo.add_text( main_font, tmp, c, pen);

        tmp = fmt::format("{0:}: {1:}", getLangSTR(LANG_STR::ITEMZ_VER), completeVersion);
        // we need to know Text_Length_in_px in advance, so we call this:
        texture_font_load_glyphs( main_font, tmp.c_str() ); 
        // we know 'tl' now, right align
        pen.x  = origin.x - tl,
        pen.y -= 32;
        // fill the vbo
        t_vbo.add_text( main_font, tmp, c, pen);
        // draw theme info below
        if (v_curr == ITEMzFLOW)
        {   
            /* text for disk_free stats */
            pen = (vec2){ 26, 100 };
            vec4 c = col * .75f;
            //
#if defined (__ORBIS__)
            if (usbpath() != -1)
            {   // get mountpoint stat info
                dfp_ext = df(usb, tmp);
                pen.y += 32;

            }
#else
            const char *homedir;

            if ((homedir = getenv("HOME")) == NULL) {
                homedir = getpwuid(getuid())->pw_dir;
            }
            dfp_ext = df(homedir, tmp);
            pen.y += 32;
#endif
            // fill the vbo
            t_vbo.add_text( sub_font, getLangSTR(LANG_STR::STORAGE), col, pen);
            // new line
            pen.x = 26,
            pen.y -= 32;

#if defined(__ORBIS__)
            if (usbpath() != -1)
#else
//log_vinfo("%s", tmp.c_str());
            if (if_exists(homedir))
#endif
                {
                    t_vbo.add_text( main_font, tmp, c, pen);
                    // a new line
                    pen.x = 26,
                    pen.y -= 32;
                }


                dfp_ext = df("/user",  tmp);
                t_vbo.add_text( main_font, tmp, c, pen);

        // update FMEM
        dfp_fmem = (1. - (double)fmem / (double)0x8000000) * 100.;
#ifdef __ORBIS__
        if(current_song.is_playing && !current_song.song_artist.empty()
         && !current_song.song_title.empty()){
            //draw music info on the right corner of a 1080p screen
            pen = (vec2){ 26, 20 };
            // fill the vbo
            t_vbo.add_text( main_font, fmt::format("{0:}{1:.35}{2:} - {3:.30}{4:} ({5:}/{6:})", (get->lang == 1 || get->lang == 18) ? "Playing: " : "", current_song.song_title, current_song.song_title.size() > 35 ? "..." : "" ,current_song.song_artist, current_song.song_artist.size() > 35 ? "..." : "", current_song.total_files > 1 ? current_song.current_file-1 : 1, current_song.total_files), col, pen);
        }
#endif

        // we eventually added glyphs... (todo: glyph cache)
        drop_some_coverbox();
        refresh_atlas();
        }
    }
    
if(!is_idle && v_curr == ITEMzFLOW){

//DrawWhiteStar();
//
#if SHOW_FMEM==1
    /* FMEM: draw filling color bar, by percentage */
    vec4 r = (vec4) { -.975, -.950,   -.505, -.955 };
    ORBIS_RenderFillRects(USE_COLOR, &grey, &r, 1);
    ORBIS_RenderDrawBox(USE_COLOR, &grey, &r);
    GLES2_DrawFillingRect(&r, &white, &dfp_fmem);
#else
    std::vector<vec4> r;
    r.push_back((vec4){-.975, -.900, -.505, -.905});
    // gles render the frect
    ORBIS_RenderFillRects(USE_COLOR, grey, r, 1);
    /* draw filling color bar, by percentage */
    GLES2_DrawFillingRect(r, sele, dfp_ext);
#endif
    }
    // texts out of VBO
    t_vbo.render_vbo(NULL);
}

void GLES2_refresh_sysinfo(void)
{   // will trigger t_vbo refresh (all happen in renderloop)
    refresh_clk = true;
}



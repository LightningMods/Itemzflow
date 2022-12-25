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
extern vec2 resolution;
extern int v_curr;
/* common text */

static vertex_buffer_t *t_vbo = NULL; // for sysinfo

vec3 clk = (vec3){ 0., 0, 15 }; // timer(now, prev, limit in seconds)

// disk free percentages
double dfp_hdd  = -1.,
       dfp_ext  = -1.,
       dfp_fmem = -1.;

static bool refresh_clk = true;
/* upper right sytem_info, updates
   internally each 'clk.z' seconds */
#define SHOW_FMEM 0
void GLES2_Draw_sysinfo(void)
{  
    uint32_t numb = 70;
    size_t fmem = 1000000;
    std::string usb = fmt::format("/mnt/usb{}/", usbpath());
    std::string tmp;
    // update time
    clk.x += u_t - clk.y;
    clk.y  = u_t;
    // time limit passed!
    if(clk.x / clk.z > 1. || refresh_clk) // by time or requested
    {   // destroy VBO
        if(t_vbo) vertex_buffer_delete(t_vbo), t_vbo = NULL;
        // reset clock
        clk.x = 0., refresh_clk = false;
    }

    if( ! t_vbo ) // refreshzzzz
    {   
        vec2 pen,
             origin   = resolution,
             border   = (vec2)(50.);
             t_vbo    = vertex_buffer_new( "vertex:3f,tex_coord:2f,color:4f" );
        // add border, in px
              origin -= border;
              pen     = origin;
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
        add_text( t_vbo, sub_font, tmp, &col, &pen);
        //we dont want all the text on FS view
        if (v_curr == FILE_BROWSER){
            if (t_vbo)
                ftgl_render_vbo(t_vbo, NULL);
            return;
        }

#if defined(__ORBIS__)
        sceKernelGetCpuTemperature(&numb);
        if (numb > 90)
            ani_notify(NOTIFI_WARNING, getLangSTR(REALLY_HOT), getLangSTR(REALLY_HOT_2));

        sceKernelAvailableFlexibleMemorySize(&fmem);
#endif
        tmp = fmt::format("{0:} {1:x}, {2:d}°C, {3:x}", getLangSTR(SYS_VER), ps4_fw_version(), numb, fmem);
        // we need to know Text_Length_in_px in advance, so we call this:
        texture_font_load_glyphs( main_font, tmp.c_str() ); 
        // we know 'tl' now, right align
        pen.x  = origin.x - tl,
        pen.y -= 32;
        vec4 c = col * .75f;
        // fill the vbo
        add_text( t_vbo, main_font, tmp, &c, &pen);

        tmp = fmt::format("{0:}: {1:}", getLangSTR(ITEMZ_VER), completeVersion);
        // we need to know Text_Length_in_px in advance, so we call this:
        texture_font_load_glyphs( main_font, tmp.c_str() ); 
        // we know 'tl' now, right align
        pen.x  = origin.x - tl,
        pen.y -= 32;
        // fill the vbo
        add_text( t_vbo, main_font, tmp, &c, &pen);
        // eventually, skip dfp on some view...

        // eventually, skip dfp on some view...
        if(v_curr == ITEMzFLOW)
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
#endif
            // fill the vbo
            add_text(t_vbo, sub_font, getLangSTR(STORAGE), &col, &pen);
            // new line
            pen.x = 26,
            pen.y -= 32;

#if defined(__ORBIS__)
            if (usbpath() != -1)
#else
//log_info("%s", tmp.c_str());
            if (if_exists(tmp.c_str()))
#endif
                {
                    add_text(t_vbo, main_font, tmp, &c, &pen);
                    // a new line
                    pen.x = 26,
                    pen.y -= 32;
                }


                dfp_ext = df("/user",  tmp);
                add_text(t_vbo, main_font, tmp, &c, &pen);

        // update FMEM
        dfp_fmem = (1. - (double)fmem / (double)0x8000000) * 100.;
#ifdef __ORBIS__
        if(current_song.is_playing && !current_song.song_artist.empty()
         && !current_song.song_title.empty()){
            //draw music info on the right corner of a 1080p screen
            pen = (vec2){ 26, 20 };
            // fill the vbo
            add_text(t_vbo, main_font, fmt::format("{0:}{1:.35}{2:} - {3:.30}{4:} ({5:}/{6:})", (get->lang == 1 || get->lang == 18) ? "Playing: " : "", current_song.song_title, current_song.song_title.size() > 35 ? "..." : "" ,current_song.song_artist, current_song.song_artist.size() > 35 ? "..." : "", current_song.total_files > 1 ? current_song.current_file-1 : 1, current_song.total_files), &col, &pen);
        }
#endif

        // we eventually added glyphs... (todo: glyph cache)
        drop_some_coverbox();
        refresh_atlas();
        }
    }
    
if(v_curr == ITEMzFLOW){
#if SHOW_FMEM==1
    /* FMEM: draw filling color bar, by percentage */
    vec4 r = (vec4) { -.975, -.950,   -.505, -.955 };
    ORBIS_RenderFillRects(USE_COLOR, &grey, &r, 1);
    ORBIS_RenderDrawBox(USE_COLOR, &grey, &r);
    GLES2_DrawFillingRect(&r, &white, &dfp_fmem);
#else
    vec4 r = (vec4){ -.975, -.900,   -.505, -.905 };
    // gles render the frect
    ORBIS_RenderFillRects(USE_COLOR, &grey, &r, 1);
    /* draw filling color bar, by percentage */
    GLES2_DrawFillingRect(&r, &sele, &dfp_ext);
#endif
    }
    // texts out of VBO
    if(t_vbo) ftgl_render_vbo(t_vbo, NULL);
}

void GLES2_refresh_sysinfo(void)
{   // will trigger t_vbo refresh (all happen in renderloop)
    refresh_clk = true;
}



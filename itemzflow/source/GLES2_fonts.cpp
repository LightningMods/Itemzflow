/*
    GLES2 menu

    2020, 2021, masterzorag
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "defines.h"
#ifdef __ORBIS__
#include "shaders.h"
#else
#include "pc_shaders.h"
#endif

// ft-gl: main, from demo-font.c
extern GLuint shader;
extern texture_atlas_t *atlas;
extern mat4             model, view, projection;
extern vec4 c[8], // palette
            col,  // default for texts
            grey,
            sele,
            white;

// to json-simple.c
texture_font_t *titl_font = NULL,
                *sub_font = NULL,
               *main_font = NULL;

// -> ftgl_draw_vbo()
static mat4 bak;
void ftgl_render_vbo( vertex_buffer_t *vbo, vec3 *offset )
{
    if(vbo == NULL) return;

    if(offset)
    {
        bak = view;
        const vec3 *o = offset;
        mat4_translate( &view, o->x, o->y, o->z );
    }

    glUseProgram( shader );
    glActiveTexture( GL_TEXTURE0 );
    glBindTexture  ( GL_TEXTURE_2D, atlas->id ); // rebind glyph atlas
    glEnable       ( GL_BLEND );
    glBlendFunc    ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glDisable      ( GL_CULL_FACE );
    {
        glUniform1i       ( glGetUniformLocation( shader, "texture" ),    0 );
        glUniformMatrix4fv( glGetUniformLocation( shader, "model" ),      1, 0, model.data);
        glUniformMatrix4fv( glGetUniformLocation( shader, "view" ),       1, 0, view.data);
        glUniformMatrix4fv( glGetUniformLocation( shader, "projection" ), 1, 0, projection.data);
        /* draw whole VBO item array */
        vertex_buffer_render( vbo, GL_TRIANGLES );
    }
    glDisable    ( GL_BLEND );
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram ( 0 );

    if(offset) // restore
    {
        view = bak;
    }
}


// font reloading
static void ftgl_purge_fonts( void )
{
    if(titl_font) texture_font_delete(titl_font);
    if( sub_font) texture_font_delete( sub_font);
    if(main_font) texture_font_delete(main_font);

    // discard cached glyphs, whole atlas from start
    if(atlas) texture_atlas_delete(atlas);
    //if(atlas->id) glDeleteTextures(1, &atlas->id), atlas->id = 0;
    /* renew atlas map */
    atlas = texture_atlas_new( 1024, 1024, 1 );
}

// smallest part
static texture_font_t *font_from_buffer(unsigned char *data, size_t size, int fontsize)
{
    return texture_font_new_from_memory(atlas, fontsize, data, size);
}

static bool ftgl_init_fonts( unsigned char *data, size_t size )
{
    bool  ret = 1;
    titl_font = font_from_buffer(data, size, 32);
     sub_font = font_from_buffer(data, size, 25);
    main_font = font_from_buffer(data, size, 18);

    if( ! titl_font
    ||  !  sub_font
    ||  ! main_font ) ret = 0;

    return ret;
}

// recreate fonts glyphs atlas
void refresh_atlas(void)
{
   // log_info( "* atlas to gpu *");
    if(1)
    {
        glUseProgram( shader );
        /* discard old texture, we eventually added glyphs! */
        if(atlas->id) glDeleteTextures(1, &atlas->id), atlas->id = 0;

        /* re-create texture and upload atlas into gpu memory */
        glGenTextures  ( 1, &atlas->id );
        glBindTexture  ( GL_TEXTURE_2D, atlas->id );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexImage2D   ( GL_TEXTURE_2D, 0, GL_ALPHA, atlas->width, atlas->height,
                                        0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas->data );
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram( 0 );
    }
}

void clean_textures(layout_t & l)
{
/*
    if(l.texture)
    {
        log_info("%s: layout: %p, %p", __FUNCTION__, l, l.texture);
        for(int i = 0; i < l.fieldsize.x * l.fieldsize.y; i++)
        {
            if(l.texture[i] != 0)
            {
                log_info("glDeleteTextures texture[%2d]: %d", i, l.texture[i]);
                glDeleteTextures(1, &l.texture[i] ), l.texture[i] = 0;
            }
        }
        free(l.texture), l.texture = NULL;
    }
*/
}

bool GLES2_fonts_from_ttf(const char *path)
{
    unsigned char *ttf = NULL;
    size_t size = 0;
    bool suc = true;

    if(path)
    {   /* load .ttf in memory */
        ttf  = orbisFileGetFileContent(path);
        size = _orbisFile_lastopenFile_size;
        if(!ttf)  {
            goto default_embedded;
        }
    } else {

default_embedded: // fallback on error
        suc = false;
        ttf  = (unsigned char *)&font_ttf[0];
        size =  font_len;
    }
    // recreate global glyph atlas
    ftgl_purge_fonts();

    // prepare our set of different size
    if( ! ftgl_init_fonts( ttf, size ) )
    {   // custom data failed...
#if defined (__ORBIS__)
        sceKernelIccSetBuzzer(3);
#endif
        // go back and fallback to default font
        goto default_embedded;
    }
    // done with ttf data, release (to recheck for fix)
   // free(ttf), ttf = NULL;
   return suc;
}

// init VBOs for menu texts, unused, old way
void GLES2_init_submenu( void )
{
    // load fonts once, don't delete them, but reuse!
    titl_font = texture_font_new_from_memory(atlas, 32, font_ttf, font_len);
     sub_font = texture_font_new_from_memory(atlas, 25, font_ttf, font_len);
    main_font = texture_font_new_from_memory(atlas, 18, font_ttf, font_len);
}


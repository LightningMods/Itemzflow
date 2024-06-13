/* ============================================================================
 * Freetype GL - A C OpenGL Freetype engine
 * Platform:    Any
 * WWW:         http://code.google.com/p/freetype-gl/
 * ----------------------------------------------------------------------------
 * Copyright 2011,2012 Nicolas P. Rougier. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NICOLAS P. ROUGIER ''AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL NICOLAS P. ROUGIER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Nicolas P. Rougier.
 * ============================================================================
 *
 * Example showing regular font usage
 *
 * ============================================================================
 */
#include <stdio.h>
#include <string.h>
#include <freetype-gl.h>  // links against libfreetype-gl
#include "defines.h"
#include "utils.h"

#if defined (__ORBIS__)

#include "shaders.h"

#else // on pc
#include "pc_shaders.h"
#endif



/* basic indexing of lines/texts */
GLuint num_of_texts = 0;  // text lines, or num of lines/how we split entire texts
/* shared freetype-gl function! */
void add_text( vertex_buffer_t * buffer, texture_font_t * font,
               const char * text, vec4 * color, vec2 * pen );

// ------------------------------------------------------- global variables ---
GLuint shader;

texture_atlas_t *atlas  = NULL;
vertex_buffer_t *buffer = NULL;
mat4             model, view, projection;

// ---------------------------------------------------------------- reshape ---
static void reshape(int width, int height)
{
    glViewport(0, 0, width, height);
    mat4_set_orthographic( &projection, 0, width, 0, height, -1, 1);
}


// --------------------------------------------------------------- add_text ---
void add_text( vertex_buffer_t * buffer, texture_font_t * font,
               const char * text, vec4 * color, vec2 * pen )
{
    if(!buffer)
       return; 

    size_t i;
    float r = color->r, g = color->g, b = color->b, a = color->a;

    for( i = 0; i < strlen(text); ++i )
    {
        texture_glyph_t *glyph = texture_font_get_glyph( font, text + i );
        if( glyph != NULL )
        {
            float kerning = 0.0f;
            if(i > 0)
            {
                kerning = texture_glyph_get_kerning( glyph, text + i - 1 );
            }
            pen->x  += kerning;
            int   x0 = (int)( pen->x + glyph->offset_x );
            int   y0 = (int)( pen->y + glyph->offset_y );
            int   x1 = (int)( x0 + glyph->width );
            int   y1 = (int)( y0 - glyph->height );
            float s0 = glyph->s0;
            float t0 = glyph->t0;
            float s1 = glyph->s1;
            float t1 = glyph->t1;
            GLuint indices[6] = {0,1,2,   0,2,3}; // (two triangles)
            /* VBO is setup as: "vertex:3f, tex_coord:2f, color:4f" */
            vertex_t vertices[4] = { { x0,y0,0,   s0,t0,   r,g,b,a },
                                     { x0,y1,0,   s0,t1,   r,g,b,a },
                                     { x1,y1,0,   s1,t1,   r,g,b,a },
                                     { x1,y0,0,   s1,t0,   r,g,b,a } };
            vertex_buffer_push_back( buffer, vertices, 4, indices, 6 );
            pen->x += glyph->advance_x;
        }
    }
}

// ------------------------------------------------------ freetype-gl shaders ---
static GLuint CreateProgram( void )
{
    GLuint programID = 0;
    /* we can use OrbisGl wrappers, or MiniAPI ones */
    /* else,
       include and links against MiniAPI library!
    programID = miniCompileShaders(s_vertex_shader_code, s_fragment_shader_code);
    */
    programID = BuildProgram(font_vs, font_fs, font_vs_length, font_fs_length);

    // feedback
    log_info( "program_id=%d (0x%08x)", programID, programID);

    return programID;
}


// freetype-gl pass last composed Text_Length in pixel, we use to align text!
extern float tl;
texture_font_t *font = NULL;
// ------------------------------------------------------------------- main ---
int es2init_text (int width, int height)
{
    /* init page: compose all texts to draw */
    atlas  = texture_atlas_new( 1024, 1024, 1 );

    /* create texture and upload atlas into gpu memory */
    glGenTextures  ( 1, &atlas->id );
    glBindTexture  ( GL_TEXTURE_2D, atlas->id );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexImage2D   ( GL_TEXTURE_2D, 0, GL_ALPHA, atlas->width, atlas->height,
                                    0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas->data );
    // compile, link and use shader
    shader = CreateProgram();
                                  
    if(!shader) 
        msgok(MSG_DIALOG::FATAL, fmt::format("{} {:x}", getLangSTR(LANG_STR::FAILED_W_CODE), shader));


    mat4_set_identity( &projection );
    mat4_set_identity( &model );
    mat4_set_identity( &view );

    reshape(width, height);

    return 0;
}

void es2sample_end( void )
{
    texture_atlas_delete(atlas),  atlas  = NULL;
    vertex_buffer_delete(buffer), buffer = NULL;

    if(shader) glDeleteProgram(shader), shader = 0;
}


/*    SAVE DATA CODE LIFTED FROM HERE
* ******************************************
* **https://github.com/bucanero/apollo-ps4**
* ******************************************
* ******************************************/
 /* Made with info from https://www.psdevwiki.com/ps4/Param.sfo. */
 /*https://github.com/hippie68/sfo. */

#if __has_include("<byteswap.h>")
#include <byteswap.h>
#else
// Replacement function for byteswap.h's bswap_32
uint32_t bswap_32(uint32_t val) {
  val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0x00FF00FF );
  return (val << 16) | (val >> 16);
}
#endif

// Complete param.sfo file structure, 4 parts:
// 1. header
// 2. all entries
// 3. key_table.content (with trailing 4-byte alignment)
// 4. data_table.content

using namespace multi_select_options;
struct header {
    uint32_t magic;
    uint32_t version;
    uint32_t key_table_offset;
    uint32_t data_table_offset;
    uint32_t entries_count;
};

header header;

struct index_table_entry {
    uint16_t key_offset;
    uint16_t param_fmt;
    uint32_t param_len;
    uint32_t param_max_len;
    uint32_t data_offset;
} *entries;

struct table {
    unsigned int size;
    char* content;
} key_table, data_table;

// Finds the param.sfo's offset inside a PS4 PKG file
long int get_ps4_pkg_offset(FILE* file) {
  uint32_t pkg_table_offset;
  uint32_t pkg_file_count;
  fseek(file, 0x00C, SEEK_SET);
  fread(&pkg_file_count, 4, 1, file);
  fseek(file, 0x018, SEEK_SET);
  fread(&pkg_table_offset, 4, 1, file);
  pkg_file_count = bswap_32(pkg_file_count);
  pkg_table_offset = bswap_32(pkg_table_offset);
  struct pkg_table_entry {
    uint32_t id;
    uint32_t filename_offset;
    uint32_t flags1;
    uint32_t flags2;
    uint32_t offset;
    uint32_t size;
    uint64_t padding;
  } pkg_table_entry[pkg_file_count];
  fseek(file, pkg_table_offset, SEEK_SET);
  fread(pkg_table_entry, sizeof (struct pkg_table_entry), pkg_file_count, file);
  for (int i = 0; i < pkg_file_count; i++) {
    if (pkg_table_entry[i].id == 1048576) { // param.sfo ID
      return bswap_32(pkg_table_entry[i].offset);
    }
  }
  log_error("[SFO] Could not find param.sfo in PKG file.");
  return -1;
}

bool load_header(FILE* file) {
    if (fread(&header, sizeof(struct header), 1, file) != 1) {
		log_error("[SFO] Could not read header.");
       return false;
    }
    return true;
}

bool load_entries(FILE* file) {
    unsigned int size = sizeof(struct index_table_entry) * header.entries_count;
    entries = (struct index_table_entry *)malloc(size);
    if (entries == NULL) {
	   log_error("[SFO] Could not allocate %u bytes of memory for index table.");
       return false;
    }
    if (size && fread(entries, size, 1, file) != 1) {
	   log_error("[SFO] Could not read index table entries.");
       return false;
    }
    return true;
}

bool load_key_table(FILE* file) {
    key_table.size = header.data_table_offset - header.key_table_offset;
    key_table.content = (char *)malloc(key_table.size);
    if (key_table.content == NULL) {
       log_error("[SFO] Could not allocate %u bytes of memory for key table.",
            key_table.size);
       return false;
    }
    if (key_table.size && fread(key_table.content, key_table.size, 1, file) != 1) {
		log_error("[SFO] Could not read key table.");
       return false;
    }
    return true;
}

bool load_data_table(FILE* file) {
    if (header.entries_count) {
        data_table.size =
            (entries[header.entries_count - 1].data_offset +
                entries[header.entries_count - 1].param_max_len);
    }
    else {
        data_table.size = 0; // For newly created, empty param.sfo files
    }
    data_table.content = (char*)malloc(data_table.size);
    if (data_table.content == NULL) {
       log_error("[SFO] Could not allocate %u bytes of memory for data table.",
            data_table.size);
       return false;
    }
    if (data_table.size && fread(data_table.content, data_table.size, 1, file) != 1) {
		log_error("[SFO] Could not read data table.");
       return false;
    }
    return true;
}

// Returns a parameter's index table position
int get_index(char* key) {
    for (int i = 0; i < header.entries_count; i++) {
        if (strcmp(key, &key_table.content[entries[i].key_offset]) == 0) {
            return i;
        }
    }
    return -1;
}

void sfo_mem_to_file(char *file_name) {
  FILE *file = fopen(file_name, "wb");
  if (file == NULL) {
    log_error("[SFO] Could not open file \"%s\" for writing.", file_name);
    return;
  }

  // Adjust header's table offsets before saving
  header.key_table_offset = sizeof(struct header) +
    sizeof(struct index_table_entry) * header.entries_count;
  header.data_table_offset = header.key_table_offset + key_table.size;

  if (fwrite(&header, sizeof(struct header), 1, file) != 1) {
    log_error("[SFO] Could not write header to file \"%s\".", file_name);
  }
  if (header.entries_count && fwrite(entries,
    sizeof(struct index_table_entry) * header.entries_count, 1, file) != 1) {
    log_error("[SFO] Could not write index table to file \"%s\".", file_name);
  }
  if (key_table.size && fwrite(key_table.content, key_table.size, 1, file) != 1) {
    log_error("[SFO] Could not write key table to file \"%s\".", file_name);
  }
  if (data_table.size && fwrite(data_table.content, data_table.size, 1, file) != 1) {
    log_error("[SFO] Could not write data table to file \"%s\".", file_name);
  }

  fclose(file);
}

bool extract_sfo_from_pkg(const char* pkg, const char* outdir){
    // CAUTION! This function assumes you checked the pkg magic already
    unlink(outdir); 
    FILE* file = fopen(pkg, "rb"); // TODO: use Open for both next
    if (file == NULL) {
        log_error("Could not open file %s", pkg);
        return false;
    }

     // Get SFO header offset
    fseek(file, get_ps4_pkg_offset(file), SEEK_SET);


    // Load file contents
    if(!load_header(file)){
        log_error("Could not load header");
        fclose(file);
        return false;
    }
    if(!load_entries(file)){
        log_error("Could not load entries table");
        fclose(file);
        return false;
    }
     if(!load_key_table(file)){
        log_error("Could not load data table");
        fclose(file);
        return false;
    }
    if(!load_data_table(file)){
        log_error("Could not load data table");
        fclose(file);
        return false;
    }

    fclose(file);
    
    sfo_mem_to_file((char*)outdir);

    if (entries) free(entries);
    if (key_table.content) free(key_table.content);
    if (data_table.content) free(data_table.content);

    return true;
}

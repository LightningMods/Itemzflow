#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <GLES2/gl2.h>
#ifdef __ORBIS__
#include <user_mem.h> 
#endif
#include "defines.h"

/////// png
#include <png.h>
#include <string.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/// png format
GLuint load_texture(
    const GLsizei width, const GLsizei height,
    const GLenum  type,  const GLvoid *pixels)
{
    // create new OpenGL texture
    GLuint texture_object_id;
    glGenTextures(1, &texture_object_id);
    glBindTexture(GL_TEXTURE_2D, texture_object_id);
    // set texture filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // generate texture from bitmap data

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    // don't create MipMaps, we must assert npot2!
    // glGenerateMipmap(GL_TEXTURE_2D);
    // release and return texture
    glBindTexture(GL_TEXTURE_2D, 0);

//    log_info("width: %d, height: %d, type: %d, pixels: %p", width, height, type, pixels);

    return texture_object_id;
}
/* externed */ 
vec2 tex_size; // last loaded png size as (w, h)
/// textures
GLuint load_png_asset_into_texture(const char* relative_path)
{  
    int w = 0, h = 0, comp= 0;
    unsigned char* image = stbi_load(relative_path, &w, &h, &comp, STBI_rgb_alpha);
    if(image == nullptr){
        log_debug( "%s(%s) FAILED!", __FUNCTION__, relative_path);
        return 0;
    }

    GLuint tex = load_texture(w, h, comp, image);
    log_info("loaded %s(%s) as %d", __FUNCTION__, relative_path, tex);
    stbi_image_free(image);
    return tex;
}

void load_png_cover_data_into_texture(struct AppIcon::ImageData& data, std::atomic<bool>& needs_loaded, std::atomic<GLuint>& tex)
{
    if (!data.data) {
        log_debug("%s(%p) FAILED!", __FUNCTION__, data.data.get());
        return;
    }

    tex = load_texture(data.w, data.h, data.comp, data.data.get());
    log_info("loaded %s(%p) as %d", __FUNCTION__, data.data.get(), tex.load());

    data.data.reset();  // Release the unique_ptr, freeing the memory
    needs_loaded = false;
}

GLuint load_png_data_into_texture(const char* data, int size)
{
    int w = 0, h = 0, comp = 0;
    unsigned char* image = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(data), size, &w, &h, &comp, STBI_rgb_alpha);
    if (image == nullptr) {
        log_debug("%s(data, %d) FAILED!", __FUNCTION__, size);
        return 0;
    }

    GLuint tex = load_texture(w, h, comp, image);
    log_info("loaded %s(data, %d) as %d", __FUNCTION__, size, tex);
    stbi_image_free(image);
    return tex;
}
// with int (u32)
void setRGB(png_byte *ptr, int *val)
{
    unsigned char *c = (unsigned char *)val;

    ptr[0] = *c++; ptr[1] = *c++; ptr[2] = *c++;
}

int writeImage(char* filename, int width, int height, int *buffer, const char* title)
{
    int code = 0;
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep row = NULL;

    // Open file for writing (binary mode)
    fp = fopen(filename, "wb");
    if (fp == NULL) { log_error( "Could not open file %s for writing", filename); code = 1; goto finalise; }

    // Initialize write structure
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) { log_error( "Could not allocate write struct"); code = 1; goto finalise; }
    // Initialize info structure
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) { log_error( "Could not allocate info struct"); code = 1; goto finalise; }
    // Setup Exception handling
    if (setjmp(png_jmpbuf(png_ptr))) { log_error( "Error during png creation"); code = 1; goto finalise; }

    png_init_io(png_ptr, fp);
    // Write header (8 bit colour depth)
    png_set_IHDR(png_ptr, info_ptr, width, height,
            8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    // Set title
    if (title != NULL) {
        png_text title_text;
        title_text.compression = PNG_TEXT_COMPRESSION_NONE;
        title_text.key = (char*)"Title";
        title_text.text = (char*) title;
        png_set_text(png_ptr, info_ptr, &title_text, 1);
    }
    png_write_info(png_ptr, info_ptr);

    // Allocate memory for one row (3 bytes per pixel - RGB)
    row = (png_bytep)calloc(3 * width +1, sizeof(png_byte));

    // Write image data
    int x, y;
    // note we flip vertically here!
    for (y=height ; y>-1 ; y--) {
        for (x=0 ; x<width ; x++) {
            setRGB(&(row[ x*3 ]), &buffer[y * width + x]); // converts RGBA to RGB
        }
        png_write_row(png_ptr, row);
    }
    // End write
    png_write_end(png_ptr, NULL);

finalise:

    if (fp) fclose(fp);
    if (info_ptr) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    if (png_ptr) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
    if (row) free(row);

    return code;
}


bool is_png_vaild(const char *relative_path, int *width, int *height) {
    // Load the image with stb_image
    int channels;
    unsigned char *data = stbi_load(relative_path, width, height, &channels, 0);
    // Check if the image was loaded successfully
    if (data) {
        stbi_image_free(data);
        return true;
    } else {
        return false;
    }
}

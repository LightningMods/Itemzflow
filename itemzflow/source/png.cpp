#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <GLES2/gl2.h>
#include "defines.h"

/////// png
#include <png.h>
#include <string.h>
#include <stdlib.h>
#include "GLES2_common.h"

#if __cplusplus
extern "C" {
#endif

typedef struct {
    const png_byte* data;
    const png_size_t size;
} DataHandle;

typedef struct {
    const DataHandle data;
    png_size_t offset;
} ReadDataHandle;

typedef struct {
    const png_uint_32 width;
    const png_uint_32 height;
    const int color_type;
} PngInfo;

/// helpers
/*static void read_png_data_callback(
    png_structp png_ptr, png_byte* png_data, png_size_t read_length);
static PngInfo read_and_update_info(const png_structp png_ptr, const png_infop info_ptr);
static DataHandle read_entire_png_image(
    const png_structp png_ptr, const png_infop info_ptr, const png_uint_32 height);
static GLenum get_gl_color_format(const int png_color_format);
*/
static PngInfo read_and_update_info(const png_structp png_ptr, const png_infop info_ptr)
{
    png_uint_32 width, height;
    int bit_depth, color_type;

    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(
        png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    // Convert transparency to full alpha
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    // Convert grayscale, if needed.
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    // Convert paletted images, if needed.
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    // Add alpha channel, if there is none.
    // Rationale: GL_RGBA is faster than GL_RGB on many GPUs)
    if (color_type == PNG_COLOR_TYPE_PALETTE || color_type == PNG_COLOR_TYPE_RGB)
        png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);

    // Ensure 8-bit packing
    if (bit_depth < 8)
        png_set_packing(png_ptr);
    else if (bit_depth == 16)
        png_set_scale_16(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    // Read the new color type after updates have been made.
    color_type = png_get_color_type(png_ptr, info_ptr);

    return (PngInfo) { width, height, color_type };
}

static void read_png_data_callback(
    png_structp png_ptr, png_byte* raw_data, png_size_t read_length) {
    ReadDataHandle* handle = (ReadDataHandle *)png_get_io_ptr(png_ptr);
    const png_byte* png_src = handle->data.data + handle->offset;

    memcpy(raw_data, png_src, read_length);
    handle->offset += read_length;
}

static DataHandle read_entire_png_image(
    const png_structp png_ptr,
    const png_infop info_ptr,
    const png_uint_32 height)
{
    const png_size_t row_size = png_get_rowbytes(png_ptr, info_ptr);
    const int data_length = row_size * (height + 1);
    assert(row_size > 0);

    png_byte* raw_image = (png_byte*)malloc(data_length);

    if (raw_image == NULL)
        log_error("%s %p", __FUNCTION__, raw_image);

    png_byte* row_ptrs[height];

    png_uint_32 i;
    for (i = 0; i < height; i++) {
        row_ptrs[i] = raw_image + i * row_size;
    }

    png_read_image(png_ptr, &row_ptrs[0]);

    return (DataHandle) { raw_image, data_length };
}

static GLenum get_gl_color_format(const int png_color_format) {
    assert(png_color_format == PNG_COLOR_TYPE_GRAY
        || png_color_format == PNG_COLOR_TYPE_RGB_ALPHA
        || png_color_format == PNG_COLOR_TYPE_GRAY_ALPHA);

    switch (png_color_format) {
    case PNG_COLOR_TYPE_GRAY:
        return GL_LUMINANCE;
    case PNG_COLOR_TYPE_RGB_ALPHA:
        return GL_RGBA;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
        return GL_LUMINANCE_ALPHA;
    }
    return 0;
}

/// main
RawImageData get_raw_image_data_from_png(const void* png_data, const int png_data_size, const char* fp)
{

    assert(png_data != NULL && png_data_size > 8);
    assert(png_check_sig((void*)png_data, 8));

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png_ptr != NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    assert(info_ptr != NULL);

    ReadDataHandle png_data_handle = (ReadDataHandle){ { (const png_byte*)png_data, png_data_size}, 0 };
    png_set_read_fn(png_ptr, &png_data_handle, read_png_data_callback);

    if (setjmp(png_jmpbuf(png_ptr))) {
        log_info("Error reading PNG file!");
        if(fp)
           unlink(fp);
        return (RawImageData) { 512, 512, 0, 0, NULL };
    }

    const PngInfo    png_info = read_and_update_info(png_ptr, info_ptr);
    const DataHandle raw_image = read_entire_png_image(png_ptr, info_ptr, png_info.height);

    png_read_end(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return (RawImageData) {
        png_info.width,
            png_info.height,
            raw_image.size,
            get_gl_color_format(png_info.color_type),
            raw_image.data
    };
}


void release_raw_image_data(const RawImageData* data)
{
    if (data != NULL)
    {
        if (data->data) free((void*)data->data);
    }
}


/// png format
GLuint load_texture(
    const GLsizei width, const GLsizei height,
    const GLenum  type, const GLvoid* pixels)
{
    // create new OpenGL texture
    GLuint texture_object_id;
    glGenTextures(1, &texture_object_id);
    glBindTexture(GL_TEXTURE_2D, texture_object_id);
    // set texture filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // generate texture from bitmap data
    glTexImage2D(GL_TEXTURE_2D, 0, type, width, height, 0, type, GL_UNSIGNED_BYTE, pixels);
    // don't create MipMaps, we must assert npot2!
    // glGenerateMipmap(GL_TEXTURE_2D);
    // release and return texture
    glBindTexture(GL_TEXTURE_2D, 0);

    return texture_object_id;
}

bool is_png_vaild(const char *relative_path, int* out_width, int* out_height)
{
    unsigned char *png_file = orbisFileGetFileContent(relative_path);
    // texture creation
    if (png_file != NULL && _orbisFile_lastopenFile_size > 0)
    {
        const RawImageData raw_image_data = get_raw_image_data_from_png(png_file, _orbisFile_lastopenFile_size, relative_path);
        if (raw_image_data.data == NULL)
        {
            // dont leak png_file
            log_error("PNG Is corrupted!");
            free((void *)png_file), png_file = NULL;
            return false;
        }
        // delete buffers & set the image dem.
        *out_width = raw_image_data.width;
        *out_height = raw_image_data.height;

        release_raw_image_data(&raw_image_data);
        // release_file_data(&png_file);
        free((void *)png_file), png_file = NULL;

        return true;
    }
    //else
      //  log_debug("%s(%s) returned INVALID!", __FUNCTION__, relative_path);

    // no texture
    return false;
}

/* externed */ vec2 tex_size; // last loaded png size as (w, h)
/// textures
GLuint load_png_asset_into_texture(const char* relative_path)
{
    // const FileData     png_file       = get_file_data(relative_path);
    // const RawImageData raw_image_data = get_raw_image_data_from_png(png_file.data, png_file.data_length);
#if defined (USE_NFS)
    const unsigned char* png_file = orbisNfsGetFileContent(relative_path);
#else
    const unsigned char* png_file = orbisFileGetFileContent(relative_path);
#endif
retry:
    // texture creation
    if (png_file != NULL && _orbisFile_lastopenFile_size > 8)
    {
        const RawImageData raw_image_data = get_raw_image_data_from_png(png_file, _orbisFile_lastopenFile_size, relative_path);
        if (raw_image_data.data == NULL) {
            //dont leak png_file
            free((void*)png_file), png_file = NULL;
#if defined(__ORBIS__)

            if (if_exists("/user/appmeta/" ITEMZFLOW_TID "/icon0.png"))
                png_file = orbisFileGetFileContent("/user/appmeta/" ITEMZFLOW_TID "/icon0.png");
#endif

            goto retry;
        }
        const GLuint    texture_object_id = load_texture(raw_image_data.width, raw_image_data.height,
            raw_image_data.gl_color_format,
            raw_image_data.data);
        // take note of image resolution size to setup VBO in px size
        tex_size = (vec2){ raw_image_data.width, raw_image_data.height };
        // delete buffers
        release_raw_image_data(&raw_image_data);
        //release_file_data(&png_file);
        free((void*)png_file), png_file = NULL;

        //log_debug( "%s(%s) ret: %d", __FUNCTION__, relative_path, texture_object_id);

        return texture_object_id;
    }
    else
        log_debug("%s(%s) FAILED!", __FUNCTION__, relative_path);

    // no texture
    return 0;
}
/// textures
GLuint load_png_data_into_texture(const char* data, int size)
{

    // texture creation
    if (size > 0)
    {
        const RawImageData raw_image_data = get_raw_image_data_from_png(data, size, NULL);

        const GLuint    texture_object_id = load_texture(raw_image_data.width, raw_image_data.height, raw_image_data.gl_color_format, raw_image_data.data);

        // take note of image resolution size to setup VBO in px size
        tex_size = (vec2){ raw_image_data.width, raw_image_data.height };
        // delete buffers
        release_raw_image_data(&raw_image_data);

        //log_debug( "%s(%s) ret: %d", __FUNCTION__, relative_path, texture_object_id);

        return texture_object_id;
    }
    else
        log_debug("%s(%s) FAILED!", __FUNCTION__, "EMBEDDED DATA");

    // no texture
    return 0;
}


// with int (u32)
void setRGB(png_byte* ptr, int* val)
{
    unsigned char* c = (unsigned char*)val;

    ptr[0] = *c++; ptr[1] = *c++; ptr[2] = *c++;
}

int writeImage(char* filename, int width, int height, int* buffer, char* title)
{
    int code = 0;
    FILE* fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep row = NULL;

    // Open file for writing (binary mode)
    fp = fopen(filename, "wb");
    if (fp == NULL) { log_error("Could not open file %s for writing", filename); code = 1; goto finalise; }

    // Initialize write structure
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) { log_error("Could not allocate write struct"); code = 1; goto finalise; }
    // Initialize info structure
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) { log_error("Could not allocate info struct"); code = 1; goto finalise; }
    // Setup Exception handling
    if (setjmp(png_jmpbuf(png_ptr))) { log_error("Error during png creation"); code = 1; goto finalise; }

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
        title_text.text = title;
        png_set_text(png_ptr, info_ptr, &title_text, 1);
    }
    png_write_info(png_ptr, info_ptr);

    // Allocate memory for one row (3 bytes per pixel - RGB)
    row = (png_bytep)calloc(3 * width + 1, sizeof(png_byte));

    // Write image data
    int x, y;
    // note we flip vertically here!
    for (y = height; y > -1; y--) {
        for (x = 0; x < width; x++) {
            setRGB(&(row[x * 3]), &buffer[y * width + x]); // converts RGBA to RGB
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
#if __cplusplus
}
#endif

#pragma once

#ifndef SHADERS_H_INCLUDED
#define SHADERS_H_INCLUDED

//SMALL EMBEDDED FONT
#include "font.h"
//ITEMZFLOW COVER TEMPLATE
//#include "shaders/cover_template.h"

#define MAX_SL_PROGRAMS  (3)

#ifndef HAVE_SHADER_COMPILER

#include "shaders/coverflow_vs.essl.h"
#include "shaders/coverflow_fs.essl.h"
#include "shaders/ani_vs.essl.h"
#include "shaders/ani_fs.essl.h"
#include "shaders/rects_vs.essl.h"
#include "shaders/rects_fs.essl.h"
#include "shaders/icons_vs.essl.h"
#include "shaders/icons_fs.essl.h"
#include "shaders/font_vs.essl.h"
#include "shaders/font_fs.essl.h"
#include "shaders/p_fs.essl.h"
#include "shaders/p_vs.essl.h"

#else

#error "Comment this out if you really have your OWN Shader Compiler, These are NOT Compiled and are Plaintext"

#include "shaders/GLSL/coverflow_vs.essl"
#include "shaders/GLSL/coverflow_fs.essl"
#include "shaders/GLSL/ani_vs.essl"
#include "shaders/GLSL/ani_fs.essl"
#include "shaders/GLSL/rects_vs.essl"
#include "shaders/GLSL/rects_fs.essl"
#include "shaders/GLSL/icons_vs.essl"
#include "shaders/GLSL/icons_fs.essl"
#include "shaders/GLSL/font_vs.essl"
#include "shaders/GLSL/font_fs.essl"
#include "shaders/GLSL/p_fs.essl"
#include "shaders/GLSL/p_vs.essl"

#endif
/*
  implement some way to setup different kind of
  animations to use in print text with FreeType
  scope: reusing and sharing resources from the
  already available freetype-gl library
  2020, masterzorag
*/
//#include <freetype-gl.h>


// -------------------------------------------------------------- effects ---


/* each fx have those states */
enum ani_states
{
    ANI_CLOSED,
    ANI_IN,
    ANI_DEFAULT,
    ANI_OUT
};

enum ani_type_num
{
    TYPE_0,
    TYPE_1,
    TYPE_2,
    TYPE_3,
    MAX_ANI_TYPE
};

/* hold the current state values */
typedef struct
{
    // GLuint program;
    int   status, // current ani_status
        fcount; // current framecount (depr.)

    float t_now,   // current time
        t_life;  // total duration
} fx_entry_t;

static fx_entry_t fx_entry[MAX_ANI_TYPE];


#endif

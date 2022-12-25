/*
    mimic an SDL_Rect

    standard GLES2 code to DrawLines and FillRects

    2020, masterzorag
*/

#include <defines.h>
#if defined(__ORBIS__)
#include "shaders.h"
#else
#include "pc_shaders.h"
#endif

#include <GLES2_common.h>
/* glsl programs */
static GLuint glsl_Program[3];
static GLuint curr_Program;  // the current one

extern vec2   resolution;
static vec4   color = { 1., 0., .5, 1. }; // current RGBA color
// shaders locations
static GLint  a_position_location;
static GLint  u_color_location;
static GLint  u_time_location;
 // from main.c
extern double u_t;

/* translate px vector to normalized coord */
vec2 px_pos_to_normalized(vec2 *pos)
{
    return 2. / resolution * (*pos) - 1.;
}

/*================= RECT SHADERS  =================================*/

void ORBIS_RenderFillRects_init(int width, int height)
{
    resolution = (vec2){(float)width, (float)height};

    curr_Program = glsl_Program[0] = BuildProgram(rects_vs0, rects_fs0, rects_vs0_length, rects_fs0_length);
    curr_Program = glsl_Program[1] = BuildProgram(rects_vs1, rects_fs1, rects_vs1_length, rects_fs1_length);
    curr_Program = glsl_Program[2] = BuildProgram(rects_vs2, rects_fs2, rects_vs2_length, rects_fs2_length);

    for (int i = 0; i < 3; i++)
    {
        curr_Program = glsl_Program[i];

        glUseProgram(curr_Program);
        // gles2 attach shader locations
        a_position_location = glGetAttribLocation(curr_Program, "a_Position");
        u_color_location = glGetUniformLocation(curr_Program, "u_color");
        u_time_location = glGetUniformLocation(curr_Program, "u_time");
    }
    
    curr_Program = glsl_Program[0];
    // select for setup
    glUseProgram(curr_Program);
    // reshape
    glViewport(0, 0, width, height);
}
/* ================================= =========================*/


void ORBIS_RenderFillRects_fini(void)
{
    for(int i = 0; i < 3; ++i)
    {
        if (glsl_Program[i]) glDeleteProgram(glsl_Program[i]), glsl_Program[i] = 0;
    }
}


// takes point count
void ORBIS_RenderDrawLines(//SDL_Renderer *renderer,
    const vec2 *points, int count)
{
    GLfloat vertices[4];

    glUseProgram(curr_Program);
    // enable alpha
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* emit a line for each point pair */
    for (int i = 0; i < count; i += 2) {
#if 0
        log_info("%d, %d, %d %.3f,%.3f,%.3f,%.3f", 
            idx, count, count/2,
            points[idx   ].x, points[idx   ].y,
            points[idx +1].x, points[idx +1].y);
#endif
        /* (x, y) for 2 points: 4 vertices */
        vertices[0] = points[i   ].x;  vertices[1] = points[i   ].y;
        vertices[2] = points[i +1].x;  vertices[3] = points[i +1].y;
        /* each (vec2)point comes from pairs of floats */
        glVertexAttribPointer    (a_position_location, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        glEnableVertexAttribArray(a_position_location);
        /* write color to use to the shader location */
        glUniform4f(u_color_location, color.r, color.g, color.b, color.a);
        /* floats pairs for points from 0-4 */
        glDrawArrays(GL_LINES, 0, 2);
    }
    // revert state back
    glDisable(GL_BLEND);
    // release program
    glUseProgram(0);
}


/* draw a white box around selected rect */
void ORBIS_RenderDrawBox(enum SH_type SL_program, const vec4 *rgba, const vec4 *r)
{
    if(rgba) color = *rgba;
    // backup current color
    vec4 curr_color = color;
    // a box is 4 segments joining 2 points
    vec2 b[4 * 2];
    // 1 line for each 2 points
    b[0] = r->xy,  b[1] = r->xw;
    b[2] = r->xw,  b[3] = r->zw;
    b[4] = r->zw,  b[5] = r->zy;
    b[6] = r->zy,  b[7] = r->xy;
    // select shader to use
    curr_Program = glsl_Program[SL_program];
    // gles render all lines
    ORBIS_RenderDrawLines(&b[0], 8);
    // restore current color
    color = curr_color;
}

/* main function to draw FillRects */
void ORBIS_RenderFillRects(enum SH_type SL_program, const vec4 *rgba, const vec4 *rects, int count)
{
    if(rgba) color = *rgba;
    // (4 float pairs!)
    GLfloat vertices[8];
    // select shader to use
    curr_Program = glsl_Program[SL_program];

    glUseProgram(curr_Program);
    //glDisable(GL_CULL_FACE);
    // enable alpha
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* emit a triangle strip for each rectangle */
    for(int i = 0; i < count; ++i)
    {
        const vec4 *rect = &rects[i];

        GLfloat xMin = rect->x,  xMax = rect->z,
                yMin = rect->y,  yMax = rect->w;
        /* (x, y) for 4 points: 8 */
        vertices[0] = xMin;  vertices[1] = yMin;
        vertices[2] = xMax;  vertices[3] = yMin;
        vertices[4] = xMin;  vertices[5] = yMax;
        vertices[6] = xMax;  vertices[7] = yMax;
        /* each (vec2) point comes from pairs of floats */
        glVertexAttribPointer    (a_position_location, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        glEnableVertexAttribArray(a_position_location);
        /* write color to use to the shader location */
        glUniform4f(u_color_location, color.r, color.g, color.b, color.a);
        /* floats pairs for points from 0-4 */
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    // revert state back
    glDisable(GL_BLEND);
    // release VBO, texture, program, ...
    glUseProgram(0);
}

/* draw filling color bar */
void GLES2_DrawFillingRect(vec4* r, // float/normalized rectangle
    vec4* c, // normalized color
    double* percentage) // how much filled from left
{   // shrink frect RtoL by_percentage
    r->z = r->x + (r->z - r->x) * (*percentage / 100.f);
    //  log_info( "%.2f %.2f, %.2f, %.2f %f", r.x, r.y, r.z, r.w, dfp);
    ORBIS_RenderFillRects(USE_COLOR, c, r, 1);
}

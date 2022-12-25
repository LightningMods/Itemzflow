static const char ani_vs[] = 
"precision mediump float;\
uniform mat4 model;\
uniform mat4 view;\
uniform mat4 projection;\
uniform vec4 meta;\
attribute vec3 vertex;\
attribute vec2 tex_coord;\
attribute vec4 color;\
varying vec2  vTexCoord;\
varying vec4  fragColor;\
varying float frame;\
float t(float a)\
{\
    return mod(a, .01);\
}\
\
void main(void)\
{\
    vTexCoord.xy = tex_coord.xy;\
    fragColor = color;\
    vec4  offset = vec4(0.05, -0.02, 0., 0.15);\
    if (meta.w == .1) offset = vec4(0.15, 0., 0., 0.);\
    if (meta.w == .2) offset = vec4(0.0, 0.3, 0., 0.);\
    if (meta.w == .3) offset = vec4(0.0, 0.3, 0., 0.);\
    vec4  step = offset / meta.z;\
    frame = mod(meta.x, meta.z);\
    vec4  p1 = projection * (view * (model * vec4(vertex, 1.))),\
        p0 = p1 - offset;\
    if (meta.y >= .3)\
    {\
        p1 -= step * frame;\
        gl_Position = p1; return;\
    }\
    if (meta.y >= .2)\
    {\
        if (meta.w == .2) { p1.y += abs(sin(frame * .2) * .1); }\
        if (meta.w == .3) { p1.w += cos(frame * .2) * .05; }\
\
        gl_Position = p1; return;\
    }\
    if (meta.y >= .1)\
    {\
        p0 += step * frame;\
        gl_Position = p0; return;\
    }\
    if (meta.y >= .0) { return; }\
}";

static const int ani_vs_length = 0;

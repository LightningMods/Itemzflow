static const char ani_fs[] =
"precision mediump float;\
uniform sampler2D texture;\
uniform vec4      meta;\
varying vec2  vTexCoord;\
varying vec4  fragColor;\
varying float frame;\
void main(void)\
{\
    float a = texture2D(texture, vTexCoord).a;\
    float step = frame / meta.z;\
    vec4  c1 = vec4(fragColor.rgb, fragColor.a * a);\
    gl_FragColor = c1;\
    if (meta.y >= .3) \
    {\
        step = 1. - step; \
        gl_FragColor.a = clamp(step, 0., c1.a);\
        return;\
    }\
    if (meta.y >= .2)\
    {\
        if (meta.w == .2) { gl_FragColor.rgb = vec3(.7, .2, 4.); }\
        if (meta.w == .3) {\
            gl_FragColor.a = clamp(abs(sin(meta.x * 2.5)),\
                0., c1.a);\
        }\
        return;\
    }\
    if (meta.y >= .1)\
    {\
        gl_FragColor.a = clamp(step, 0., c1.a); return;\
    }\
    if (meta.y >= .0)\
    {\
        gl_FragColor.a = 0.; return;\
    }\
}";

static const int ani_fs_length = 0;

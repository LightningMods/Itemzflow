static const char font_vs[] =
       "precision mediump float; \
        uniform mat4 model; \
        uniform mat4 view; \
        uniform mat4 projection; \
        \
        attribute vec3 vertex; \
        attribute vec2 tex_coord; \
        attribute vec4 color; \
        \
        varying vec2 vTexCoord; \
        varying vec4 fragColor; \
        \
        void main() \
        { \
            vTexCoord.xy = tex_coord.xy; \
            fragColor    = color; \
            gl_Position  = projection*(view*(model*vec4(vertex,1.0))); \
        }";

static const int font_vs_length = 0;
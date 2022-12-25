
static const char coverflow_vs0[] =
       "precision mediump float; \
        uniform mat4 model; \
        uniform mat4 view; \
        uniform mat4 projection; \
        \
        uniform   vec4 Color;  \
        attribute vec3 vertex; \
        attribute vec3 normal; \
        attribute vec4 color;  \
        \
        varying   vec4 fragColor; \
        varying   vec3 norm; \
        \
        void main() \
        { \
            fragColor   = color*Color; \
            norm        = normal; \
            gl_Position = projection*(view*(model*vec4(vertex,1.0))); \
        }";

 static const char coverflow_vs1[] =
       "precision mediump float; \
        uniform mat4 model; \
        uniform mat4 view; \
        uniform mat4 projection; \
        \
        uniform   vec4 Color;  \
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
            fragColor    = Color; \
            gl_Position  = projection*(view*(model*vec4(vertex,1.0))); \
        }";


static const char coverflow_vs2[] =
       "precision mediump float; \
        uniform mat4 model; \
        uniform mat4 view; \
        uniform mat4 projection; \
        \
        uniform   vec4 Color;  \
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
            fragColor    = Color; \
            gl_Position  = projection*(view*(model*vec4(vertex,1.0))); \
 }";

// disc reflection
static const char coverflow_vs4[] =
       "precision mediump float; \
        uniform mat4 model; \
        uniform mat4 view; \
        uniform mat4 projection; \
        \
        uniform   vec4 Color;  \
        attribute vec3 vertex; \
        attribute vec2 tex_coord; \
        attribute vec4 color; \
        \
        varying vec2 vTexCoord; \
        varying vec4 fragColor; \
        varying float y; \
        \
        void main() \
        { \
            vTexCoord.xy = tex_coord.xy; \
            fragColor    = color*Color; \
            gl_Position  = projection*(view*(model*vec4(vertex,1.0))); \
            y            = gl_Position.y; \
        }";

static const int coverflow_vs0_length = 0;
static const int coverflow_vs1_length = 0;
static const int coverflow_vs2_length = 0;
static const int coverflow_vs4_length = 0;
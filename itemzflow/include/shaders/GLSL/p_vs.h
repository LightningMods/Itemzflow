static const char p_vs1[] =    
    "precision mediump float;"
    "uniform   mat4  model;"
    "uniform   mat4  view;"
    "uniform   mat4  projection;"
    ""
    "attribute vec3  vertex;"
    "attribute vec4  color;"
    "varying   vec4  fragColor;"
    ""
    "void main(void){"
    "    fragColor   = color;"
    "    gl_Position = projection*(view*(model*vec4(vertex,1.0)));"
    "}";

static const int p_vs1_length = 0;

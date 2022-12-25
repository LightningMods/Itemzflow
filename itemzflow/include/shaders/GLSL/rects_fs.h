
static const char rects_fs0[] = 
/// 1. use passed u_color
    "precision mediump float;"
    "varying   vec4    fragColor;"
    "uniform   float   u_time;"
    ""
    "void main(void)"
    "{"
    "  gl_FragColor    = fragColor;"
     "} "; 
static const char rects_fs1[] = 
    /// 2. USE_UTIME
    "precision mediump float;"
    "varying   vec4    fragColor;"
    "uniform   float   u_time;"
    ""
    "void main(void)"
    "{"
    "  gl_FragColor    = fragColor;"
    "  gl_FragColor.a *= abs(sin(u_time));"
    "} ";
 
static const char rects_fs2[] = 
  "precision mediump float;"
    "varying   vec4    fragColor;"
    "uniform   float   u_time;"
    ""
    "void main(void)"
    "{"
    "  gl_FragColor    = fragColor;"
    "  gl_FragColor.a *= abs(sin(u_time));"
    "} "; 

static const int rects_fs0_length = 0;
static const int rects_fs1_length = 0;
static const int rects_fs2_length = 0;

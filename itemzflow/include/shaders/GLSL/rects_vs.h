
static const char rects_vs0[] = 
    "precision mediump float;"
    "attribute vec4    a_Position;"
    "uniform   vec4    u_color;"
    "uniform   float   u_time;"
    // use your own output instead of gl_FrontColor
    "varying   vec4 fragColor;"
    ""
    "void main(void)"
    "{"
    "  fragColor    = u_color;"
    "  gl_Position  = a_Position;"
    "} ";

static const char rects_vs1[] = 
    /// 2. USE_UTIME
    "precision mediump float;"
    "attribute vec4    a_Position;"
    "uniform   vec4    u_color;"
    "uniform   float   u_time;"
    // use your own output instead of gl_FrontColor
    "varying   vec4 fragColor;"
    ""
    "void main(void)"
    "{"
    "  fragColor    = u_color;"
    "  gl_Position  = a_Position;"
    // apply a little of zooming
//    "  gl_Position.w -= ( abs(sin(u_time)) * .006 );"
    "} ";

static const char rects_vs2[] = 
    /// 3. 
    "precision mediump float;"
    "attribute vec4    a_Position;"
    "uniform   vec4    u_color;"
    "uniform   float   u_time;"
    // use your own output instead of gl_FrontColor
    "varying   vec4 fragColor;"
    ""
    "void main(void)"
    "{"
    "  fragColor    = u_color;"
    "  gl_Position  = a_Position;"
    // apply a little of zooming
    "  gl_Position.w -= ( abs(sin(u_time)) * .01 );"
    "} ";

static const int rects_vs0_length = 0;
static const int rects_vs1_length = 0;
static const int rects_vs2_length = 0;


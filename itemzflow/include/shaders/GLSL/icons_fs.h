static const char icons_fs0[] = 
"precision mediump float;"
"uniform   sampler2D u_TextureUnit;"
"varying   vec2      v_TextureCoordinates;"
"uniform   float     u_time;"
"void main(void)"
"{"
      "gl_FragColor = texture2D(u_TextureUnit, v_TextureCoordinates);"
"}";

static const char icons_fs1[] = 
"precision mediump float;"
"uniform   sampler2D u_TextureUnit;"
"varying   vec2      v_TextureCoordinates;"
"uniform   float     u_time;"
"void main(void)"
"{"
      "gl_FragColor = texture2D(u_TextureUnit, v_TextureCoordinates);"
      "gl_FragColor.a *= abs(sin(u_time));"
"}";

static const char icons_fs2[] = 
"precision mediump float;"
"uniform   sampler2D u_TextureUnit;"
"varying   vec2      v_TextureCoordinates;"
"uniform   float     u_time;"
"void main(void)"
"{"
      "gl_FragColor = texture2D(u_TextureUnit, v_TextureCoordinates);"
      "gl_FragColor.r += abs( cos(u_time *1.) * sin(u_time *.5) /4. );"
"}";

static const int icons_fs0_length = 0;
static const int icons_fs1_length = 0;
static const int icons_fs2_length = 0;

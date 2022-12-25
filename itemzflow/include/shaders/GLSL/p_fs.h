

static const char p_fs0[] = 
    "precision mediump float;"
    ""
    "uniform float time;"
    "uniform vec2  mouse;"
    "uniform vec2  resolution;"
    ""
    "void main( void )"
    "{"
    "   vec2 vu = ( 2.* gl_FragCoord.xy - resolution.xy ) / resolution.y;"
    "   float c = 0.0;"
    "   for(float i = 0.; i < 1.; i++){"
    "     c += abs( 1. / sin( vu.y + 0.132 + sin( i * .0103 + vu.x * 3.3 + time * .30003 -i * .134 ) * cos( vu.x + time * .10001 ) * sin( vu.x + time * .5000001 + i * .12) * cos( i * .041 )  ) );"
    "   }"
    "   vec3 kek = vec3(0.5,0.5,0.5) / dot(vu, vu) / 6.0;"
    "   float x = kek.r / (kek.r + 1.);"
    "   gl_FragColor = vec4(0.);"
    "   gl_FragColor.a = 1. - x;"
    "}";

static const char p_fs1[] = 
"precision mediump float;\
    void main( void )\
    {\
        \
    }";

static const char p_fs2[] = 
    "precision mediump  float;"
    "uniform   vec2     resolution;"
    "uniform   vec2     mouse;"
    "uniform   float    time;"
    ""
    "#define  time       time"
    "#define  resolution resolution"
    "#define  iMouse      mouse"
    ""
    "void main( void )"
    "{"
    "   vec2 uv      = gl_FragCoord.xy / resolution.xy;"
    "   vec3 col     = 0.5 + 0.5 * cos( time+uv.xyx + vec3(0, 2, 4));"
    "   gl_FragColor = vec4(col, 1.0);"
    "}";

static const char p_fs3[] =  
"precision mediump float;"
    "uniform   float   time;"
    "uniform   vec2    resolution;"
    "uniform   vec2    mouse;"
    ""
    "#define speed 0.3"
    "#define freq  0.8"
    "#define amp   0.9"
    "#define phase 2.5"
    ""
    "void main( void ) {"
    "   vec2  p   = ( gl_FragCoord.xy / resolution.xy ) - 2. * (1. - mouse.y);"
    "   float sx  = (amp) *1.9 * sin( 4.0 * (freq) * (p.x-phase) - 6.0 * (speed)*time);"
    "   float dy  = 43./ ( 60. * abs(4.9*p.y - sx - 1.2));"
    "         dy +=  1./ ( 60. * length(p - vec2(p.x, 0.)));"
    "   gl_FragColor = vec4( (p.x + 0.05) * dy, 0.2 * dy, dy, 2.0 );"
    "}";
static
const char p_fs5[] =
"precision mediump float; \
uniform float time; \
uniform vec2 resolution; \
float intensity = 1.0; \
float radius = 0.1; \
float triangleDist(vec2 p){  \
    const float k = sqrt(3.0); \
    p.x = abs(p.x) - 1.0; \
    p.y = p.y + 1.0/k; \
    if( p.x+k*p.y>0.0 ) p=vec2(p.x-k*p.y,-k*p.x-p.y)/2.0; \
    p.x -= clamp( p.x, -2.0, 0.0 ); \
    return -length(p)*sign(p.y); \
} \
float boxDist(vec2 p){ \
    vec2 d = abs(p)-1.0; \
    return length(max(d,vec2(0))) + min(max(d.x,d.y),0.0); \
} \
float crossDist(vec2 p) {   \
    vec2  d = abs(p)-1.0; \
    float x = length(p) - 1.; \
	return max(x, length(d.x - d.y)); \
} \
float circleDist(vec2 p) {\
	\
		return length(p) - 1.0; \
} \
float getGlow(float dist, float radius, float intensity) {\
		\
			return pow(radius / dist, intensity); \
	} \
		void mainImage(out vec4 fragColor, in vec2 fragCoord) {\
			\
				vec2 uv = fragCoord / resolution.xy; \
				float widthHeightRatio = resolution.x / resolution.y; \
				vec2 centre; \
				vec2 pos; \
				float t = time * 0.025; \
				float dist; \
				float glow; \
				vec3 col = vec3(0); \
				const float scale = 500.0; \
				const float layers = 8.0; \
				float depth; \
				vec2 bend; \
				const vec3 purple = vec3(0.611, 0.129, 0.909); \
				const vec3 green = vec3(0.133, 0.62, 0.698); \
				float angle; \
				float rotationAngle; \
				mat2 rotation; \
				float d = 2.5 * (sin(t) + sin(3.0 * t)); \
				vec2 anchor = vec2(0.5 + cos(d), 0.5 + sin(d)); \
				pos = anchor - uv; \
				pos.y /= widthHeightRatio; \
				dist = length(pos); \
				glow = getGlow(dist, 0.35, 1.9); \
				col += glow * vec3(0.7, 0.6, 1.0); \
				for (float i = 0.0; i < layers; i++) {\
					\
						depth = fract(i / layers + t); \
						centre = vec2(0.5 + 0.2 * sin(t), 0.5 + 0.2 * cos(t)); \
						bend = mix(anchor, centre, depth); \
						pos = bend - uv; \
						pos.y /= widthHeightRatio; \
						rotationAngle = 3.14 * sin(depth + fract(t) * 6.28) + i; \
						rotation = mat2(cos(rotationAngle), -sin(rotationAngle), sin(rotationAngle), cos(rotationAngle)); \
						pos *= rotation; \
						pos *= mix(scale, 0.0, depth); \
						float m = mod(i, 4.0); \
						if (m == 0.0) {\
							\
								dist = abs(boxDist(pos)); \
						}\
						else if (m == 1.0) {\
							\
								dist = abs(triangleDist(pos)); \
						}\
						else if (m == 2.0) {\
							\
								dist = abs(crossDist(pos)); \
						}\
						else {\
							\
								dist = abs(circleDist(pos)); \
						} \
							glow = getGlow(dist, radius + (1.0 - depth) * 2.0, intensity + depth); \
								angle = (atan(pos.y, pos.x) + 3.14) / 6.28; \
								angle = abs((2.0 * fract(angle + i / layers)) - 1.0); \
								col += glow * mix(green, purple, angle);\
			}\
	fragColor = vec4(col, 1.0); \
} \
void main(void) \
{ \
mainImage(gl_FragColor, gl_FragCoord.xy); \
}";

static const int p_fs0_length = 0;
static const int p_fs1_length = 0;
static const int p_fs2_length = 0;
static const int p_fs3_length = 0;
static const int p_fs5_length = 0;

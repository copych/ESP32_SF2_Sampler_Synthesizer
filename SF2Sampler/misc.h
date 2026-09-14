/*
* Miscellaneous constants, macros and functions
* 
* Author: Evgeny Aslovskiy AKA Copych
* License: MIT
*/

#pragma once



// lookup tables
#define TABLE_BIT             5UL           // bits per index of lookup tables. 10 bit means 2^10 = 1024 samples. Values from 5 to 11 are OK. Choose the most appropriate.
#define TABLE_SIZE            (1<<TABLE_BIT)        // samples used for lookup tables (it works pretty well down to 32 samples due to linear approximation, so listen and free some memory)
#define TABLE_MASK            (TABLE_SIZE-1)        // strip MSB's and remain within our desired range of TABLE_SIZE
#define DIV_TABLE_SIZE        (1.0f/(float)TABLE_SIZE)
#define HALF_TABLE            TABLE_SIZE/2
#define QUARTER_TABLE         TABLE_SIZE/4
#define CYCLE_INDEX(i)        (((int32_t)(i)) & TABLE_MASK ) // this way we can operate with periodic functions or waveforms with auto-phase-reset ("if's" are pretty CPU-costly)
#define SHAPER_LOOKUP_MAX     5.0f                  // maximum X argument value for tanh(X) lookup table, tanh(X)~=1 if X>4 
#define SHAPER_LOOKUP_COEF    ((float)TABLE_SIZE / SHAPER_LOOKUP_MAX)
#define TWOPI                 6.2831853f
#define ONE_DIV_TWOPI         0.1591549f 
#define ONE_DIV_32768         (1.0f / 32768.0f)
#define ONE_DIV_127           (1.0f / 127.0f)

// ===================== MISC  ======================================================================================
const float DRAM_ATTR DIV_SAMPLE_RATE     = (1.0f/(float)SAMPLE_RATE);
const float DRAM_ATTR DIV_12              = (1.0f / 12.0f);
const float DRAM_ATTR DIV_63              = (1.0f / 63.0f);
const float DRAM_ATTR DIV_127             = (1.0f / 127.0f);
const float DRAM_ATTR MIDI_NORM           = (1.0f / 127.0f);
const float DRAM_ATTR DIV_128             = (1.0f / 128.0f);
const float DRAM_ATTR DIV_1200            = (1.0f / 1200.0f);
const float DRAM_ATTR DIV_8192            = (1.0f / 8192.0f);
const float DRAM_ATTR TWO_DIV_16383       = (2.0f / 16383.0f);
const float DRAM_ATTR MS_SAMPLE_RATE      = (float)SAMPLE_RATE * 0.001f;
const float DRAM_ATTR DIV_MS_SAMPLE_RATE  = 1.0f / (float)(MS_SAMPLE_RATE);
const float DRAM_ATTR SAMPLES_PER_MICROS  = (float)SAMPLE_RATE * 0.000001f;

// 1.0594630943592952645618252949463 // is a 12th root of 2 (pitch increase per semitone)
// 1.05952207969042122905182367802396 // stretched tuning (plus 60 cents per 7 octaves)

// converts semitones to speed: -12.0 semitones becomes 0.5, +12.0 semitones becomes 2.0
static __attribute__((always_inline)) inline float semitones2speed(float semitones) {
 // return powf(2.0f, semitones * 0.08333333f);
  return powf(1.059463f, semitones);
}

// fast but inaccurate approximation ideas taken from here:
// https://martin.ankerl.com/2007/10/04/optimized-pow-approximation-for-java-and-c-c/

static __attribute__((always_inline)) inline float fast_pow(float a, float b) {
  int16_t mc = 16256;
  float ex = fabs(fabs((float)b - (int)b) - 0.5f ) - 0.5f;
    union {
        float f;
        int16_t x[2];
    } u = { a };
    u.x[1] = (int16_t)(b * ((float)u.x[1] - (float)mc) + (float)mc);
    u.x[0] = 0;
    u.f *= 1.0f + 0.138f * (float)ex ;
    return u.f;
}

static __attribute__((always_inline)) inline float fast_semitones2speed(float semitones) {
  return fast_pow(2.0f, semitones * 0.08333333f);
}
 


// multiply by a reciprocal works almost faster than a float division though it can provide less accuracy

static __attribute__((always_inline)) inline float one_div(float a) {
    float result;
    asm volatile (
        "wfr f1, %1"          "\n\t"
        "recip0.s f0, f1"     "\n\t"
        "const.s f2, 1"       "\n\t"
        "msub.s f2, f1, f0"   "\n\t"
        "maddn.s f0, f0, f2"  "\n\t"
        "const.s f2, 1"       "\n\t"
        "msub.s f2, f1, f0"   "\n\t"
        "maddn.s f0, f0, f2"  "\n\t"
        "rfr %0, f0"          "\n\t"
        : "=r" (result)
        : "r" (a)
        : "f0","f1","f2"
    );
    return result;
}


// this float division works faster compared to the "/" operator.. dunno why
static __attribute__((always_inline)) inline float fdiv(float a, float b) {
    float result;
    asm volatile (
        "wfr f0, %1\n"
        "wfr f1, %2\n"
        "div0.s f3, f1 \n"
        "nexp01.s f4, f1 \n"
        "const.s f5, 1 \n"
        "maddn.s f5, f4, f3 \n"
        "mov.s f6, f3 \n"
        "mov.s f7, f1 \n"
        "nexp01.s f8, f0 \n"
        "maddn.s f6, f5, f3 \n"
        "const.s f5, 1 \n"
        "const.s f2, 0 \n"
        "neg.s f9, f8 \n"
        "maddn.s f5,f4,f6 \n"
        "maddn.s f2, f9, f3 \n" /* Original was "maddn.s f2, f0, f3 \n" */
        "mkdadj.s f7, f0 \n"
        "maddn.s f6,f5,f6 \n"
        "maddn.s f9,f4,f2 \n"
        "const.s f5, 1 \n"
        "maddn.s f5,f4,f6 \n"
        "maddn.s f2,f9,f6 \n"
        "neg.s f9, f8 \n"
        "maddn.s f6,f5,f6 \n"
        "maddn.s f9,f4,f2 \n"
        "addexpm.s f2, f7 \n"
        "addexp.s f6, f7 \n"
        "divn.s f2,f9,f6\n"
        "rfr %0, f2\n"
        :"=r"(result):"r"(a), "r"(b)
    );
    return result;
}

inline static int strpos(char *hay, char *needle, int offset) {
   char haystack[strlen(hay)];
   strncpy(haystack, hay+offset, strlen(hay)-offset);
   char *p = strstr(haystack, needle);
   if (p)
      return p - haystack+offset;
   return -1;
}

static const float sin_tbl[TABLE_SIZE+1] = {
  0.000000000f,   0.195090322f,  0.382683432f,  0.555570233f,  0.707106781f,  0.831469612f,  0.923879533f,  0.980785280f,
  1.000000000f,   0.980785280f,  0.923879533f,  0.831469612f,  0.707106781f,  0.555570233f,  0.382683432f,  0.195090322f, 
  0.000000000f,  -0.195090322f, -0.382683432f, -0.555570233f, -0.707106781f, -0.831469612f, -0.923879533f, -0.980785280f, 
  -1.000000000f, -0.980785280f, -0.923879533f, -0.831469612f, -0.707106781f, -0.555570233f, -0.382683432f, -0.195090322f, 0.000000000f 
};

static const float shaper_tbl[TABLE_SIZE+1] {
  0.000000000f, 0.154990730f, 0.302709729f, 0.437188785f, 0.554599722f, 0.653423588f, 0.734071520f, 0.798242755f, 
  0.848283640f, 0.886695149f, 0.915824544f, 0.937712339f, 0.954045260f, 0.966170173f, 0.975136698f, 0.981748725f, 
  0.986614298f, 0.990189189f, 0.992812795f, 0.994736652f, 0.996146531f, 0.997179283f, 0.997935538f, 0.998489189f, 
  0.998894443f, 0.999191037f, 0.999408086f, 0.999566912f, 0.999683128f, 0.999768161f, 0.999830378f, 0.999875899f , 0.999909204f 
};

static const float exp_tbl[TABLE_SIZE+1] {
  1.000000000f, 0.897423378f, 0.804994137f, 0.721708450f, 0.646661790f, 0.579039113f, 0.518106002f, 0.463200693f,
  0.413726886f, 0.369147269f, 0.328977687f, 0.292781876f, 0.260166733f, 0.230778038f, 0.204296618f, 0.180434868f, 
  0.158933640f, 0.139559419f, 0.122101791f, 0.106371156f, 0.092196673f, 0.079424399f, 0.067915621f, 0.057545348f, 
  0.048200953f, 0.039780951f, 0.032193897f, 0.025357391f, 0.019197186f, 0.013646379f, 0.008644684f, 0.004137782f, 0.0f
};

static const float knob_tbl[128] { 
  0.0f, 0.001537347f, 0.003107852f, 0.00471223f, 0.00635121f, 0.008025539f, 0.009735981f, 0.011483313f,
  0.013268331f, 0.015091848f, 0.016954695f, 0.018857719f, 0.020801787f, 0.022787785f, 0.024816617f, 0.026889206f,
  0.029006497f, 0.031169453f, 0.033379059f, 0.035636322f, 0.03794227f, 0.040297951f, 0.04270444f, 0.045162832f,
  0.047674246f, 0.050239826f, 0.05286074f, 0.055538181f, 0.058273369f, 0.06106755f, 0.063921995f, 0.066838004f,
  0.069816905f, 0.072860055f, 0.07596884f, 0.079144674f, 0.082389004f, 0.085703307f, 0.089089093f, 0.092547904f,
  0.096081313f, 0.099690931f, 0.1033784f, 0.1071454f, 0.110993647f, 0.114924891f, 0.118940925f, 0.123043576f,
  0.127234712f, 0.131516242f, 0.135890116f, 0.140358325f, 0.144922904f, 0.149585931f, 0.15434953f, 0.159215869f,
  0.164187165f, 0.169265681f, 0.17445373f, 0.179753675f, 0.185167927f, 0.190698954f, 0.196349274f, 0.202121458f,
  0.208018137f, 0.214041994f, 0.220195773f, 0.226482276f, 0.232904366f, 0.239464967f, 0.246167065f, 0.253013714f,
  0.260008031f, 0.2671532f, 0.274452475f, 0.28190918f, 0.28952671f, 0.297308534f, 0.305258196f, 0.313379315f,
  0.32167559f, 0.330150797f, 0.338808797f, 0.347653531f, 0.356689028f, 0.365919401f, 0.375348853f, 0.384981679f,
  0.394822265f, 0.404875091f, 0.415144735f, 0.425635873f, 0.436353284f, 0.447301846f, 0.458486545f, 0.469912474f,
  0.481584837f, 0.493508948f, 0.505690236f, 0.518134249f, 0.530846653f, 0.543833236f, 0.557099913f, 0.570652723f,
  0.584497839f, 0.598641565f, 0.613090341f, 0.627850746f, 0.642929502f, 0.658333475f, 0.674069679f, 0.690145279f,
  0.706567596f, 0.723344107f, 0.740482452f, 0.757990435f, 0.775876028f, 0.794147375f, 0.812812797f, 0.831880792f,
  0.851360043f, 0.87125942f, 0.891587985f, 0.912354993f, 0.933569902f, 0.955242372f, 0.97738227f, 0.99999968f 
};

inline static float lookupTable(const float (&table)[TABLE_SIZE+1], float index ) { // lookup value in a table by float index, using linear interpolation
  static float v1, v2, res;
  static int32_t i;
  static float f;
  i = (int32_t)index;
  f = (float)index - i;
  v1 = (table)[i];
  v2 = (table)[i+1];
  res = (float)f * (float)(v2-v1) + v1;
  return res;
}

inline float fast_shape(float x) {
    float sign = 1.0f;
    if (x < 0) {
      x = -x;
      sign = -1.0f;
    }
    if (x >= 4.95f) {
      return sign; // tanh(x) ~= 1, when |x| > 4
    }
  return  (float)sign * (float)lookupTable(shaper_tbl, ((float)x * (float)SHAPER_LOOKUP_COEF)); // lookup table contains tanh(x), 0 <= x <= 5
}
 

inline static float fast_sin(const float x) {
  const float argument = x * ONE_DIV_TWOPI * TABLE_SIZE;
  const float res = lookupTable(sin_tbl, CYCLE_INDEX(argument)+((float)argument-(int32_t)argument));
  return res;
}

inline static float fast_cos(const float x) {
  const float argument = ((x * ONE_DIV_TWOPI + 0.25f) * TABLE_SIZE);
  const float res = lookupTable(sin_tbl, CYCLE_INDEX(argument)+((float)argument-(int32_t)argument));
  return res;
}

inline void fast_sincos(const float x, float* sinRes, float* cosRes){  
  float argument = x * ONE_DIV_TWOPI * TABLE_SIZE;    
  *sinRes = lookupTable(sin_tbl, CYCLE_INDEX(argument)+((float)argument-(int32_t)argument));
  argument += QUARTER_TABLE;
  *cosRes = lookupTable(sin_tbl, CYCLE_INDEX(argument)+((float)argument-(int32_t)argument));
}

inline static float sin_lut(const float x_norm) {
  const float argument = (x_norm * TABLE_SIZE);
  const float res = lookupTable(sin_tbl, CYCLE_INDEX(argument)+((float)argument-(int32_t)argument));
  return res;
}

// norm_x belongs to [0..1], returns sin(alpha) curve for alpha [-pi/2 .. pi/2] normalized to [0..1]
inline static float sin_fadein(float norm_x) {
  return -0.5f * lookupTable(sin_tbl, HALF_TABLE * norm_x + QUARTER_TABLE) + 0.5f;
}

// norm_x belongs to [0..1], returns sin(alpha) curve for alpha [pi/2 .. 3*pi/2] normalized to [0..1]
inline static float sin_fadeout(float norm_x) {
  return 0.5f * lookupTable(sin_tbl, HALF_TABLE * norm_x + QUARTER_TABLE) + 0.5f;
}


inline static float ms_to_samples(float ms) {
  return MS_SAMPLE_RATE * ms;
}

inline static float samples_to_ms(float n_smp) {
  return n_smp * DIV_MS_SAMPLE_RATE;
}


inline static float lin_interpolate(float& v1, float& v2, float index) {
  float res;
  int32_t i = (int32_t)index;
  float f = (float)index - i;
  res = (float)f * (float)(v2 - v1) + (float)v1;
  return res;
}

inline static int32_t safe_index(int32_t x) {
  return x & ~(x >> 31);
}

inline static float limited(float in) {
  if (in > 1.0f) return 1.0f;
  if (in < -1.0f) return -1.0f;
  return in;
}

static inline float saturate_cubic(float x) {
    // Soft clipping cubic saturator
    // Good balance of smoothness and efficiency
    return x - (x * x * x) * (1.0f / 3.0f);
}

static inline float fclamp(float in, float minv, float maxv){
  if (in > maxv) return maxv;
  if (in < minv) return minv;
  return in;
}


struct dcBlocker {
    float x1 = 0.0f;
    float y1 = 0.0f;

    inline float process(float x) {
        const float y = x - x1 + 0.995f * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};


static inline float pitchBendRatio(int value, float range = 2.0f) {
    return powf(2.0f, (range * (value - PITCH_BEND_CENTER) * DIV_8192) * DIV_12 );
}

inline float fastLog2_(float x) {
    union { float f; uint32_t i; } vx = { x };
    float y = float(vx.i);
    y *= 1.1920928955078125e-7f; // 1 / (1 << 23)
    return y - 127.0f;
}

inline float fastLog2(float x) {
    int e;
    float m = frexpf(x, &e);  // x = m * 2^e, m ∈ [0.5, 1.0)
    m = (m - 0.70710678f) * 1.41421356f;  // map to ~[-1, 1]
    float approx = ((-0.34484843f * m + 2.02466578f) * m) - 0.67487759f;
    return float(e - 1) + approx;
}

inline float fastExp2_(float x) {
    union { uint32_t i; float f; } v;
    v.i = (uint32_t)(x * 12102203.0f + 1065353216.0f);
    return v.f;
}

inline float fastExp2(float x) {
    int i = int(x);
    float f = x - i;
    float p = 1.0f + f * (0.69314718f + f * (0.24022651f + f * 0.05550411f));
    return ldexpf(p, i);
}


static volatile uint32_t DRAM_ATTR gui_blocker = 0;   
inline void block_gui() {  gui_blocker = 100; /* this many audio buffers would run before gui starts again */ }

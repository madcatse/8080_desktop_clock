/* Zambretti / AlexGyver Forecaster (MIT), integer port.
 * (1 + 0.0065*h/(t+273.15))^5.257: binomial through x^5.
 * Linear term Q20; higher terms Horner Q16 with x in Q20.
 * Accepted: 30000..110000 Pa, -55.0..125.0 C, -500..3000 m.
 */
#include "forecast.h"
int32_t Parr[6];
int16_t altitude, delta;
uint8_t start, cast, season;
/* Load coefficients as data to avoid the sccz80 constant-add peephole
   dropping carry between E and D for a negative 32-bit addend. */
int32_t fc_coeff[3]={449221L,796137L,733317L};
void setH(int h) {
    if (h < -500) h = -500;
    if (h > 3000) h = 3000;
    altitude = h;
}
static int32_t mulQ20(int32_t a, int32_t b) {
    /* sccz80 2.4 constant signed remainder uses abs(); keep operands positive. */
    if (b<0) return -mulQ20(a,-b);
    int32_t q=a*(b/256L);
    return q/4096L+(q%4096L*256L+a*(b%256L))/1048576L;
}
static int32_t mulQ16(int32_t a, int32_t b) {
    if (b<0) { a=-a;b=-b; }
    int32_t q=a*(b/256L);
    return q/256L+(q%256L*256L+a*(b%256L))/65536L;
}
int32_t seaPressure(int32_t p, int16_t t10) {
    int32_t x, poly, factor, extra;
    uint8_t negative,i;
    if (p < 30000L || p > 110000L || t10 < -550 || t10 > 1250) return 0;
    x = (int32_t)altitude * 681574L / ((int32_t)t10 * 10 + 27315L);
    poly = 112934L;
    for (i=0;i<3;i++) poly=fc_coeff[i]+mulQ20(poly,x);
    extra = mulQ16(mulQ20(poly,x),x);
    factor = x * 5257L / 1000L + extra;
    negative=factor<0;
    if (negative) factor=-factor;
    /* Split product, including negative altitude, without 64-bit arithmetic. */
    extra = (p / 1024L) * factor;
    extra = extra / 1024L + (extra % 1024L * 1024L + (p % 1024L) * factor) / 1048576L;
    return negative?p-extra:p+extra;
}
void forecast_reset(void) { start = 0; delta = 0; }
uint8_t addP(int32_t p, int16_t t10) {
    uint8_t i;
    int32_t sum, result;
    p = seaPressure(p, t10);
    if (!p) return 0;
    if (!start) {
        start = 1;
        for (i=0; i<6; ++i) Parr[i] = p;
    } else {
        for (i=0; i<5; ++i) Parr[i] = Parr[i+1];
        Parr[5] = p;
    }
    sum = 5L*(Parr[5]-Parr[0]) + 3L*(Parr[4]-Parr[1]) + Parr[3]-Parr[2];
    sum /= 7;
    if (sum > 32767L) sum = 32767L;
    if (sum < -32767L) sum = -32767L;
    delta = sum;
    p /= 100;
    if (delta > 150) result = 160000L - 155L*p - (int32_t)season*1000;
    else if (delta < -150) result = 130000L - 124L*p + (int32_t)season*1000;
    else result = 138000L - 133L*p;
    if (result < 0) result = 0;
    cast = (result+500)/1000;
    return 1;
}
void setMonth(uint8_t month) { season = month == 0 ? 0 : ((month >= 4 && month <= 9) ? 2 : 1); }
uint8_t getCast(void) { return cast; }
int16_t getTrend(void) { return delta; }

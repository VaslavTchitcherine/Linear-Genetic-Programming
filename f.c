#include <math.h>

long double lgp(double *v)
{
    long double f[8];     // 80 bit x87 FPU registers
    long double tmp;
    unsigned char cflag=0;
    f[0] = f[1] = f[2] = f[3] = f[4] = f[5] = f[6] = f[7] = 0.0L;
// fcos 
    if ( fabsl(f[0]) < powl(2.0L,63.0L))
       f[0] = cosl(f[0]);
// fsub [0x80874D8]
    f[0] -= v[4];
// fsub [0x80874C0]
    f[0] -= v[1];
// fchs 
    f[0] = -f[0];
// fadd [0x80874D0]
    f[0] += v[3];
// fsub [0x80874C8]
    f[0] -= v[2];
// fsub [0x80874B8]
    f[0] -= v[0];
// fst st(1)
    f[1] = f[0];
// fmul st(0), st(0)
    f[0] *= f[0];
// fsin 
    if ( fabsl(f[0]) < powl(2.0L,63.0L))
    f[0] = sinl(f[0]);
	return f[0];
}

/* Math compatibility layer for the PiStorm3D static archive.
 *
 * libm.a's IEEE functions use the classic AmigaOS mathieeesingbas.library,
 * mathieeedoubbas.library and mathieeedoubtrans.library through the matching
 * base globals (soft-float libm references singbas; the 020/68881 multilib
 * references the doubbas pair).  A normal program startup would normally
 * arrange the bases, but minigl.library has no program startup and therefore
 * owns/opens the bases itself.
 *
 * The supplied PiStorm3D archive also references cexp().  The installed libm
 * does not provide it, so it is implemented locally using exp/sin/cos from
 * libm. Compile this implementation with -fno-builtin -fno-fast-math.
 * The common Classic makefile applies those flags to this object even when
 * the rest of the backend uses fast-math. Otherwise GCC 6.5 folds the
 * sin/cos pair below into a recursive cexp call.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>

#include <complex.h>
#include <math.h>

#include "math_shim.h"

/* Names and types expected by the GCC/libnix libm objects. */
struct Library *MathIeeeSingBasBase = 0;
struct Library *MathIeeeDoubBasBase = 0;
struct Library *MathIeeeDoubTransBase = 0;
struct Library *MathIeeeSingTransBase = 0;

BOOL MiniGL_OpenMathLibrary(void)
{
    if (MathIeeeSingBasBase && MathIeeeDoubBasBase && MathIeeeDoubTransBase && MathIeeeSingTransBase)
        return TRUE;

    if (!MathIeeeSingBasBase)
        MathIeeeSingBasBase = OpenLibrary((CONST_STRPTR) "mathieeesingbas.library", 0);

    if (!MathIeeeDoubBasBase)
        MathIeeeDoubBasBase = OpenLibrary((CONST_STRPTR) "mathieeedoubbas.library", 0);

    if (!MathIeeeDoubTransBase)
        MathIeeeDoubTransBase = OpenLibrary((CONST_STRPTR) "mathieeedoubtrans.library", 0);

    if (!MathIeeeSingTransBase)
        MathIeeeSingTransBase = OpenLibrary((CONST_STRPTR) "mathieeesingtrans.library", 0);

    if (MathIeeeSingBasBase && MathIeeeDoubBasBase && MathIeeeDoubTransBase && MathIeeeSingTransBase)
        return TRUE;

    MiniGL_CloseMathLibrary();
    return FALSE;
}

void MiniGL_CloseMathLibrary(void)
{
    if (MathIeeeSingBasBase)
    {
        struct Library *base = MathIeeeSingBasBase;
        MathIeeeSingBasBase = 0;
        CloseLibrary(base);
    }
    if (MathIeeeDoubBasBase)
    {
        struct Library *base = MathIeeeDoubBasBase;
        MathIeeeDoubBasBase = 0;
        CloseLibrary(base);
    }
    if (MathIeeeDoubTransBase)
    {
        struct Library *base = MathIeeeDoubTransBase;
        MathIeeeDoubTransBase = 0;
        CloseLibrary(base);
    }
    if (MathIeeeSingTransBase)
    {
        struct Library *base = MathIeeeSingTransBase;
        MathIeeeSingTransBase = 0;
        CloseLibrary(base);
    }
}

/* C99 complex exponential.  This symbol is needed by matrix.o in the supplied
 * PiStorm3D archive.  Avoid complex arithmetic expressions involving I here;
 * assigning the real/imaginary components explicitly keeps the generated ABI
 * straightforward on m68k GCC.
 */
double complex cexp(double complex value)
{
    double complex result;
    const double real_part = __real__ value;
    const double imag_part = __imag__ value;
    const double scale = exp(real_part);

    __real__ result = scale * cos(imag_part);
    __imag__ result = scale * sin(imag_part);
    return result;
}

// libm ships no sqrtf. 68881 gets fsqrt directly
#ifdef __HAVE_68881__
float sqrtf(float x)
{
    __asm__("fsqrt.x %1,%0" : "=f"(x) : "f"(x));
    return x;
}
#else
float sqrtf(float x)
{
    return (float)sqrt((double)x);
}
#endif

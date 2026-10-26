#ifndef MGLINT_H
#define MGLINT_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <utility/tagitem.h>
#include <devices/timer.h>
#include <Warp3D/Warp3D.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <proto/cybergraphics.h>
#include <inline/Warp3D.h>
#include <inline/cybergraphics.h>

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <mgl/gl.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;
extern struct Library *UtilityBase;
extern struct Library *Warp3DBase;
extern struct Library *CyberGfxBase;

enum
{
    SRC_A8,
    SRC_L8,
    SRC_LA8,
    SRC_I8,
    SRC_R8,
    SRC_G8,
    SRC_B8,
    SRC_RGB8,
    SRC_RGBA8,
    SRC_ARGB8,  /* MGL_UBYTE_ARGB */
    SRC_BGRA8,  /* MGL_UBYTE_BGRA */
    SRC_RGB565, /* MGL_UNSIGNED_SHORT_5_6_5 */
    SRC_ARGB4,  /* MGL_UNSIGNED_SHORT_4_4_4_4 */
    SRC_IDX8    /* GL_COLOR_INDEX */
};

// geometry pipeline internals
#define MGL_CLIP_EPS 1e-7f
#define MGL_CLIP_SLACK 16
#define MGL_NORMAL_RESERVE 4
#define MGL_IDXBUF_MULT 4
#define MGL_EXTRA_MAX 4

GLboolean mglEnsureVidLibs(void);
GLint mglTexLimit(GLint drvlim);
void GLMatrixInit(GLcontext context);
void mglUpdateCombined(GLcontext context);
void mglUpdateInvRot(GLcontext context);
void lt_cmtrack(GLenum mode, MGLMaterial *m, const W3D_Color *c);
void light_SetDefaults(GLcontext context);
GLboolean MGLInitContext(GLcontext context);

extern struct Device *TimerBase;

void TMA_Start(LockTimeHandle *handle);
void TMA_Stop(LockTimeHandle *handle);
double TMA_GetTime(LockTimeHandle *handle);

void d_Submit(GLcontext context, const UWORD *in, GLuint icount, GLboolean final);
void va_Stage(GLcontext context, GLuint index, GLuint slot);
void va_StageRange(GLcontext context, GLuint first, GLuint count, GLuint slot0);
void va_bindptr(GLcontext context);
void va_stagebind(GLcontext context);
ULONG va_colormode(GLcontext context);
GLboolean ef_On(GLcontext context, GLenum mode);
void ef_Run(GLcontext context, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLuint vbase);

int dx_lock(GLcontext context);
void dx_unlock(GLcontext context, int st);
void dx_draw(GLcontext context, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLuint base);

GLenum tex_srcfmt(GLenum format, GLenum type, int *sid, int *bpp);
void tex_negotiate(GLcontext context);
void tex_decode(const UBYTE *p, int sid, int swap, UBYTE *rgba);
void tex_sencode(UBYTE *d, int sid, int swap, const UBYTE *rgba);
void tex_decode_pixels(GLcontext context, const UBYTE *pixels, int sid, int sbpp, GLint w, GLint h, UBYTE *rgba);
void tex_unpk0(GLcontext context, GLint save[5]);
void tex_unpk1(GLcontext context, const GLint save[5]);
GLuint cl_ClipIndex(GLcontext context, const UWORD *in, GLuint n, ULONG orc, UWORD **outp, GLuint *freep);
void cl_FrontOrder(GLcontext context);
void fog_Set(GLcontext context);

static inline void mgl_error(GLcontext context, GLenum err)
{
    if (context->CurrentError == GL_NO_ERROR)
        context->CurrentError = err;
}

// w3d restricts the color factors to one side each, gl keeps its own errors
static inline ULONG mgl_blend_src(GLenum f)
{
    switch (f)
    {
    case GL_ZERO:
        return W3D_ZERO;
    case GL_ONE:
        return W3D_ONE;
    case GL_DST_COLOR:
        return W3D_DST_COLOR;
    case GL_ONE_MINUS_DST_COLOR:
        return W3D_ONE_MINUS_DST_COLOR;
    case GL_SRC_ALPHA:
        return W3D_SRC_ALPHA;
    case GL_ONE_MINUS_SRC_ALPHA:
        return W3D_ONE_MINUS_SRC_ALPHA;
    case GL_DST_ALPHA:
        return W3D_DST_ALPHA;
    case GL_ONE_MINUS_DST_ALPHA:
        return W3D_ONE_MINUS_DST_ALPHA;
    case GL_SRC_ALPHA_SATURATE:
        return W3D_SRC_ALPHA_SATURATE;
    }

    return 0;
}

static inline ULONG mgl_blend_dst(GLenum f)
{
    switch (f)
    {
    case GL_ZERO:
        return W3D_ZERO;
    case GL_ONE:
        return W3D_ONE;
    case GL_SRC_COLOR:
        return W3D_SRC_COLOR;
    case GL_ONE_MINUS_SRC_COLOR:
        return W3D_ONE_MINUS_SRC_COLOR;
    case GL_SRC_ALPHA:
        return W3D_SRC_ALPHA;
    case GL_ONE_MINUS_SRC_ALPHA:
        return W3D_ONE_MINUS_SRC_ALPHA;
    case GL_DST_ALPHA:
        return W3D_DST_ALPHA;
    case GL_ONE_MINUS_DST_ALPHA:
        return W3D_ONE_MINUS_DST_ALPHA;
    }

    return 0;
}

static inline ULONG mgl_envw(GLenum m)
{
    switch (m)
    {
    case GL_MODULATE:
        return W3D_MODULATE;
    case GL_DECAL:
        return W3D_DECAL;
    case GL_REPLACE:
        return W3D_REPLACE;
    case GL_BLEND:
        return W3D_BLEND;
    }

    return 0;
}

static inline ULONG mgl_depthw(GLenum f)
{
    switch (f)
    {
    case GL_NEVER:
        return W3D_Z_NEVER;
    case GL_LESS:
        return W3D_Z_LESS;
    case GL_EQUAL:
        return W3D_Z_EQUAL;
    case GL_LEQUAL:
        return W3D_Z_LEQUAL;
    case GL_GREATER:
        return W3D_Z_GREATER;
    case GL_NOTEQUAL:
        return W3D_Z_NOTEQUAL;
    case GL_GEQUAL:
        return W3D_Z_GEQUAL;
    case GL_ALWAYS:
        return W3D_Z_ALWAYS;
    }

    return 0;
}

#endif

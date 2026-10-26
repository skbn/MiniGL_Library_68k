#include "mglint.h"

static const ULONG tex_fmtneg[12][4] =
    {
        {0},
        {W3D_A8R8G8B8, W3D_A4R4G4B4, W3D_A1R5G5B5, 0}, /* CHUNKY: expand through palette */
        {W3D_A4R4G4B4, W3D_A8R8G8B8, 0, 0},            /* A1R5G5B5 */
        {W3D_A4R4G4B4, W3D_R8G8B8, 0, 0},              /* R5G6B5 */
        {W3D_R5G6B5, W3D_A4R4G4B4, 0, 0},              /* R8G8B8 */
        {W3D_A1R5G5B5, W3D_A8R8G8B8, 0, 0},            /* A4R4G4B4 */
        {W3D_A4R4G4B4, W3D_A1R5G5B5, W3D_R8G8B8A8, 0}, /* A8R8G8B8 */
        {W3D_L8A8, W3D_A4R4G4B4, W3D_A1R5G5B5, 0},     /* A8 */
        {W3D_R5G6B5, W3D_R8G8B8, W3D_A4R4G4B4, 0},     /* L8 */
        {W3D_A4R4G4B4, W3D_A8R8G8B8, 0, 0},            /* L8A8 */
        {W3D_A4R4G4B4, 0, 0, 0},                       /* I8 */
        {W3D_A8R8G8B8, W3D_A4R4G4B4, W3D_A1R5G5B5, 0}, /* R8G8B8A8 */
};

static int tex_fmtbpp(ULONG fmt)
{
    switch (fmt)
    {
    case W3D_CHUNKY:
    case W3D_A8:
    case W3D_L8:
    case W3D_I8:
        return 1;
    case W3D_L8A8:
    case W3D_R5G6B5:
    case W3D_A1R5G5B5:
    case W3D_A4R4G4B4:
        return 2;
    case W3D_R8G8B8:
        return 3;
    case W3D_A8R8G8B8:
    case W3D_R8G8B8A8:
        return 4;
    }

    return 0;
}

GLenum tex_srcfmt(GLenum format, GLenum type, int *sid, int *bpp)
{
    if (type == GL_UNSIGNED_BYTE)
    {
        switch (format)
        {
        case GL_RGBA:
            *sid = SRC_RGBA8;
            *bpp = 4;
            break;
        case GL_RGB:
            *sid = SRC_RGB8;
            *bpp = 3;
            break;
        case GL_ALPHA:
            *sid = SRC_A8;
            *bpp = 1;
            break;
        case GL_LUMINANCE:
            *sid = SRC_L8;
            *bpp = 1;
            break;
        case GL_LUMINANCE_ALPHA:
            *sid = SRC_LA8;
            *bpp = 2;
            break;
        case GL_INTENSITY:
            *sid = SRC_I8;
            *bpp = 1;
            break;
        case GL_COLOR_INDEX:
            *sid = SRC_IDX8;
            *bpp = 1;
            break;
        case GL_RED:
        case GL_DEPTH_COMPONENT:
            *sid = SRC_R8;
            *bpp = 1;
            break;
        case GL_GREEN:
            *sid = SRC_G8;
            *bpp = 1;
            break;
        case GL_BLUE:
            *sid = SRC_B8;
            *bpp = 1;
            break;
        default:
            return GL_INVALID_ENUM;
        }

        return GL_NO_ERROR;
    }

    if (type == MGL_UBYTE_ARGB || type == MGL_UBYTE_BGRA)
    {
        if (format != GL_RGBA)
            return GL_INVALID_OPERATION;

        *sid = type == MGL_UBYTE_ARGB ? SRC_ARGB8 : SRC_BGRA8;
        *bpp = 4;

        return GL_NO_ERROR;
    }

    if (type == MGL_UNSIGNED_SHORT_5_6_5)
    {
        if (format != GL_RGB)
            return GL_INVALID_OPERATION;

        *sid = SRC_RGB565;
        *bpp = 2;

        return GL_NO_ERROR;
    }

    if (type == MGL_UNSIGNED_SHORT_4_4_4_4)
    {
        if (format != GL_RGBA)
            return GL_INVALID_OPERATION;

        *sid = SRC_ARGB4;
        *bpp = 2;

        return GL_NO_ERROR;
    }

    return GL_INVALID_ENUM;
}

static int tex_fastfmt(ULONG info)
{
    return (info & W3D_TEXFMT_SUPPORTED) && (info & (W3D_TEXFMT_FAST | W3D_TEXFMT_CLUTFAST | W3D_TEXFMT_ARGBFAST));
}

void tex_negotiate(GLcontext context)
{
    W3D_Context *w3d = context->w3dContext;
    W3D_Driver *drv = context->TexFmtDrv;
    ULONG f;
    int i;

    for (f = W3D_CHUNKY; f <= W3D_R8G8B8A8; f++)
    {
        ULONG pick = 0;
        ULONG ci = W3D_GetTexFmtInfo(w3d, f, w3d->format);
        ULONG di = drv ? W3D_GetDriverTexFmtInfo(drv, f, w3d->format) : ci;

        if (tex_fastfmt(ci) && tex_fastfmt(di))
        {
            context->TexFmtNeg[f] = f;
            continue;
        }

        if (!(ci & W3D_TEXFMT_UNSUPPORTED) && (f < W3D_R8G8B8A8 || tex_fastfmt(ci) || tex_fastfmt(di)))
            pick = f;

        for (i = 0; i < 4 && tex_fmtneg[f][i]; i++)
        {
            const ULONG alt = tex_fmtneg[f][i];
            ULONG ai = W3D_GetTexFmtInfo(w3d, alt, w3d->format);
            ULONG aj = drv ? W3D_GetDriverTexFmtInfo(drv, alt, w3d->format) : ai;

            if (tex_fastfmt(ai) && tex_fastfmt(aj))
            {
                pick = alt;
                break;
            }

            if (!pick && !(ai & W3D_TEXFMT_UNSUPPORTED))
                pick = alt;
        }

        context->TexFmtNeg[f] = pick ? pick : f;
    }
}

static GLenum tex_dstfmt(GLcontext context, GLint internal, ULONG *dfmt, int *dbpp)
{
    switch (internal)
    {
    case GL_ALPHA:
    case GL_ALPHA4:
    case GL_ALPHA8:
        *dfmt = W3D_A8;
        break;
    case 1:
    case GL_LUMINANCE:
    case GL_LUMINANCE4:
    case GL_LUMINANCE8:
        *dfmt = W3D_L8;
        break;
    case 2:
    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE4_ALPHA4:
    case GL_LUMINANCE8_ALPHA8:
        *dfmt = W3D_L8A8;
        break;
    case GL_INTENSITY:
    case GL_INTENSITY8:
        *dfmt = W3D_I8;
        break;
    case 3:
    case GL_RGB:
    case GL_RGB8:
        *dfmt = W3D_R8G8B8;
        break;
    case GL_RGB4:
    case GL_RGB5:
        *dfmt = W3D_R5G6B5;
        break;
    case 4:
    case GL_RGBA:
    case GL_RGBA8:
        *dfmt = W3D_R8G8B8A8;
        break;
    case GL_RGBA4:
        *dfmt = W3D_A4R4G4B4;
        break;
    case GL_RGB5_A1:
        *dfmt = W3D_A1R5G5B5;
        break;
    case GL_COLOR_INDEX:
        *dfmt = W3D_CHUNKY;
        break;
    default:
        return GL_INVALID_ENUM;
    }

    if (!context->TexFmtNeg[1] && context->w3dContext)
        tex_negotiate(context);

    if (context->TexFmtNeg[*dfmt])
        *dfmt = context->TexFmtNeg[*dfmt];

    *dbpp = tex_fmtbpp(*dfmt);

    return GL_NO_ERROR;
}

static int tex_verbatim(int sid, ULONG fmt, int swap)
{
    if (swap && (sid == SRC_RGB565 || sid == SRC_ARGB4))
        return 0;

    switch (fmt)
    {
    case W3D_R8G8B8A8:
        return sid == SRC_RGBA8;
    case W3D_A8R8G8B8:
        return sid == SRC_ARGB8;
    case W3D_R8G8B8:
        return sid == SRC_RGB8;
    case W3D_R5G6B5:
        return sid == SRC_RGB565;
    case W3D_A4R4G4B4:
        return sid == SRC_ARGB4;
    case W3D_A8:
        return sid == SRC_A8;
    case W3D_L8:
        return sid == SRC_L8;
    case W3D_L8A8:
        return sid == SRC_LA8;
    case W3D_I8:
        return sid == SRC_I8;
    case W3D_CHUNKY:
        return sid == SRC_IDX8;
    }

    return 0;
}

void tex_decode(const UBYTE *p, int sid, int swap, UBYTE *rgba)
{
    GLuint v;

    switch (sid)
    {
    case SRC_RGBA8:
        rgba[0] = p[0];
        rgba[1] = p[1];
        rgba[2] = p[2];
        rgba[3] = p[3];
        break;
    case SRC_ARGB8:
        rgba[0] = p[1];
        rgba[1] = p[2];
        rgba[2] = p[3];
        rgba[3] = p[0];
        break;
    case SRC_BGRA8:
        rgba[0] = p[2];
        rgba[1] = p[1];
        rgba[2] = p[0];
        rgba[3] = p[3];
        break;
    case SRC_RGB8:
        rgba[0] = p[0];
        rgba[1] = p[1];
        rgba[2] = p[2];
        rgba[3] = 0xFF;
        break;
    case SRC_R8:
        rgba[0] = p[0];
        rgba[1] = 0;
        rgba[2] = 0;
        rgba[3] = 0xFF;
        break;
    case SRC_G8:
        rgba[0] = 0;
        rgba[1] = p[0];
        rgba[2] = 0;
        rgba[3] = 0xFF;
        break;
    case SRC_B8:
        rgba[0] = 0;
        rgba[1] = 0;
        rgba[2] = p[0];
        rgba[3] = 0xFF;
        break;
    case SRC_A8:
        rgba[0] = 0;
        rgba[1] = 0;
        rgba[2] = 0;
        rgba[3] = p[0];
        break;
    case SRC_L8:
        rgba[0] = p[0];
        rgba[1] = p[0];
        rgba[2] = p[0];
        rgba[3] = 0xFF;
        break;
    case SRC_LA8:
        rgba[0] = p[0];
        rgba[1] = p[0];
        rgba[2] = p[0];
        rgba[3] = p[1];
        break;
    case SRC_I8:
        rgba[0] = p[0];
        rgba[1] = p[0];
        rgba[2] = p[0];
        rgba[3] = p[0];
        break;
    case SRC_RGB565:
        v = swap ? (GLuint)p[0] | ((GLuint)p[1] << 8) : ((GLuint)p[0] << 8) | p[1];
        rgba[0] = (UBYTE)(((v >> 11) << 3) | (v >> 13));
        rgba[1] = (UBYTE)((((v >> 5) & 0x3F) << 2) | (((v >> 5) & 0x3F) >> 4));
        rgba[2] = (UBYTE)(((v & 0x1F) << 3) | ((v & 0x1F) >> 2));
        rgba[3] = 0xFF;
        break;
    case SRC_ARGB4:
        v = swap ? (GLuint)p[0] | ((GLuint)p[1] << 8) : ((GLuint)p[0] << 8) | p[1];
        rgba[0] = (UBYTE)(((v >> 8) & 0x0F) * 17);
        rgba[1] = (UBYTE)(((v >> 4) & 0x0F) * 17);
        rgba[2] = (UBYTE)((v & 0x0F) * 17);
        rgba[3] = (UBYTE)((v >> 12) * 17);
        break;
    default:
        rgba[0] = rgba[1] = rgba[2] = 0;
        rgba[3] = 0xFF;
        break;
    }
}

static void tex_encode(UBYTE *d, ULONG fmt, const UBYTE *rgba)
{
    GLuint v;

    switch (fmt)
    {
    case W3D_R8G8B8A8:
        d[0] = rgba[0];
        d[1] = rgba[1];
        d[2] = rgba[2];
        d[3] = rgba[3];
        break;
    case W3D_A8R8G8B8:
        d[0] = rgba[3];
        d[1] = rgba[0];
        d[2] = rgba[1];
        d[3] = rgba[2];
        break;
    case W3D_R8G8B8:
        d[0] = rgba[0];
        d[1] = rgba[1];
        d[2] = rgba[2];
        break;
    case W3D_R5G6B5:
        v = ((GLuint)(rgba[0] >> 3) << 11) | ((GLuint)(rgba[1] >> 2) << 5) | (rgba[2] >> 3);
        d[0] = (UBYTE)(v >> 8);
        d[1] = (UBYTE)v;
        break;
    case W3D_A1R5G5B5:
        v = ((GLuint)(rgba[3] >> 7) << 15) | ((GLuint)(rgba[0] >> 3) << 10) | ((GLuint)(rgba[1] >> 3) << 5) | (rgba[2] >> 3);
        d[0] = (UBYTE)(v >> 8);
        d[1] = (UBYTE)v;
        break;
    case W3D_A4R4G4B4:
        v = ((GLuint)(rgba[3] >> 4) << 12) | ((GLuint)(rgba[0] >> 4) << 8) | ((GLuint)(rgba[1] >> 4) << 4) | (rgba[2] >> 4);
        d[0] = (UBYTE)(v >> 8);
        d[1] = (UBYTE)v;
        break;
    case W3D_A8:
        d[0] = rgba[3];
        break;
    case W3D_L8:
        d[0] = rgba[0];
        break;
    case W3D_L8A8:
        d[0] = rgba[0];
        d[1] = rgba[3];
        break;
    case W3D_I8:
        d[0] = (UBYTE)((rgba[0] + rgba[1] + rgba[2] + rgba[3]) >> 2);
        break;
    }
}

static void tex_rows(GLcontext context, const UBYTE *src, int sid, int sbpp, UBYTE *dst, ULONG dfmt, int dbpp, GLint w, GLint h, GLint dpitch)
{
    const GLint rowlen = context->CurUnpackRowLength ? context->CurUnpackRowLength : w;
    const GLint spitch = (rowlen * sbpp + context->UnpackAlign - 1) & ~(context->UnpackAlign - 1);
    const int swap = context->CurUnpackSwapBytes != 0;
    const int verbatim = tex_verbatim(sid, dfmt, swap);
    const GLint copy = w * sbpp;
    GLint x;
    GLint y;

    src += context->CurUnpackSkipRows * spitch + context->CurUnpackSkipPixels * sbpp;

    if (sid == SRC_IDX8 && dfmt != W3D_CHUNKY)
    {
        const ULONG *pal = (const ULONG *)context->PaletteData;

        for (y = 0; y < h; y++, src += spitch, dst += dpitch)
        {
            const UBYTE *s = src;
            UBYTE *d = dst;

            for (x = 0; x < w; x++, s++, d += dbpp)
            {
                const ULONG c = pal[*s];
                UBYTE rgba[4];

                rgba[0] = (UBYTE)(c >> 16);
                rgba[1] = (UBYTE)(c >> 8);
                rgba[2] = (UBYTE)c;
                rgba[3] = (UBYTE)(c >> 24);

                tex_encode(d, dfmt, rgba);
            }
        }

        return;
    }

    for (y = 0; y < h; y++, src += spitch, dst += dpitch)
    {
        const UBYTE *s = src;
        UBYTE *d = dst;

        if (verbatim)
        {
            memcpy(dst, src, copy);
            continue;
        }

        for (x = 0; x < w; x++, s += sbpp, d += dbpp)
        {
            UBYTE rgba[4];

            tex_decode(s, sid, swap, rgba);
            tex_encode(d, dfmt, rgba);
        }
    }
}

static GLint tex_leveldim(GLint d, GLint level)
{
    while (level-- && d > 1)
        d >>= 1;

    return d;
}

static ULONG tex_levoff(GLint w, GLint h, GLint bpp, GLint level)
{
    ULONG off = 0;

    while (level--)
    {
        off += (ULONG)(w * h * bpp);

        if (w > 1)
            w >>= 1;

        if (h > 1)
            h >>= 1;
    }

    return off;
}

static ULONG tex_total(GLint w, GLint h, GLint bpp, GLint mip)
{
    ULONG total = (ULONG)(w * h * bpp);

    if (mip)
    {
        while (w > 1 || h > 1)
        {
            if (w > 1)
                w >>= 1;

            if (h > 1)
                h >>= 1;

            total += (ULONG)(w * h * bpp);
        }
    }

    return total;
}

static GLint tex_bytes(const W3D_Texture *t)
{
    return (GLint)tex_total(t->texwidth, t->texheight, tex_fmtbpp((ULONG)t->texfmtsrc), t->mipmap != 0);
}

static ULONG tex_minw(GLenum f)
{
    switch (f)
    {
    case GL_NEAREST:
        return W3D_NEAREST;
    case GL_LINEAR:
        return W3D_LINEAR;
    case GL_NEAREST_MIPMAP_NEAREST:
        return W3D_NEAREST_MIP_NEAREST;
    case GL_LINEAR_MIPMAP_NEAREST:
        return W3D_LINEAR_MIP_NEAREST;
    case GL_NEAREST_MIPMAP_LINEAR:
        return W3D_NEAREST_MIP_LINEAR;
    case GL_LINEAR_MIPMAP_LINEAR:
        return W3D_LINEAR_MIP_LINEAR;
    }

    return 0;
}

static ULONG tex_wrapw(GLenum f)
{
    switch (f)
    {
    case GL_REPEAT:
        return W3D_REPEAT;
    case GL_CLAMP:
        return W3D_CLAMP;
    }

    return 0;
}

static void tex_pushenv(GLcontext context, W3D_Texture *t)
{
    const GLenum mode = context->TexEnv[0];
    W3D_Color *c = &context->TexEnvColor[0];
    const ULONG env = mgl_envw(mode);

    if (!env)
        return;

    if (mode == context->CurTexEnv && (env != W3D_BLEND || (context->CurTexEnvColor.r == c->r && context->CurTexEnvColor.g == c->g && context->CurTexEnvColor.b == c->b && context->CurTexEnvColor.a == c->a)))
        return;

    W3D_SetTexEnv(context->w3dContext, t, env, env == W3D_BLEND ? c : NULL);

    context->CurTexEnv = mode;
    context->CurTexEnvColor = *c;
}

static void tex_pushparams(GLcontext context, W3D_Texture *t)
{
    W3D_SetFilter(context->w3dContext, t, tex_minw(context->MinFilter), tex_minw(context->MagFilter));
    W3D_SetWrapMode(context->w3dContext, t, tex_wrapw(context->WrapS), tex_wrapw(context->WrapT), NULL);
}

static GLuint tex_binding(const GLcontext context)
{
    return context->ActiveTexture ? (GLuint)context->VirtualBinding[context->ActiveTexture] : (GLuint)context->CurrentBinding;
}

static W3D_Texture *tex_object(const GLcontext context, GLuint name)
{
    if (name >= (GLuint)context->TexBufferSize)
        return NULL;

    return context->w3dTexBuffer[name];
}

static ULONG *tex_palette(GLcontext context)
{
    return context->PaletteSize > 0 ? (ULONG *)context->PaletteData : NULL;
}

GLint mglTexLimit(GLint drvlim)
{
    GLint lim = drvlim > 0 ? drvlim : 256;
    UBYTE buf[16] = {0};

    if (DOSBase && GetVar((STRPTR) "MINIGL_MAX_TEXTURE_SIZE", (STRPTR)buf, sizeof buf, 0) > 0)
    {
        const UBYTE *e;
        GLint env = 0;

        for (e = buf; *e >= '0' && *e <= '9'; e++)
            env = env * 10 + (*e - '0');

        if (env <= 0 || env > 0xFFFF)
            env = 0xFFFF;

        if (drvlim <= 0 || env < lim)
            lim = env;
    }

    // above this, w*h*4 would wrap a 32-bit total in tex_total
    if (lim > 0x4000)
        lim = 0x4000;

    return lim;
}

void GLGenTextures(GLcontext context, GLsizei n, GLuint *textures)
{
    const GLuint size = (GLuint)context->TexBufferSize;
    GLuint pos;
    GLsizei i;

    if (n < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (!textures || size < 2)
        return;

    pos = context->NextTexture;

    if (!pos || pos >= size)
        pos = 1;

    for (i = 0; i < n; i++)
    {
        const GLuint start = pos;
        GLuint name = 0;

        for (;;)
        {
            if (!context->GeneratedTextures[pos])
            {
                name = pos;
                pos++;

                if (pos >= size)
                    pos = 1;

                break;
            }

            if (++pos >= size)
                pos = 1;

            if (pos == start)
                break;
        }

        if (name)
            context->GeneratedTextures[name] = 1;

        textures[i] = name;
    }

    context->NextTexture = pos;
}

void GLDeleteTextures(GLcontext context, GLsizei n, const GLuint *textures)
{
    const GLuint size = (GLuint)context->TexBufferSize;
    W3D_Context *w3d = context->w3dContext;
    GLsizei i;

    if (n < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (!textures)
        return;

    for (i = 0; i < n; i++)
    {
        const GLuint name = textures[i];
        W3D_Texture *obj;

        if (!name || name >= size)
            continue;

        if (name == (GLuint)context->CurrentBinding)
        {
            if (w3d)
            {
                W3D_BindTexture(w3d, 0, NULL);
                W3D_SetState(w3d, W3D_TEXMAPPING, W3D_DISABLE);
            }

            context->CurrentBinding = 0;
        }

        GLuint u;

        for (u = 1; u < context->TexUnits; u++)
        {
            if (name == (GLuint)context->VirtualBinding[u])
                context->VirtualBinding[u] = 0;
        }

        obj = context->w3dTexBuffer[name];

        if (obj)
        {
            context->TexMemCur -= tex_bytes(obj);

            if (w3d)
                W3D_FreeTexObj(w3d, obj);

            context->w3dTexBuffer[name] = NULL;
        }

        if (context->w3dTexMemory[name])
        {
            free(context->w3dTexMemory[name]);
            context->w3dTexMemory[name] = NULL;
        }

        context->GeneratedTextures[name] = 0;
    }
}

void GLBindTexture(GLcontext context, GLenum target, GLuint texture)
{
    W3D_Context *w3d = context->w3dContext;
    W3D_Texture *obj;

    if (target != GL_TEXTURE_2D)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (texture >= (GLuint)context->TexBufferSize)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (texture)
        context->GeneratedTextures[texture] = 1;

    if (context->ActiveTexture)
    {
        context->VirtualBinding[context->ActiveTexture] = (GLint)texture;
        return;
    }

    if (texture == (GLuint)context->CurrentBinding)
        return;

    context->CurrentBinding = (GLint)texture;

    if (!w3d)
        return;

    obj = context->w3dTexBuffer[texture];

    W3D_BindTexture(w3d, 0, obj);
    W3D_SetState(w3d, W3D_TEXMAPPING, context->Texture2D_State[0] && obj ? W3D_ENABLE : W3D_DISABLE);

    if (obj)
    {
        tex_pushparams(context, obj);
        tex_pushenv(context, obj);
    }
}

void GLTexImage1D(GLcontext context, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
    if (target != GL_TEXTURE_1D)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    GLTexImage2D(context, GL_TEXTURE_2D, level, internalformat, width, 1, border, format, type, pixels);
}

void GLTexImage2D(GLcontext context, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
    W3D_Context *w3d = context->w3dContext;
    const GLuint name = tex_binding(context);
    W3D_Texture *old;
    W3D_Texture *obj;
    struct TagItem tt[8];
    GLubyte *mem;
    ULONG dfmt = 0;
    ULONG w3derr = W3D_SUCCESS;
    GLenum e;
    GLint lw;
    GLint lh;
    int dbpp = 0;
    int sid = 0;
    int sbpp = 0;
    int n = 0;
    const int mip = context->NoMipMapping == GL_FALSE;

    if (target != GL_TEXTURE_2D)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (border != 0 || width < 1 || height < 1 || (width & (width - 1)) || (height & (height - 1)) || level < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (!w3d)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (width > mglTexLimit(w3d->maxtexwidthp) || height > mglTexLimit(w3d->maxtexheightp))
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    e = tex_dstfmt(context, internalformat, &dfmt, &dbpp);

    if (e != GL_NO_ERROR)
    {
        mgl_error(context, e);
        return;
    }

    e = tex_srcfmt(format, type, &sid, &sbpp);

    if (e != GL_NO_ERROR)
    {
        mgl_error(context, e);
        return;
    }

    // color-index uploads need index data plus a palette for hw clut or expansion
    if ((internalformat == GL_COLOR_INDEX) != (sid == SRC_IDX8) || (sid == SRC_IDX8 && context->PaletteSize <= 0))
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    old = tex_object(context, name);

    if (level)
    {
        ULONG off;

        if (!old || !old->mipmap || level >= 16)
        {
            mgl_error(context, GL_INVALID_OPERATION);
            return;
        }

        if (!(old->texwidth >> level) && !(old->texheight >> level))
        {
            mgl_error(context, GL_INVALID_VALUE);
            return;
        }

        if ((ULONG)old->texfmtsrc != dfmt)
        {
            mgl_error(context, GL_INVALID_OPERATION);
            return;
        }

        lw = tex_leveldim(old->texwidth, level);
        lh = tex_leveldim(old->texheight, level);

        if (width != lw || height != lh)
        {
            mgl_error(context, GL_INVALID_VALUE);
            return;
        }

        off = tex_levoff(old->texwidth, old->texheight, dbpp, level);
        mem = context->w3dTexMemory[name] + off;

        if (pixels)
            tex_rows(context, (const UBYTE *)pixels, sid, sbpp, mem, dfmt, dbpp, width, height, width * dbpp);
        else
            memset(mem, 0, (size_t)(width * height * dbpp));

        W3D_UpdateTexImage(w3d, old, mem, level, tex_palette(context));

        return;
    }

    // same shape without mipmaps refills the locked image, else recreate
    if (old && !old->mipmap && !mip && old->texwidth == (int)width && old->texheight == (int)height && (ULONG)old->texfmtsrc == dfmt)
    {
        mem = context->w3dTexMemory[name];

        if (pixels)
            tex_rows(context, (const UBYTE *)pixels, sid, sbpp, mem, dfmt, dbpp, width, height, width * dbpp);
        else
            memset(mem, 0, (size_t)(width * height * dbpp));

        W3D_UpdateTexImage(w3d, old, mem, 0, tex_palette(context));

        return;
    }

    mem = (GLubyte *)malloc((size_t)tex_total(width, height, dbpp, mip));

    if (!mem)
    {
        mgl_error(context, GL_OUT_OF_MEMORY);
        return;
    }

    if (pixels)
        tex_rows(context, (const UBYTE *)pixels, sid, sbpp, mem, dfmt, dbpp, width, height, width * dbpp);
    else
        memset(mem, 0, (size_t)tex_total(width, height, dbpp, mip));

    tt[n].ti_Tag = W3D_ATO_IMAGE;
    tt[n++].ti_Data = (ULONG)mem;
    tt[n].ti_Tag = W3D_ATO_FORMAT;
    tt[n++].ti_Data = dfmt;
    tt[n].ti_Tag = W3D_ATO_WIDTH;
    tt[n++].ti_Data = (ULONG)width;
    tt[n].ti_Tag = W3D_ATO_HEIGHT;
    tt[n++].ti_Data = (ULONG)height;

    if (mip)
    {
        tt[n].ti_Tag = W3D_ATO_MIPMAP;
        tt[n++].ti_Data = 0xFFFF;
    }

    if (dfmt == W3D_CHUNKY)
    {
        tt[n].ti_Tag = W3D_ATO_PALETTE;
        tt[n++].ti_Data = (ULONG)context->PaletteData;
    }

    tt[n].ti_Tag = TAG_DONE;
    tt[n].ti_Data = 0;

    obj = W3D_AllocTexObj(w3d, &w3derr, tt);

    if (!obj)
    {
        free(mem);

        switch (w3derr)
        {
        case W3D_NOMEMORY:
        case W3D_NOGFXMEM:
            mgl_error(context, GL_OUT_OF_MEMORY);
            break;
        case W3D_UNSUPPORTEDTEXSIZE:
            mgl_error(context, GL_INVALID_VALUE);
            break;
        default:
            mgl_error(context, GL_INVALID_OPERATION);
            break;
        }

        return;
    }

    if (old)
    {
        context->TexMemCur -= tex_bytes(old);

        W3D_FreeTexObj(w3d, old);

        free(context->w3dTexMemory[name]);
    }

    context->w3dTexBuffer[name] = obj;
    context->w3dTexMemory[name] = mem;
    context->TexMemCur += (GLint)tex_total(width, height, dbpp, mip);

    if (context->TexMemCur > context->TexMemPeak)
        context->TexMemPeak = context->TexMemCur;

    W3D_UploadTexture(w3d, obj);

    tex_pushparams(context, obj);
    tex_pushenv(context, obj);

    if (!context->ActiveTexture)
    {
        W3D_BindTexture(w3d, 0, obj);
        W3D_SetState(w3d, W3D_TEXMAPPING, context->Texture2D_State[0] ? W3D_ENABLE : W3D_DISABLE);
    }
}

void GLTexSubImage2D(GLcontext context, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
    W3D_Context *w3d = context->w3dContext;
    const GLuint name = tex_binding(context);
    W3D_Texture *obj;
    UBYTE *mem;
    GLenum e;
    GLint lw;
    GLint lh;
    int sid = 0;
    int sbpp = 0;
    int dbpp;

    if (target != GL_TEXTURE_2D)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (level < 0 || level >= 16)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (!w3d)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    e = tex_srcfmt(format, type, &sid, &sbpp);

    if (e != GL_NO_ERROR)
    {
        mgl_error(context, e);
        return;
    }

    obj = tex_object(context, name);

    if (!obj || (level && !obj->mipmap))
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (level && !(obj->texwidth >> level) && !(obj->texheight >> level))
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    dbpp = tex_fmtbpp((ULONG)obj->texfmtsrc);

    if ((sid == SRC_IDX8) != (obj->texfmtsrc == W3D_CHUNKY))
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    lw = tex_leveldim(obj->texwidth, level);
    lh = tex_leveldim(obj->texheight, level);

    if (xoffset < 0 || yoffset < 0 || width < 0 || height < 0 || width > lw - xoffset || height > lh - yoffset)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (!pixels || !width || !height)
        return;

    mem = context->w3dTexMemory[name] + tex_levoff(obj->texwidth, obj->texheight, dbpp, level) + (yoffset * lw + xoffset) * dbpp;

    tex_rows(context, (const UBYTE *)pixels, sid, sbpp, mem, (ULONG)obj->texfmtsrc, dbpp, width, height, lw * dbpp);

    W3D_UpdateTexImage(w3d, obj, context->w3dTexMemory[name] + tex_levoff(obj->texwidth, obj->texheight, dbpp, level), level, tex_palette(context));
}

static UBYTE *tex_fbread(GLcontext context, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const GLint save[5] = {context->CurPackRowLength, context->CurPackSkipPixels, context->CurPackSkipRows, context->CurPackSwapBytes, context->PackAlign};
    UBYTE *buf;

    if ((ULONG)width * (ULONG)height > (1UL << 28))
    {
        mgl_error(context, GL_OUT_OF_MEMORY);
        return NULL;
    }

    buf = (UBYTE *)malloc((size_t)width * (size_t)height * 4);

    if (!buf)
    {
        mgl_error(context, GL_OUT_OF_MEMORY);
        return NULL;
    }

    context->CurPackRowLength = 0;
    context->CurPackSkipPixels = 0;
    context->CurPackSkipRows = 0;
    context->CurPackSwapBytes = 0;
    context->PackAlign = 1;

    GLReadPixels(context, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buf);

    context->CurPackRowLength = save[0];
    context->CurPackSkipPixels = save[1];
    context->CurPackSkipRows = save[2];
    context->CurPackSwapBytes = save[3];
    context->PackAlign = save[4];

    return buf;
}

void tex_decode_pixels(GLcontext context, const UBYTE *pixels, int sid, int sbpp, GLint w, GLint h, UBYTE *rgba)
{
    const GLint rowlen = context->CurUnpackRowLength ? context->CurUnpackRowLength : w;
    const GLint spitch = (rowlen * sbpp + context->UnpackAlign - 1) & ~(context->UnpackAlign - 1);
    const int swap = context->CurUnpackSwapBytes != 0;
    const UBYTE *src = pixels + context->CurUnpackSkipRows * spitch + context->CurUnpackSkipPixels * sbpp;
    GLint x;
    GLint y;

    for (y = 0; y < h; y++, src += spitch)
    {
        const UBYTE *s = src;

        for (x = 0; x < w; x++, s += sbpp, rgba += 4)
            tex_decode(s, sid, swap, rgba);
    }
}

void tex_unpk0(GLcontext context, GLint save[5])
{
    save[0] = context->CurUnpackRowLength;
    save[1] = context->CurUnpackSkipPixels;
    save[2] = context->CurUnpackSkipRows;
    save[3] = context->CurUnpackSwapBytes;
    save[4] = context->UnpackAlign;
    context->CurUnpackRowLength = 0;
    context->CurUnpackSkipPixels = 0;
    context->CurUnpackSkipRows = 0;
    context->CurUnpackSwapBytes = 0;
    context->UnpackAlign = 1;
}

void tex_unpk1(GLcontext context, const GLint save[5])
{
    context->CurUnpackRowLength = save[0];
    context->CurUnpackSkipPixels = save[1];
    context->CurUnpackSkipRows = save[2];
    context->CurUnpackSwapBytes = save[3];
    context->UnpackAlign = save[4];
}

void GLCopyTexImage2D(GLcontext context, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    GLint save[5];
    UBYTE *buf;

    if (width < 0 || height < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    buf = width && height ? tex_fbread(context, x, y, width, height) : NULL;

    if (width && height && !buf)
        return;

    tex_unpk0(context, save);
    GLTexImage2D(context, target, level, internalformat, width, height, border, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    tex_unpk1(context, save);

    free(buf);
}

void GLCopyTexSubImage2D(GLcontext context, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    GLint save[5];
    UBYTE *buf;

    if (width < 0 || height < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    buf = width && height ? tex_fbread(context, x, y, width, height) : NULL;

    if (width && height && !buf)
        return;

    tex_unpk0(context, save);
    GLTexSubImage2D(context, target, level, xoffset, yoffset, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    tex_unpk1(context, save);

    free(buf);
}

void GLTexParameteri(GLcontext context, GLenum target, GLenum pname, GLint param)
{
    W3D_Context *w3d = context->w3dContext;
    W3D_Texture *t = tex_object(context, tex_binding(context));
    ULONG w;

    if (target != GL_TEXTURE_2D)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    switch (pname)
    {
    case GL_TEXTURE_MIN_FILTER:
        w = tex_minw(param);

        if (!w)
        {
            mgl_error(context, GL_INVALID_ENUM);
            return;
        }

        context->MinFilter = param;

        if (t && w3d)
            W3D_SetFilter(w3d, t, w, tex_minw(context->MagFilter));

        break;

    case GL_TEXTURE_MAG_FILTER:
        if (param != GL_NEAREST && param != GL_LINEAR)
        {
            mgl_error(context, GL_INVALID_ENUM);
            return;
        }

        context->MagFilter = param;

        if (t && w3d)
            W3D_SetFilter(w3d, t, tex_minw(context->MinFilter), param == GL_NEAREST ? W3D_NEAREST : W3D_LINEAR);

        break;

    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
        w = tex_wrapw(param);

        if (!w)
        {
            mgl_error(context, GL_INVALID_ENUM);
            return;
        }

        if (pname == GL_TEXTURE_WRAP_S)
            context->WrapS = param;
        else
            context->WrapT = param;

        if (t && w3d)
            W3D_SetWrapMode(w3d, t, tex_wrapw(context->WrapS), tex_wrapw(context->WrapT), NULL);

        break;

    case GL_TEXTURE_PRIORITY:
        // autotexmanagement runs an lru, the hint has no w3d target
        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

void GLTexEnvi(GLcontext context, GLenum target, GLenum pname, GLint param)
{
    ULONG env;

    if (target != GL_TEXTURE_ENV)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    switch (pname)
    {
    case GL_TEXTURE_ENV_MODE:
        env = mgl_envw(param);

        if (!env)
        {
            mgl_error(context, GL_INVALID_ENUM);
            return;
        }

        context->TexEnv[context->ActiveTexture] = param;

        if (!context->ActiveTexture)
        {
            W3D_Texture *t = tex_object(context, (GLuint)context->CurrentBinding);

            if (t && context->w3dContext)
            {
                W3D_SetTexEnv(context->w3dContext, t, env, env == W3D_BLEND ? &context->TexEnvColor[0] : NULL);

                context->CurTexEnv = param;
                context->CurTexEnvColor = context->TexEnvColor[0];
            }
        }

        break;

    case GL_TEXTURE_ENV_COLOR:
        // the int entry point cannot carry the four components
        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

void GLTexEnvfv(GLcontext context, GLenum target, GLenum pname, const GLfloat *params)
{
    if (!params)
        return;

    if (pname == GL_TEXTURE_ENV_MODE)
    {
        GLTexEnvi(context, target, pname, (GLint)params[0]);

        return;
    }

    if (target != GL_TEXTURE_ENV || pname != GL_TEXTURE_ENV_COLOR)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->TexEnvColor[context->ActiveTexture].r = params[0];
    context->TexEnvColor[context->ActiveTexture].g = params[1];
    context->TexEnvColor[context->ActiveTexture].b = params[2];
    context->TexEnvColor[context->ActiveTexture].a = params[3];

    // a live blend env picks the new color up on the bound texture now
    if (!context->ActiveTexture && context->TexEnv[0] == GL_BLEND)
    {
        W3D_Texture *t = tex_object(context, (GLuint)context->CurrentBinding);

        if (t && context->w3dContext)
        {
            W3D_SetTexEnv(context->w3dContext, t, W3D_BLEND, &context->TexEnvColor[0]);

            context->CurTexEnv = GL_BLEND;
            context->CurTexEnvColor = context->TexEnvColor[0];
        }
    }
}

void GLTexGeni(GLcontext context, GLenum coord, GLenum mode, GLenum map)
{
    GLenum *slot;

    if (mode != GL_TEXTURE_GEN_MODE)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    switch (map)
    {
    case GL_SPHERE_MAP:
    case GL_OBJECT_LINEAR:
    case GL_EYE_LINEAR:
        break;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (coord == GL_S)
        slot = &context->TexGenModeS;
    else if (coord == GL_T)
        slot = &context->TexGenModeT;
    else
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    *slot = map;
}

void GLTexGenfv(GLcontext context, GLenum coord, GLenum pname, const GLfloat *params)
{
    GLfloat *plane;
    const GLfloat *m;
    const GLfloat *r;
    GLfloat tx;
    GLfloat ty;
    GLfloat tz;

    if (!params)
        return;

    if (pname == GL_TEXTURE_GEN_MODE)
    {
        GLTexGeni(context, coord, pname, (GLenum)params[0]);

        return;
    }

    if (pname != GL_OBJECT_PLANE && pname != GL_EYE_PLANE)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (coord == GL_S)
        plane = pname == GL_OBJECT_PLANE ? context->ObjectPlaneS : context->EyePlaneS;
    else if (coord == GL_T)
        plane = pname == GL_OBJECT_PLANE ? context->ObjectPlaneT : context->EyePlaneT;
    else
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (pname == GL_OBJECT_PLANE)
    {
        memcpy(plane, params, 4 * sizeof(GLfloat));
        return;
    }

    // eye planes store p * modelview^-1 once, evaluation stays a plain dot
    mglUpdateInvRot(context);

    r = context->InvRot;
    m = context->ModelView[context->ModelViewNr].v;
    tx = r[0] * m[12] + r[1] * m[13] + r[2] * m[14];
    ty = r[3] * m[12] + r[4] * m[13] + r[5] * m[14];
    tz = r[6] * m[12] + r[7] * m[13] + r[8] * m[14];

    plane[0] = r[0] * params[0] + r[3] * params[1] + r[6] * params[2];
    plane[1] = r[1] * params[0] + r[4] * params[1] + r[7] * params[2];
    plane[2] = r[2] * params[0] + r[5] * params[1] + r[8] * params[2];
    plane[3] = params[3] - tx * params[0] - ty * params[1] - tz * params[2];
}

static GLenum tex_dstinternal(ULONG fmt)
{
    switch (fmt)
    {
    case W3D_A8:
        return GL_ALPHA8;
    case W3D_L8:
        return GL_LUMINANCE8;
    case W3D_L8A8:
        return GL_LUMINANCE8_ALPHA8;
    case W3D_I8:
        return GL_INTENSITY8;
    case W3D_R5G6B5:
        return GL_RGB5;
    case W3D_A1R5G5B5:
        return GL_RGB5_A1;
    case W3D_A4R4G4B4:
        return GL_RGBA4;
    case W3D_R8G8B8:
        return GL_RGB8;
    case W3D_A8R8G8B8:
    case W3D_R8G8B8A8:
        return GL_RGBA8;
    case W3D_CHUNKY:
        return GL_COLOR_INDEX;
    }

    return 0;
}

static GLint tex_fmtsize(ULONG fmt, GLenum pname)
{
    switch (fmt)
    {
    case W3D_A8:
        return pname == GL_TEXTURE_ALPHA_SIZE ? 8 : 0;
    case W3D_L8:
        return pname == GL_TEXTURE_LUMINANCE_SIZE ? 8 : 0;
    case W3D_L8A8:
        return pname == GL_TEXTURE_LUMINANCE_SIZE || pname == GL_TEXTURE_ALPHA_SIZE ? 8 : 0;
    case W3D_I8:
        return pname == GL_TEXTURE_INTENSITY_SIZE ? 8 : 0;
    case W3D_R5G6B5:
        return pname == GL_TEXTURE_RED_SIZE ? 5 : pname == GL_TEXTURE_GREEN_SIZE ? 6
                                              : pname == GL_TEXTURE_BLUE_SIZE    ? 5
                                                                                 : 0;
    case W3D_A1R5G5B5:
        return pname == GL_TEXTURE_ALPHA_SIZE ? 1 : pname == GL_TEXTURE_RED_SIZE || pname == GL_TEXTURE_GREEN_SIZE || pname == GL_TEXTURE_BLUE_SIZE ? 5
                                                                                                                                                    : 0;
    case W3D_A4R4G4B4:
        return pname == GL_TEXTURE_RED_SIZE || pname == GL_TEXTURE_GREEN_SIZE || pname == GL_TEXTURE_BLUE_SIZE || pname == GL_TEXTURE_ALPHA_SIZE ? 4 : 0;
    case W3D_R8G8B8:
        return pname == GL_TEXTURE_RED_SIZE || pname == GL_TEXTURE_GREEN_SIZE || pname == GL_TEXTURE_BLUE_SIZE ? 8 : 0;
    case W3D_A8R8G8B8:
    case W3D_R8G8B8A8:
        return pname == GL_TEXTURE_RED_SIZE || pname == GL_TEXTURE_GREEN_SIZE || pname == GL_TEXTURE_BLUE_SIZE || pname == GL_TEXTURE_ALPHA_SIZE ? 8 : 0;
    }

    return 0;
}

void GLGetTexParameteriv(GLcontext context, GLenum target, GLenum pname, GLint *params)
{
    if (target != GL_TEXTURE_2D)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (!params)
        return;

    switch (pname)
    {
    case GL_TEXTURE_MIN_FILTER:
        *params = (GLint)context->MinFilter;
        break;
    case GL_TEXTURE_MAG_FILTER:
        *params = (GLint)context->MagFilter;
        break;
    case GL_TEXTURE_WRAP_S:
        *params = (GLint)context->WrapS;
        break;
    case GL_TEXTURE_WRAP_T:
        *params = (GLint)context->WrapT;
        break;
    case GL_TEXTURE_PRIORITY:
        *params = 1;
        break;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

void GLGetTexEnviv(GLcontext context, GLenum target, GLenum pname, GLint *params)
{
    if (target != GL_TEXTURE_ENV)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (!params)
        return;

    switch (pname)
    {
    case GL_TEXTURE_ENV_MODE:
        params[0] = (GLint)context->TexEnv[context->ActiveTexture];
        break;
    case GL_TEXTURE_ENV_COLOR:
        params[0] = (GLint)(context->TexEnvColor[context->ActiveTexture].r >= 0.0f ? context->TexEnvColor[context->ActiveTexture].r + 0.5f : context->TexEnvColor[context->ActiveTexture].r - 0.5f);
        params[1] = (GLint)(context->TexEnvColor[context->ActiveTexture].g >= 0.0f ? context->TexEnvColor[context->ActiveTexture].g + 0.5f : context->TexEnvColor[context->ActiveTexture].g - 0.5f);
        params[2] = (GLint)(context->TexEnvColor[context->ActiveTexture].b >= 0.0f ? context->TexEnvColor[context->ActiveTexture].b + 0.5f : context->TexEnvColor[context->ActiveTexture].b - 0.5f);
        params[3] = (GLint)(context->TexEnvColor[context->ActiveTexture].a >= 0.0f ? context->TexEnvColor[context->ActiveTexture].a + 0.5f : context->TexEnvColor[context->ActiveTexture].a - 0.5f);
        break;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

void GLGetTexEnvfv(GLcontext context, GLenum target, GLenum pname, GLfloat *params)
{
    if (target != GL_TEXTURE_ENV)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (!params)
        return;

    switch (pname)
    {
    case GL_TEXTURE_ENV_MODE:
        params[0] = (GLfloat)context->TexEnv[context->ActiveTexture];
        break;
    case GL_TEXTURE_ENV_COLOR:
        params[0] = context->TexEnvColor[context->ActiveTexture].r;
        params[1] = context->TexEnvColor[context->ActiveTexture].g;
        params[2] = context->TexEnvColor[context->ActiveTexture].b;
        params[3] = context->TexEnvColor[context->ActiveTexture].a;
        break;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

static int tex_genget(GLcontext context, GLenum coord, GLenum pname, GLfloat *out)
{
    const GLfloat *plane;

    if (pname == GL_TEXTURE_GEN_MODE)
    {
        out[0] = (GLfloat)(coord == GL_S ? context->TexGenModeS : context->TexGenModeT);
        return 1;
    }

    plane = pname == GL_OBJECT_PLANE ? (coord == GL_S ? context->ObjectPlaneS : context->ObjectPlaneT) : (coord == GL_S ? context->EyePlaneS : context->EyePlaneT);

    memcpy(out, plane, 4 * sizeof(GLfloat));

    return 4;
}

void GLGetTexGeniv(GLcontext context, GLenum coord, GLenum pname, GLint *params)
{
    GLfloat out[4];
    int i;
    int n;

    if ((coord != GL_S && coord != GL_T) || (pname != GL_TEXTURE_GEN_MODE && pname != GL_OBJECT_PLANE && pname != GL_EYE_PLANE))
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (!params)
        return;

    n = tex_genget(context, coord, pname, out);

    for (i = 0; i < n; i++)
        params[i] = (GLint)(out[i] >= 0.0f ? out[i] + 0.5f : out[i] - 0.5f);
}

void GLGetTexGenfv(GLcontext context, GLenum coord, GLenum pname, GLfloat *params)
{
    if ((coord != GL_S && coord != GL_T) || (pname != GL_TEXTURE_GEN_MODE && pname != GL_OBJECT_PLANE && pname != GL_EYE_PLANE))
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (!params)
        return;

    tex_genget(context, coord, pname, params);
}

void GLGetTexLevelParameteriv(GLcontext context, GLenum target, GLint level, GLenum pname, GLint *params)
{
    W3D_Texture *t;

    if (target != GL_TEXTURE_2D)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (!params)
        return;

    if (level < 0 || level >= 16)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    t = tex_object(context, tex_binding(context));

    if (!t)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (level && (!t->mipmap || (!(t->texwidth >> level) && !(t->texheight >> level))))
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    switch (pname)
    {
    case GL_TEXTURE_WIDTH:
        *params = (GLint)tex_leveldim(t->texwidth, level);
        break;
    case GL_TEXTURE_HEIGHT:
        *params = (GLint)tex_leveldim(t->texheight, level);
        break;
    case GL_TEXTURE_INTERNAL_FORMAT:
        *params = (GLint)tex_dstinternal((ULONG)t->texfmtsrc);
        break;
    case GL_TEXTURE_BORDER:
        *params = 0;
        break;
    case GL_TEXTURE_RED_SIZE:
    case GL_TEXTURE_GREEN_SIZE:
    case GL_TEXTURE_BLUE_SIZE:
    case GL_TEXTURE_ALPHA_SIZE:
    case GL_TEXTURE_LUMINANCE_SIZE:
    case GL_TEXTURE_INTENSITY_SIZE:
    case GL_TEXTURE_DEPTH_SIZE:
        *params = tex_fmtsize((ULONG)t->texfmtsrc, pname);
        break;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

static void tex_ddecode(const UBYTE *p, ULONG fmt, UBYTE *rgba)
{
    GLuint v;

    switch (fmt)
    {
    case W3D_R8G8B8A8:
        rgba[0] = p[0];
        rgba[1] = p[1];
        rgba[2] = p[2];
        rgba[3] = p[3];
        break;
    case W3D_A8R8G8B8:
        rgba[0] = p[1];
        rgba[1] = p[2];
        rgba[2] = p[3];
        rgba[3] = p[0];
        break;
    case W3D_R8G8B8:
        rgba[0] = p[0];
        rgba[1] = p[1];
        rgba[2] = p[2];
        rgba[3] = 0xFF;
        break;
    case W3D_R5G6B5:
        v = ((GLuint)p[0] << 8) | p[1];
        rgba[0] = (UBYTE)(((v >> 11) << 3) | (v >> 13));
        rgba[1] = (UBYTE)((((v >> 5) & 0x3F) << 2) | (((v >> 5) & 0x3F) >> 4));
        rgba[2] = (UBYTE)(((v & 0x1F) << 3) | ((v & 0x1F) >> 2));
        rgba[3] = 0xFF;
        break;
    case W3D_A1R5G5B5:
        v = ((GLuint)p[0] << 8) | p[1];
        rgba[0] = (UBYTE)((((v >> 10) & 0x1F) << 3) | (((v >> 10) & 0x1F) >> 2));
        rgba[1] = (UBYTE)((((v >> 5) & 0x1F) << 3) | (((v >> 5) & 0x1F) >> 2));
        rgba[2] = (UBYTE)(((v & 0x1F) << 3) | ((v & 0x1F) >> 2));
        rgba[3] = (UBYTE)(v >> 15 ? 0xFF : 0);
        break;
    case W3D_A4R4G4B4:
        v = ((GLuint)p[0] << 8) | p[1];
        rgba[0] = (UBYTE)(((v >> 8) & 0x0F) * 17);
        rgba[1] = (UBYTE)(((v >> 4) & 0x0F) * 17);
        rgba[2] = (UBYTE)((v & 0x0F) * 17);
        rgba[3] = (UBYTE)(((v >> 12) & 0x0F) * 17);
        break;
    case W3D_A8:
        rgba[0] = rgba[1] = rgba[2] = 0;
        rgba[3] = p[0];
        break;
    case W3D_L8:
        rgba[0] = rgba[1] = rgba[2] = p[0];
        rgba[3] = 0xFF;
        break;
    case W3D_L8A8:
        rgba[0] = rgba[1] = rgba[2] = p[0];
        rgba[3] = p[1];
        break;
    case W3D_I8:
        rgba[0] = rgba[1] = rgba[2] = rgba[3] = p[0];
        break;
    default:
        rgba[0] = rgba[1] = rgba[2] = 0;
        rgba[3] = 0xFF;
        break;
    }
}

void tex_sencode(UBYTE *d, int sid, int swap, const UBYTE *rgba)
{
    GLuint v;

    switch (sid)
    {
    case SRC_RGBA8:
        d[0] = rgba[0];
        d[1] = rgba[1];
        d[2] = rgba[2];
        d[3] = rgba[3];
        break;
    case SRC_ARGB8:
        d[0] = rgba[3];
        d[1] = rgba[0];
        d[2] = rgba[1];
        d[3] = rgba[2];
        break;
    case SRC_BGRA8:
        d[0] = rgba[2];
        d[1] = rgba[1];
        d[2] = rgba[0];
        d[3] = rgba[3];
        break;
    case SRC_RGB8:
        d[0] = rgba[0];
        d[1] = rgba[1];
        d[2] = rgba[2];
        break;
    case SRC_R8:
    case SRC_L8:
    case SRC_I8:
        d[0] = rgba[0];
        break;
    case SRC_G8:
        d[0] = rgba[1];
        break;
    case SRC_B8:
        d[0] = rgba[2];
        break;
    case SRC_A8:
        d[0] = rgba[3];
        break;
    case SRC_LA8:
        d[0] = rgba[0];
        d[1] = rgba[3];
        break;
    case SRC_RGB565:
        v = ((GLuint)(rgba[0] >> 3) << 11) | ((GLuint)(rgba[1] >> 2) << 5) | (rgba[2] >> 3);

        if (swap)
        {
            d[0] = (UBYTE)v;
            d[1] = (UBYTE)(v >> 8);
        }
        else
        {
            d[0] = (UBYTE)(v >> 8);
            d[1] = (UBYTE)v;
        }

        break;
    case SRC_ARGB4:
        v = ((GLuint)(rgba[3] >> 4) << 12) | ((GLuint)(rgba[0] >> 4) << 8) | ((GLuint)(rgba[1] >> 4) << 4) | (rgba[2] >> 4);

        if (swap)
        {
            d[0] = (UBYTE)v;
            d[1] = (UBYTE)(v >> 8);
        }
        else
        {
            d[0] = (UBYTE)(v >> 8);
            d[1] = (UBYTE)v;
        }

        break;
    }
}

void GLGetTexImage(GLcontext context, GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels)
{
    const W3D_Texture *t;
    const UBYTE *src;
    const GLuint name = tex_binding(context);
    UBYTE *dst;
    GLenum e;
    GLint lw;
    GLint lh;
    GLint rowlen;
    GLint dpitch;
    GLint copy;
    GLint x;
    GLint y;
    int sid = 0;
    int sbpp = 0;
    int dbpp;
    int swap;

    if (target != GL_TEXTURE_2D)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (level < 0 || level >= 16)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    e = tex_srcfmt(format, type, &sid, &sbpp);

    if (e != GL_NO_ERROR)
    {
        mgl_error(context, e);
        return;
    }

    t = tex_object(context, name);

    if (!t)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (level && (!t->mipmap || (!(t->texwidth >> level) && !(t->texheight >> level))))
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if ((sid == SRC_IDX8) != ((ULONG)t->texfmtsrc == W3D_CHUNKY))
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (!pixels)
        return;

    lw = tex_leveldim(t->texwidth, level);
    lh = tex_leveldim(t->texheight, level);
    dbpp = tex_fmtbpp((ULONG)t->texfmtsrc);
    src = context->w3dTexMemory[name] + tex_levoff(t->texwidth, t->texheight, dbpp, level);

    rowlen = context->CurPackRowLength ? context->CurPackRowLength : lw;
    dpitch = (rowlen * sbpp + context->PackAlign - 1) & ~(context->PackAlign - 1);
    dst = (UBYTE *)pixels + context->CurPackSkipRows * dpitch + context->CurPackSkipPixels * sbpp;

    swap = context->CurPackSwapBytes != 0;
    copy = lw * sbpp;

    if (tex_verbatim(sid, (ULONG)t->texfmtsrc, swap))
    {
        for (y = 0; y < lh; y++, src += lw * dbpp, dst += dpitch)
            memcpy(dst, src, (size_t)copy);
    }
    else
    {
        for (y = 0; y < lh; y++, src += lw * dbpp, dst += dpitch)
        {
            const UBYTE *s = src;
            UBYTE *d = dst;

            for (x = 0; x < lw; x++, s += dbpp, d += sbpp)
            {
                UBYTE rgba[4];

                tex_ddecode(s, (ULONG)t->texfmtsrc, rgba);
                tex_sencode(d, sid, swap, rgba);
            }
        }
    }
}

void GLPixelStorei(GLcontext context, GLenum pname, GLint param)
{
    // alignment takes {1,2,4,8}, the rest of the layout fields take >= 0
    if (pname == GL_PACK_ALIGNMENT || pname == GL_UNPACK_ALIGNMENT)
    {
        if (param != 1 && param != 2 && param != 4 && param != 8)
        {
            mgl_error(context, GL_INVALID_VALUE);
            return;
        }
    }
    else
    {
        switch (pname)
        {
        case GL_PACK_ROW_LENGTH:
        case GL_UNPACK_ROW_LENGTH:
        case GL_PACK_SKIP_PIXELS:
        case GL_UNPACK_SKIP_PIXELS:
        case GL_PACK_SKIP_ROWS:
        case GL_UNPACK_SKIP_ROWS:
            if (param < 0)
            {
                mgl_error(context, GL_INVALID_VALUE);
                return;
            }

            break;
        case GL_PACK_SWAP_BYTES:
        case GL_UNPACK_SWAP_BYTES:
        case GL_PACK_LSB_FIRST:
        case GL_UNPACK_LSB_FIRST:
            break;
        default:
            mgl_error(context, GL_INVALID_ENUM);
            return;
        }
    }

    switch (pname)
    {
    case GL_PACK_ALIGNMENT:
        context->PackAlign = param;
        break;
    case GL_UNPACK_ALIGNMENT:
        context->UnpackAlign = param;
        break;
    case GL_PACK_ROW_LENGTH:
        context->CurPackRowLength = param;
        break;
    case GL_UNPACK_ROW_LENGTH:
        context->CurUnpackRowLength = param;
        break;
    case GL_PACK_SKIP_PIXELS:
        context->CurPackSkipPixels = param;
        break;
    case GL_UNPACK_SKIP_PIXELS:
        context->CurUnpackSkipPixels = param;
        break;
    case GL_PACK_SKIP_ROWS:
        context->CurPackSkipRows = param;
        break;
    case GL_UNPACK_SKIP_ROWS:
        context->CurUnpackSkipRows = param;
        break;
    case GL_PACK_SWAP_BYTES:
        context->CurPackSwapBytes = param != 0;
        break;
    case GL_UNPACK_SWAP_BYTES:
        context->CurUnpackSwapBytes = param != 0;
        break;
    case GL_PACK_LSB_FIRST:
        context->CurPackLsbFirst = param != 0;
        break;
    case GL_UNPACK_LSB_FIRST:
        context->CurUnpackLsbFirst = param != 0;
        break;
    }
}

void GLColorTable(GLcontext context, GLenum target, GLenum internalformat, GLint width, GLenum format, GLenum type, GLvoid *data)
{
    ULONG *pal = (ULONG *)context->PaletteData;
    const UBYTE *src = (const UBYTE *)data;
    GLenum e;
    ULONG dfmt;
    int dbpp;
    int sid = 0;
    int sbpp = 0;
    int i;

    if (target != GL_COLOR_TABLE && target != GL_SHARED_TEXTURE_PALETTE_EXT && target != GL_TEXTURE_2D)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    e = tex_dstfmt(context, internalformat, &dfmt, &dbpp);

    if (e != GL_NO_ERROR)
    {
        mgl_error(context, e);
        return;
    }

    if (width < 1 || width > 256)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    e = tex_srcfmt(format, type, &sid, &sbpp);

    if (e != GL_NO_ERROR || sid == SRC_IDX8)
    {
        mgl_error(context, e != GL_NO_ERROR ? e : GL_INVALID_OPERATION);
        return;
    }

    if (!pal)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (src)
    {
        const int swap = context->CurUnpackSwapBytes != 0;

        src += context->CurUnpackSkipPixels * sbpp;

        for (i = 0; i < width; i++, src += sbpp)
        {
            UBYTE rgba[4];

            tex_decode(src, sid, swap, rgba);

            pal[i] = ((ULONG)rgba[3] << 24) | ((ULONG)rgba[0] << 16) | ((ULONG)rgba[1] << 8) | rgba[2];
        }
    }
    else
    {
        for (i = 0; i < width; i++)
            pal[i] = 0xFF000000;
    }

    context->PaletteSize = width;
    context->PaletteFormat = format;

    // the table is shared, every chunky texture on the driver picks it up now
    if (context->w3dContext)
    {
        int i;

        for (i = 0; i < context->TexBufferSize; i++)
        {
            W3D_Texture *t = context->w3dTexBuffer[i];

            if (t && t->texfmtsrc == W3D_CHUNKY)
                W3D_UpdateTexImage(context->w3dContext, t, t->texsource, 0, pal);
        }
    }
}

void GLActiveTextureARB(GLcontext context, GLenum unit)
{
    const int u = unit - GL_TEXTURE0_ARB;

    if (u < 0 || u >= (int)context->TexUnits)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->ActiveTexture = (GLuint)u;
}

void MGLDrawMultitexBuffer(GLcontext context, GLenum BSrc, GLenum BDst, GLenum TexEnv)
{
    const ULONG src = mgl_blend_src(BSrc);
    const ULONG dst = mgl_blend_dst(BDst);
    const ULONG env = mgl_envw(TexEnv);

    if (!src || !dst || !env)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (!context->VertexBufferPointer)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    context->mtexSrc = src;
    context->mtexDst = dst;
    context->mtexEnv = env;
    context->mtexArmed = GL_TRUE;

    d_Submit(context, NULL, 0, GL_TRUE);

    context->mtexArmed = GL_FALSE;
}

void MGLTexMemStat(GLcontext context, GLint *Current, GLint *Peak)
{
    if (Current)
        *Current = context->TexMemCur;

    if (Peak)
        *Peak = context->TexMemPeak;
}

ULONG MGLSetTexChromaBounds(GLcontext context, GLuint texture, ULONG rgba_lower, ULONG rgba_upper, ULONG mode)
{
    W3D_Texture *t;

    if (mode != W3D_CHROMATEST_NONE && mode != W3D_CHROMATEST_INCLUSIVE && mode != W3D_CHROMATEST_EXCLUSIVE)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return W3D_NOT_SUPPORTED;
    }

    if (!(t = tex_object(context, texture)))
    {
        mgl_error(context, GL_INVALID_VALUE);
        return W3D_NOT_SUPPORTED;
    }

    W3D_SetState(context->w3dContext, W3D_CHROMATEST, mode != W3D_CHROMATEST_NONE ? W3D_ENABLE : W3D_DISABLE);

    return W3D_SetChromaTestBounds(context->w3dContext, t, rgba_lower, rgba_upper, mode);
}

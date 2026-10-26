#include "mglint.h"

static GLbitfield va_csbit(GLenum cap)
{
    switch (cap)
    {
    case GL_VERTEX_ARRAY:
        return GLCS_VERTEX;

    case GL_COLOR_ARRAY:
        return GLCS_COLOR;

    case GL_TEXTURE_COORD_ARRAY:
        return GLCS_TEXTURE;

    case GL_NORMAL_ARRAY:
        return GLCS_NORMAL;

    case GL_EDGE_FLAG_ARRAY:
        return GLCS_EDGEFLAG;
    }

    return 0;
}

static GLint va_esz(GLenum type)
{
    if (type == GL_DOUBLE)
        return 8;

    if (type == GL_SHORT || type == GL_UNSIGNED_SHORT)
        return 2;

    if (type == GL_BYTE || type == GL_UNSIGNED_BYTE || type == MGL_UBYTE_ARGB || type == MGL_UBYTE_BGRA)
        return 1;

    return 4;
}

void va_stagebind(GLcontext context)
{
    W3D_Context *w3d = context->w3dContext;

    context->ArrayClientBound = GL_FALSE;

    W3D_VertexPointer(w3d, context->VertexBuffer, sizeof(MGLVertex), W3D_VERTEX_F_F_D, 0);
    W3D_TexCoordPointer(w3d, &context->VertexBuffer[0].v.u, sizeof(MGLVertex), 0, 4, -4, W3D_TEXCOORD_NORMALIZED);
    W3D_ColorPointer(w3d, &context->VertexBuffer[0].v.color, sizeof(MGLVertex), W3D_COLOR_FLOAT, W3D_CMODE_RGBA, 0);
}

ULONG va_colormode(GLcontext context)
{
    const GLenum type = context->ArrColorType;
    const int size = context->ArrColorSize;

    switch (type)
    {
    case GL_UNSIGNED_BYTE:
        return W3D_COLOR_UBYTE | (size == 3 ? W3D_CMODE_RGB : W3D_CMODE_RGBA);

    case MGL_UBYTE_ARGB:
        return size == 4 ? (W3D_COLOR_UBYTE | W3D_CMODE_ARGB) : 0;

    case MGL_UBYTE_BGRA:
        return size == 4 ? (W3D_COLOR_UBYTE | W3D_CMODE_BGRA) : 0;

    case GL_FLOAT:
        return W3D_COLOR_FLOAT | (size == 3 ? W3D_CMODE_RGB : W3D_CMODE_RGBA);

    default:
        return 0;
    }
}

void va_bindptr(GLcontext context)
{
    MGLAPointer *ap = &context->ArrayPointer;
    W3D_Context *w3d = context->w3dContext;
    const GLbitfield cs = context->ClientState;

    if (context->VertexArrayPipeline)
    {
        va_stagebind(context);

        return;
    }

    context->ArrayClientBound = GL_TRUE;

    if (ap->verts && context->ArrVertType == GL_FLOAT && context->ArrVertSize >= 3)
    {
        ap->vertexmode = W3D_VERTEX_F_F_F;

        W3D_VertexPointer(w3d, ap->verts, ap->vertexstride, ap->vertexmode, 0);
    }

    if (cs & GLCS_TEXTURE)
    {
        if (ap->ctex[0].ptr && ap->ctex[0].type == GL_FLOAT && ap->ctex[0].size >= 3)
        {
            ap->w_off = (ap->ctex[0].size - 1) * (GLint)sizeof(GLfloat);

            W3D_TexCoordPointer(w3d, ap->ctex[0].ptr, ap->ctex[0].stride, 0, sizeof(GLfloat), ap->w_off, W3D_TEXCOORD_NORMALIZED);
        }
    }
    else
    {
        static const GLfloat deftex[4] = {0.0f, 0.0f, 0.0f, 1.0f};

        ap->w_off = 3 * (GLint)sizeof(GLfloat);

        W3D_TexCoordPointer(w3d, (void *)deftex, 0, 0, sizeof(GLfloat), ap->w_off, W3D_TEXCOORD_NORMALIZED);
    }

    if (cs & GLCS_COLOR)
    {
        const ULONG cm = va_colormode(context);

        if (ap->colors && cm)
        {
            ap->colormode = cm;

            W3D_ColorPointer(w3d, ap->colors, ap->colorstride, cm & W3D_COLOR_UBYTE ? W3D_COLOR_UBYTE : W3D_COLOR_FLOAT, cm & ~(W3D_COLOR_FLOAT | W3D_COLOR_UBYTE), 0);
        }
    }
    else
    {
        ap->colormode = W3D_COLOR_FLOAT | W3D_CMODE_RGBA;

        W3D_ColorPointer(w3d, &context->CurrentColor, 0, W3D_COLOR_FLOAT, W3D_CMODE_RGBA, 0);
    }
}

void GLEnableClientState(GLcontext context, GLenum cap)
{
    const GLbitfield bit = va_csbit(cap);

    if (!bit)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (cap == GL_TEXTURE_COORD_ARRAY)
    {
        const GLuint cat = context->ClientActiveTexture;

        context->TexCSMask |= 1u << cat;

        if (!cat)
            context->ClientState |= GLCS_TEXTURE;
    }
    else
        context->ClientState |= bit;

    if (!context->VertexArrayPipeline)
        va_bindptr(context);
}

void GLDisableClientState(GLcontext context, GLenum cap)
{
    const GLbitfield bit = va_csbit(cap);

    if (!bit)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (cap == GL_TEXTURE_COORD_ARRAY)
    {
        const GLuint cat = context->ClientActiveTexture;

        context->TexCSMask &= ~(1u << cat);

        if (!cat)
            context->ClientState &= ~GLCS_TEXTURE;
    }
    else
        context->ClientState &= ~bit;

    if (!context->VertexArrayPipeline)
        va_bindptr(context);
}

void GLVertexPointer(GLcontext context, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    if (size < 2 || size > 4 || stride < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (type != GL_BYTE && type != GL_SHORT && type != GL_INT && type != GL_FLOAT && type != GL_DOUBLE)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->ArrayPointer.verts = (UBYTE *)pointer;
    context->ArrayPointer.vertexstride = stride ? stride : size * va_esz(type);
    context->ArrVertSize = (GLubyte)size;
    context->ArrVertType = type;

    if (!context->VertexArrayPipeline)
    {
        if (size < 3 || type != GL_FLOAT)
        {
            mgl_error(context, GL_INVALID_VALUE);
            return;
        }

        va_bindptr(context);
    }
}

void GLColorPointer(GLcontext context, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    if (size < 3 || size > 4 || stride < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (type != GL_BYTE && type != GL_UNSIGNED_BYTE && type != GL_SHORT && type != GL_UNSIGNED_SHORT && type != GL_INT && type != GL_UNSIGNED_INT && type != GL_FLOAT && type != GL_DOUBLE && type != MGL_UBYTE_ARGB && type != MGL_UBYTE_BGRA)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->ArrayPointer.colors = (UBYTE *)pointer;
    context->ArrayPointer.colorstride = stride ? stride : size * va_esz(type);
    context->ArrColorSize = (GLubyte)size;
    context->ArrColorType = type;

    if (!context->VertexArrayPipeline)
    {
        if (!va_colormode(context))
        {
            mgl_error(context, type == GL_UNSIGNED_BYTE || type == GL_FLOAT ? GL_INVALID_VALUE : GL_INVALID_ENUM);
            return;
        }

        va_bindptr(context);
    }
}

void GLTexCoordPointer(GLcontext context, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    if (size < 1 || size > 4 || stride < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (type != GL_BYTE && type != GL_SHORT && type != GL_INT && type != GL_FLOAT && type != GL_DOUBLE)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    MGLClientTex *tx = &context->ArrayPointer.ctex[context->ClientActiveTexture];

    tx->ptr = (UBYTE *)pointer;
    tx->stride = stride ? stride : size * va_esz(type);
    tx->size = (GLubyte)size;
    tx->type = type;

    if (!context->VertexArrayPipeline && context->ClientActiveTexture == 0)
    {
        if (type != GL_FLOAT || size < 3)
        {
            mgl_error(context, GL_INVALID_VALUE);
            return;
        }

        context->ArrayPointer.w_buffer = (UBYTE *)pointer + (size - 1) * (GLint)sizeof(GLfloat);

        va_bindptr(context);
    }
}

void GLNormalPointer(GLcontext context, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    if (stride < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (type != GL_BYTE && type != GL_SHORT && type != GL_INT && type != GL_FLOAT && type != GL_DOUBLE)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->ArrNormal = (GLubyte *)pointer;
    context->ArrNormalStride = stride ? stride : 3 * va_esz(type);
    context->ArrNormalType = type;
}

// stage unit-k texcoords from its client array into a tcoord slot
static void va_stagetex(MGLVertex *v, GLuint first, GLuint count, const MGLClientTex *tx, GLuint slot)
{
    const GLubyte *p = tx->ptr + first * tx->stride;
    const int sz = tx->size;
    GLuint i;

    const GLuint stride = tx->stride;

    switch (tx->type)
    {
    case GL_FLOAT:
        for (i = 0; i < count; i++, p += stride)
        {
            const GLfloat *f = (const GLfloat *)p;

            v[i].tcoord[slot].s = f[0];
            v[i].tcoord[slot].t = sz > 1 ? f[1] : 0.0f;
        }

        break;

    case GL_DOUBLE:
        for (i = 0; i < count; i++, p += stride)
        {
            const GLdouble *d = (const GLdouble *)p;

            v[i].tcoord[slot].s = (GLfloat)d[0];
            v[i].tcoord[slot].t = sz > 1 ? (GLfloat)d[1] : 0.0f;
        }

        break;

    case GL_INT:
        for (i = 0; i < count; i++, p += stride)
        {
            const GLint *l = (const GLint *)p;

            v[i].tcoord[slot].s = (GLfloat)l[0];
            v[i].tcoord[slot].t = sz > 1 ? (GLfloat)l[1] : 0.0f;
        }

        break;

    case GL_BYTE:
        for (i = 0; i < count; i++, p += stride)
        {
            const GLbyte *b = (const GLbyte *)p;

            v[i].tcoord[slot].s = (GLfloat)b[0];
            v[i].tcoord[slot].t = sz > 1 ? (GLfloat)b[1] : 0.0f;
        }

        break;

    default:
        for (i = 0; i < count; i++, p += stride)
        {
            const GLshort *s = (const GLshort *)p;

            v[i].tcoord[slot].s = (GLfloat)s[0];
            v[i].tcoord[slot].t = sz > 1 ? (GLfloat)s[1] : 0.0f;
        }

        break;
    }
}

// fetch a run of client elements into staged slots, type dispatch once per call
void va_StageRange(GLcontext context, GLuint first, GLuint count, GLuint slot0)
{
    MGLVertex *v = context->VertexBuffer + slot0;
    const MGLAPointer *ap = &context->ArrayPointer;
    const GLbitfield cs = context->ClientState;
    const GLuint vstride = ap->vertexstride;
    const int vsz = context->ArrVertSize;
    const GLubyte *p = ap->verts + first * vstride;
    int wone = context->WOne_Hint;
    GLuint i;

    switch (context->ArrVertType)
    {
    case GL_FLOAT:
        for (i = 0; i < count; i++, p += vstride)
        {
            const GLfloat *f = (const GLfloat *)p;

            v[i].bx = f[0];
            v[i].by = f[1];
            v[i].bz = vsz > 2 ? f[2] : 0.0f;
            v[i].bw = vsz > 3 ? f[3] : 1.0f;

            // a non-unit staged w kills the one-w transform hint
            if (v[i].bw != 1.0f)
                wone = GL_FALSE;
        }

        break;

    case GL_DOUBLE:
        for (i = 0; i < count; i++, p += vstride)
        {
            const GLdouble *d = (const GLdouble *)p;

            v[i].bx = (GLfloat)d[0];
            v[i].by = (GLfloat)d[1];
            v[i].bz = vsz > 2 ? (GLfloat)d[2] : 0.0f;
            v[i].bw = vsz > 3 ? (GLfloat)d[3] : 1.0f;

            if (v[i].bw != 1.0f)
                wone = GL_FALSE;
        }

        break;

    case GL_INT:
        for (i = 0; i < count; i++, p += vstride)
        {
            const GLint *l = (const GLint *)p;

            v[i].bx = (GLfloat)l[0];
            v[i].by = (GLfloat)l[1];
            v[i].bz = vsz > 2 ? (GLfloat)l[2] : 0.0f;
            v[i].bw = vsz > 3 ? (GLfloat)l[3] : 1.0f;

            if (v[i].bw != 1.0f)
                wone = GL_FALSE;
        }

        break;

    case GL_BYTE:
        for (i = 0; i < count; i++, p += vstride)
        {
            const GLbyte *b = (const GLbyte *)p;

            v[i].bx = (GLfloat)b[0];
            v[i].by = (GLfloat)b[1];
            v[i].bz = vsz > 2 ? (GLfloat)b[2] : 0.0f;
            v[i].bw = vsz > 3 ? (GLfloat)b[3] : 1.0f;

            if (v[i].bw != 1.0f)
                wone = GL_FALSE;
        }

        break;

    default:
        for (i = 0; i < count; i++, p += vstride)
        {
            const GLshort *s = (const GLshort *)p;

            v[i].bx = (GLfloat)s[0];
            v[i].by = (GLfloat)s[1];
            v[i].bz = vsz > 2 ? (GLfloat)s[2] : 0.0f;
            v[i].bw = vsz > 3 ? (GLfloat)s[3] : 1.0f;

            if (v[i].bw != 1.0f)
                wone = GL_FALSE;
        }

        break;
    }

    // per-unit current texcoords seed every staged slot array-enabled units overwrite below
    const GLuint tcm = context->TCopyMax;
    const GLbitfield tcsm = context->TexCSMask;
    GLuint k;

    for (k = 0; k < tcm; k++)
    {
        const struct TCoord_t t = context->CurrentTcoord[k + 1];

        for (i = 0; i < count; i++)
            v[i].tcoord[k] = t;
    }

    for (k = 1; k <= tcm; k++)
    {
        const MGLClientTex *tx = &ap->ctex[k];

        if ((tcsm & (1u << k)) && tx->ptr)
            va_stagetex(v, first, count, tx, k - 1);
    }

    if (cs & GLCS_TEXTURE)
    {
        const MGLClientTex *tx0 = &ap->ctex[0];
        const GLuint tx0st = tx0->stride;
        const int tx0sz = tx0->size;

        p = tx0->ptr + first * tx0st;

        switch (tx0->type)
        {
        case GL_FLOAT:
            for (i = 0; i < count; i++, p += tx0st)
            {
                const GLfloat *f = (const GLfloat *)p;

                v[i].v.u = f[0];
                v[i].v.v = tx0sz > 1 ? f[1] : 0.0f;
                v[i].v.tex3d = tx0sz > 2 ? f[2] : 0.0f;

                if (tx0sz > 3)
                    v[i].q = 1.0f / f[3];
            }

            break;

        case GL_DOUBLE:
            for (i = 0; i < count; i++, p += tx0st)
            {
                const GLdouble *d = (const GLdouble *)p;

                v[i].v.u = (GLfloat)d[0];
                v[i].v.v = tx0sz > 1 ? (GLfloat)d[1] : 0.0f;
                v[i].v.tex3d = tx0sz > 2 ? (GLfloat)d[2] : 0.0f;

                if (tx0sz > 3)
                    v[i].q = 1.0f / (GLfloat)d[3];
            }

            break;

        case GL_INT:
            for (i = 0; i < count; i++, p += tx0st)
            {
                const GLint *l = (const GLint *)p;

                v[i].v.u = (GLfloat)l[0];
                v[i].v.v = tx0sz > 1 ? (GLfloat)l[1] : 0.0f;
                v[i].v.tex3d = tx0sz > 2 ? (GLfloat)l[2] : 0.0f;

                if (tx0sz > 3)
                    v[i].q = 1.0f / (GLfloat)l[3];
            }

            break;

        case GL_BYTE:
            for (i = 0; i < count; i++, p += tx0st)
            {
                const GLbyte *b = (const GLbyte *)p;

                v[i].v.u = (GLfloat)b[0];
                v[i].v.v = tx0sz > 1 ? (GLfloat)b[1] : 0.0f;
                v[i].v.tex3d = tx0sz > 2 ? (GLfloat)b[2] : 0.0f;

                if (tx0sz > 3)
                    v[i].q = 1.0f / (GLfloat)b[3];
            }

            break;

        default:
            for (i = 0; i < count; i++, p += tx0st)
            {
                const GLshort *s = (const GLshort *)p;

                v[i].v.u = (GLfloat)s[0];
                v[i].v.v = tx0sz > 1 ? (GLfloat)s[1] : 0.0f;
                v[i].v.tex3d = tx0sz > 2 ? (GLfloat)s[2] : 0.0f;

                if (tx0sz > 3)
                    v[i].q = 1.0f / (GLfloat)s[3];
            }

            break;
        }
    }
    else
    {
        const float tu = context->CurrentTexS;
        const float tv = context->CurrentTexT;
        const float tr = context->CurrentTexR;
        const float tq = context->CurrentTexQ;

        for (i = 0; i < count; i++)
        {
            v[i].v.u = tu;
            v[i].v.v = tv;
            v[i].v.tex3d = tr;
            v[i].q = tq;
        }
    }

    if (cs & GLCS_COLOR)
    {
        const GLuint cstride = ap->colorstride;
        const int colsz = context->ArrColorSize;

        p = ap->colors + first * cstride;

        switch (context->ArrColorType)
        {
        case GL_UNSIGNED_BYTE:
            for (i = 0; i < count; i++, p += cstride)
            {
                v[i].v.color.r = p[0] * (1.0f / 255.0f);
                v[i].v.color.g = p[1] * (1.0f / 255.0f);
                v[i].v.color.b = p[2] * (1.0f / 255.0f);
                v[i].v.color.a = colsz > 3 ? p[3] * (1.0f / 255.0f) : 1.0f;
            }

            break;

        case MGL_UBYTE_ARGB:
            for (i = 0; i < count; i++, p += cstride)
            {
                v[i].v.color.r = p[1] * (1.0f / 255.0f);
                v[i].v.color.g = p[2] * (1.0f / 255.0f);
                v[i].v.color.b = p[3] * (1.0f / 255.0f);
                v[i].v.color.a = colsz > 3 ? p[0] * (1.0f / 255.0f) : 1.0f;
            }

            break;

        case MGL_UBYTE_BGRA:
            for (i = 0; i < count; i++, p += cstride)
            {
                v[i].v.color.r = p[2] * (1.0f / 255.0f);
                v[i].v.color.g = p[1] * (1.0f / 255.0f);
                v[i].v.color.b = p[0] * (1.0f / 255.0f);
                v[i].v.color.a = colsz > 3 ? p[3] * (1.0f / 255.0f) : 1.0f;
            }

            break;

        case GL_UNSIGNED_SHORT:
            for (i = 0; i < count; i++, p += cstride)
            {
                const GLushort *s = (const GLushort *)p;

                v[i].v.color.r = s[0] * (1.0f / 65535.0f);
                v[i].v.color.g = s[1] * (1.0f / 65535.0f);
                v[i].v.color.b = s[2] * (1.0f / 65535.0f);
                v[i].v.color.a = colsz > 3 ? s[3] * (1.0f / 65535.0f) : 1.0f;
            }

            break;

        case GL_FLOAT:
            for (i = 0; i < count; i++, p += cstride)
            {
                const GLfloat *f = (const GLfloat *)p;

                v[i].v.color.r = f[0];
                v[i].v.color.g = f[1];
                v[i].v.color.b = f[2];
                v[i].v.color.a = colsz > 3 ? f[3] : 1.0f;
            }

            break;

        case GL_DOUBLE:
            for (i = 0; i < count; i++, p += cstride)
            {
                const GLdouble *d = (const GLdouble *)p;

                v[i].v.color.r = (GLfloat)d[0];
                v[i].v.color.g = (GLfloat)d[1];
                v[i].v.color.b = (GLfloat)d[2];
                v[i].v.color.a = colsz > 3 ? (GLfloat)d[3] : 1.0f;
            }

            break;

        case GL_UNSIGNED_INT:
            for (i = 0; i < count; i++, p += cstride)
            {
                const GLuint *u = (const GLuint *)p;

                v[i].v.color.r = u[0] * (1.0f / 4294967295.0f);
                v[i].v.color.g = u[1] * (1.0f / 4294967295.0f);
                v[i].v.color.b = u[2] * (1.0f / 4294967295.0f);
                v[i].v.color.a = colsz > 3 ? u[3] * (1.0f / 4294967295.0f) : 1.0f;
            }

            break;

        case GL_INT:
            for (i = 0; i < count; i++, p += cstride)
            {
                const GLint *l = (const GLint *)p;

                v[i].v.color.r = l[0] * (1.0f / 2147483647.0f);
                v[i].v.color.g = l[1] * (1.0f / 2147483647.0f);
                v[i].v.color.b = l[2] * (1.0f / 2147483647.0f);
                v[i].v.color.a = colsz > 3 ? l[3] * (1.0f / 2147483647.0f) : 1.0f;
            }

            break;

        case GL_SHORT:
            for (i = 0; i < count; i++, p += cstride)
            {
                const GLshort *s = (const GLshort *)p;

                v[i].v.color.r = s[0] * (1.0f / 32767.0f);
                v[i].v.color.g = s[1] * (1.0f / 32767.0f);
                v[i].v.color.b = s[2] * (1.0f / 32767.0f);
                v[i].v.color.a = colsz > 3 ? s[3] * (1.0f / 32767.0f) : 1.0f;
            }

            break;

        default:
            for (i = 0; i < count; i++, p += cstride)
            {
                const char *b = (const char *)p;

                v[i].v.color.r = b[0] < -127 ? -1.0f : b[0] * (1.0f / 127.0f);
                v[i].v.color.g = b[1] < -127 ? -1.0f : b[1] * (1.0f / 127.0f);
                v[i].v.color.b = b[2] < -127 ? -1.0f : b[2] * (1.0f / 127.0f);
                v[i].v.color.a = colsz > 3 ? (b[3] < -127 ? -1.0f : b[3] * (1.0f / 127.0f)) : 1.0f;
            }

            break;
        }
    }
    else
    {
        // flat fallback needs every vertex's color for the provoking test
        const W3D_Color ccur = context->CurrentColor;

        for (i = 0; i < count; i++)
            v[i].v.color = ccur;
    }

    if (cs & GLCS_NORMAL)
    {
        // array normals live in the upper domain, slot 0..size-1 rides immediate
        const GLuint nbase = context->VertexBufferSize + slot0;
        MGLNormal *nb = context->NormalBuffer + nbase;

        const GLuint nstride = context->ArrNormalStride;

        p = context->ArrNormal + first * nstride;

        switch (context->ArrNormalType)
        {
        case GL_FLOAT:
            for (i = 0; i < count; i++, p += nstride)
            {
                const GLfloat *f = (const GLfloat *)p;

                nb[i].x = f[0];
                nb[i].y = f[1];
                nb[i].z = f[2];
            }

            break;

        case GL_DOUBLE:
            for (i = 0; i < count; i++, p += nstride)
            {
                const GLdouble *d = (const GLdouble *)p;

                nb[i].x = (GLfloat)d[0];
                nb[i].y = (GLfloat)d[1];
                nb[i].z = (GLfloat)d[2];
            }

            break;

        case GL_INT:
            for (i = 0; i < count; i++, p += nstride)
            {
                const GLint *l = (const GLint *)p;

                nb[i].x = l[0] * (1.0f / 2147483647.0f);
                nb[i].y = l[1] * (1.0f / 2147483647.0f);
                nb[i].z = l[2] * (1.0f / 2147483647.0f);
            }

            break;

        case GL_SHORT:
            for (i = 0; i < count; i++, p += nstride)
            {
                const GLshort *s = (const GLshort *)p;

                nb[i].x = s[0] * (1.0f / 32767.0f);
                nb[i].y = s[1] * (1.0f / 32767.0f);
                nb[i].z = s[2] * (1.0f / 32767.0f);
            }

            break;

        default:
            for (i = 0; i < count; i++, p += nstride)
            {
                const char *b = (const char *)p;

                nb[i].x = b[0] < -127 ? -1.0f : b[0] * (1.0f / 127.0f);
                nb[i].y = b[1] < -127 ? -1.0f : b[1] * (1.0f / 127.0f);
                nb[i].z = b[2] < -127 ? -1.0f : b[2] * (1.0f / 127.0f);
            }

            break;
        }

        for (i = 0; i < count; i++)
            v[i].normal = nbase + i;
    }
    else
    {
        // verts without a normal array share one slot, keep it current
        const GLuint nbp = context->NormalBufferPointer;

        context->NormalBuffer[nbp] = context->CurrentNormal;

        for (i = 0; i < count; i++)
            v[i].normal = nbp;
    }

    context->WOne_Hint = wone;
}

// fetch one client element into its staged slot
void va_Stage(GLcontext context, GLuint index, GLuint slot)
{
    va_StageRange(context, index, 1, slot);
}

void GLEdgeFlag(GLcontext context, GLboolean flag)
{
    context->EdgeFlagCur = flag;
}

void GLEdgeFlagv(GLcontext context, const GLboolean *flag)
{
    context->EdgeFlagCur = *flag;
}

void GLEdgeFlagPointer(GLcontext context, GLsizei stride, const GLvoid *pointer)
{
    context->EdgeFlagStride = stride ? stride : (GLsizei)sizeof(GLboolean);
    context->EdgeFlagPtr = pointer;
}

void GLDrawArrays(GLcontext context, GLenum mode, GLint first, GLsizei count)
{
    const MGLAPointer *ap = &context->ArrayPointer;
    const GLbitfield cs = context->ClientState;
    GLboolean qsave;
    GLuint ptr;
    GLuint i;

    if (mode > GL_POLYGON)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (first < 0 || count < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (context->CurrentPrimitive != GL_BASE)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (!count || !(cs & GLCS_VERTEX) || !ap->verts || ((cs & GLCS_TEXTURE) && !ap->ctex[0].ptr) || ((cs & GLCS_COLOR) && !ap->colors) || ((cs & GLCS_NORMAL) && !context->ArrNormal))
        return;

    if (ef_On(context, mode) && (GLuint)first + (GLuint)count <= 0x10000)
    {
        ef_Run(context, mode, count, 0, NULL, (GLuint)first);
        return;
    }

    // every enabled extra texcoord array must point at valid data
    for (i = 1; i < context->TexUnits; i++)
    {
        if ((context->TexCSMask & (1u << i)) && !ap->ctex[i].ptr)
            return;
    }

    if (!context->VertexArrayPipeline)
    {
        dx_draw(context, mode, count, 0, NULL, (GLuint)first);
        return;
    }

    // a draw never mutates current state, the flag covers only this batch
    qsave = context->CurrentTexQValid;

    if (cs & GLCS_TEXTURE)
        context->CurrentTexQValid = ap->ctex[0].size == 4;

    context->CurrentPrimitive = mode;
    context->VertexBufferPointer = 0;
    context->EmitBase = 0;
    context->StripBase = 0;
    context->NormalBufferPointer = 0;
    context->LockStaged = GL_FALSE;

    ptr = 0;

    for (i = 0; i < (GLuint)count;)
    {
        GLuint run;

        if (ptr == context->VertexBufferSize)
        {
            context->VertexBufferPointer = ptr;

            d_Submit(context, NULL, 0, GL_FALSE);

            ptr = context->VertexBufferPointer;
        }

        run = (GLuint)count - i;

        if (run > context->VertexBufferSize - ptr)
            run = context->VertexBufferSize - ptr;

        va_StageRange(context, (GLuint)first + i, run, ptr);

        i += run;
        ptr += run;
    }

    context->VertexBufferPointer = ptr;

    d_Submit(context, NULL, 0, GL_TRUE);

    context->CurrentPrimitive = GL_BASE;
    context->CurrentTexQValid = qsave;
}

void GLMultiDrawArrays(GLcontext context, GLenum mode, const GLint *first, const GLsizei *count, GLsizei primcount)
{
    GLsizei i;

    if (primcount < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    for (i = 0; i < primcount; i++)
        GLDrawArrays(context, mode, first[i], count[i]);
}

// selects which client's texcoord state the array calls configure
void GLClientActiveTextureARB(GLcontext context, GLenum unit)
{
    const int u = unit - GL_TEXTURE0_ARB;

    if (u < 0 || u >= (int)context->TexUnits)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->ClientActiveTexture = (GLuint)u;
}

// formats carry their own field offsets, stride defaults to packed
void GLInterleavedArrays(GLcontext context, GLenum format, GLsizei stride, const GLvoid *pointer)
{
    const GLubyte *p = (const GLubyte *)pointer;

    if (stride < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    switch (format)
    {
    case GL_V2F:
        GLVertexPointer(context, 2, GL_FLOAT, stride ? stride : 8, p);

        context->ClientState = GLCS_VERTEX;
        break;

    case GL_V3F:
        GLVertexPointer(context, 3, GL_FLOAT, stride ? stride : 12, p);

        context->ClientState = GLCS_VERTEX;
        break;

    case GL_C4UB_V2F:
        GLColorPointer(context, 4, GL_UNSIGNED_BYTE, stride ? stride : 12, p);
        GLVertexPointer(context, 2, GL_FLOAT, stride ? stride : 12, p + 4);

        context->ClientState = GLCS_COLOR | GLCS_VERTEX;
        break;

    case GL_C4UB_V3F:
        GLColorPointer(context, 4, GL_UNSIGNED_BYTE, stride ? stride : 16, p);
        GLVertexPointer(context, 3, GL_FLOAT, stride ? stride : 16, p + 4);

        context->ClientState = GLCS_COLOR | GLCS_VERTEX;
        break;

    case GL_C3F_V3F:
        GLColorPointer(context, 3, GL_FLOAT, stride ? stride : 24, p);
        GLVertexPointer(context, 3, GL_FLOAT, stride ? stride : 24, p + 12);

        context->ClientState = GLCS_COLOR | GLCS_VERTEX;
        break;

    case GL_N3F_V3F:
        GLNormalPointer(context, GL_FLOAT, stride ? stride : 24, p);
        GLVertexPointer(context, 3, GL_FLOAT, stride ? stride : 24, p + 12);

        context->ClientState = GLCS_NORMAL | GLCS_VERTEX;
        break;

    case GL_C4F_N3F_V3F:
        GLColorPointer(context, 4, GL_FLOAT, stride ? stride : 40, p);
        GLNormalPointer(context, GL_FLOAT, stride ? stride : 40, p + 16);
        GLVertexPointer(context, 3, GL_FLOAT, stride ? stride : 40, p + 28);

        context->ClientState = GLCS_COLOR | GLCS_NORMAL | GLCS_VERTEX;
        break;

    case GL_T2F_V3F:
        GLTexCoordPointer(context, 2, GL_FLOAT, stride ? stride : 20, p);
        GLVertexPointer(context, 3, GL_FLOAT, stride ? stride : 20, p + 8);

        context->ClientState = GLCS_TEXTURE | GLCS_VERTEX;
        break;

    case GL_T4F_V4F:
        GLTexCoordPointer(context, 4, GL_FLOAT, stride ? stride : 32, p);
        GLVertexPointer(context, 4, GL_FLOAT, stride ? stride : 32, p + 16);

        context->ClientState = GLCS_TEXTURE | GLCS_VERTEX;
        break;

    case GL_T2F_C3F_V3F:
        GLTexCoordPointer(context, 2, GL_FLOAT, stride ? stride : 32, p);
        GLColorPointer(context, 3, GL_FLOAT, stride ? stride : 32, p + 8);
        GLVertexPointer(context, 3, GL_FLOAT, stride ? stride : 32, p + 20);

        context->ClientState = GLCS_TEXTURE | GLCS_COLOR | GLCS_VERTEX;
        break;

    case GL_T2F_C4F_V3F:
        GLTexCoordPointer(context, 2, GL_FLOAT, stride ? stride : 36, p);
        GLColorPointer(context, 4, GL_FLOAT, stride ? stride : 36, p + 8);
        GLVertexPointer(context, 3, GL_FLOAT, stride ? stride : 36, p + 24);

        context->ClientState = GLCS_TEXTURE | GLCS_COLOR | GLCS_VERTEX;
        break;

    case GL_T2F_N3F_V3F:
        GLTexCoordPointer(context, 2, GL_FLOAT, stride ? stride : 32, p);
        GLNormalPointer(context, GL_FLOAT, stride ? stride : 32, p + 8);
        GLVertexPointer(context, 3, GL_FLOAT, stride ? stride : 32, p + 20);

        context->ClientState = GLCS_TEXTURE | GLCS_NORMAL | GLCS_VERTEX;
        break;

    case GL_T2F_C4F_N3F_V3F:
        GLTexCoordPointer(context, 2, GL_FLOAT, stride ? stride : 48, p);
        GLColorPointer(context, 4, GL_FLOAT, stride ? stride : 48, p + 8);
        GLNormalPointer(context, GL_FLOAT, stride ? stride : 48, p + 24);
        GLVertexPointer(context, 3, GL_FLOAT, stride ? stride : 48, p + 36);

        context->ClientState = GLCS_TEXTURE | GLCS_COLOR | GLCS_NORMAL | GLCS_VERTEX;
        break;

    case GL_T4F_C4F_N3F_V4F:
        GLTexCoordPointer(context, 4, GL_FLOAT, stride ? stride : 60, p);
        GLColorPointer(context, 4, GL_FLOAT, stride ? stride : 60, p + 16);
        GLNormalPointer(context, GL_FLOAT, stride ? stride : 60, p + 32);
        GLVertexPointer(context, 4, GL_FLOAT, stride ? stride : 60, p + 44);

        context->ClientState = GLCS_TEXTURE | GLCS_COLOR | GLCS_NORMAL | GLCS_VERTEX;
        break;

    case GL_T2F_C4UB_V3F:
        GLTexCoordPointer(context, 2, GL_FLOAT, stride ? stride : 24, p);
        GLColorPointer(context, 4, GL_UNSIGNED_BYTE, stride ? stride : 24, p + 8);
        GLVertexPointer(context, 3, GL_FLOAT, stride ? stride : 24, p + 12);

        context->ClientState = GLCS_TEXTURE | GLCS_COLOR | GLCS_VERTEX;
        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
    }

    if (context->ClientState & GLCS_TEXTURE)
    {
        context->TexCSMask |= 1u << context->ClientActiveTexture;

        if (context->ClientActiveTexture)
            context->ClientState &= ~GLCS_TEXTURE;
    }
    else
        context->TexCSMask &= ~(1u << context->ClientActiveTexture);

    if (!context->VertexArrayPipeline)
        va_bindptr(context);
}

static void vao_save(GLcontext context, MGLVAO *v)
{
    v->ptr = context->ArrayPointer;
    v->clientstate = context->ClientState;
    v->texcs = context->TexCSMask;
    v->arrvertsize = context->ArrVertSize;
    v->arrcolorsize = context->ArrColorSize;
    v->arrverttype = context->ArrVertType;
    v->arrcolortype = context->ArrColorType;
    v->arrnormal = context->ArrNormal;
    v->arrnormalstride = context->ArrNormalStride;
    v->arrnormaltype = context->ArrNormalType;
    v->tcopymax = context->TCopyMax;
    v->arrayslocked = context->ArraysLocked;
    v->lockfirst = context->LockFirst;
    v->lockcount = context->LockCount;
    v->arredgeflag = context->EdgeFlagPtr;
    v->arredgestride = context->EdgeFlagStride;
}

static void vao_load(GLcontext context, const MGLVAO *v)
{
    context->ArrayPointer = v->ptr;
    context->ClientState = v->clientstate;
    context->TexCSMask = v->texcs;
    context->ArrVertSize = v->arrvertsize;
    context->ArrColorSize = v->arrcolorsize;
    context->ArrVertType = v->arrverttype;
    context->ArrColorType = v->arrcolortype;
    context->ArrNormal = v->arrnormal;
    context->ArrNormalStride = v->arrnormalstride;
    context->ArrNormalType = v->arrnormaltype;
    context->TCopyMax = v->tcopymax;
    context->ArraysLocked = v->arrayslocked;
    context->LockFirst = v->lockfirst;
    context->LockCount = v->lockcount;
    context->EdgeFlagPtr = v->arredgeflag;
    context->EdgeFlagStride = v->arredgestride;

    context->LockStaged = GL_FALSE;
}

void GLGenVertexArrays(GLcontext context, GLsizei n, GLuint *ids)
{
    GLuint i;
    GLsizei got = 0;

    if (n < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    for (i = 1; i <= MGL_VAO_MAX && got < n; i++)
    {
        if (!context->VAOs[i].name)
        {
            context->VAOs[i].name = i;
            ids[got++] = i;
        }
    }

    if (got < n)
        mgl_error(context, GL_OUT_OF_MEMORY);
}

GLboolean GLIsVertexArray(GLcontext context, GLuint id)
{
    return id && id <= MGL_VAO_MAX && context->VAOs[id].name ? GL_TRUE : GL_FALSE;
}

void GLBindVertexArray(GLcontext context, GLuint id)
{
    if (id > MGL_VAO_MAX || (id && !context->VAOs[id].name))
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (id == context->VAOBound)
        return;

    vao_save(context, &context->VAOs[context->VAOBound]);
    vao_load(context, &context->VAOs[id]);

    context->VAOBound = id;

    va_bindptr(context);
}

void GLDeleteVertexArrays(GLcontext context, GLsizei n, const GLuint *ids)
{
    GLsizei i;

    if (n < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    for (i = 0; i < n; i++)
    {
        GLuint id = ids[i];

        if (id && id <= MGL_VAO_MAX && context->VAOs[id].name)
        {
            if (id == context->VAOBound)
                GLBindVertexArray(context, 0);

            memset(&context->VAOs[id], 0, sizeof(MGLVAO));
        }
    }
}

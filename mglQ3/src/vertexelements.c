#include "mglint.h"

// elements per primitive for packed modes, walk modes emit per index
static const GLubyte ve_align[] = {1, 2, 1, 1, 3, 1, 1, 4, 2, 1};

// indices re-emitted from the previous slice to keep prim continuity
static const GLubyte ve_seeds[] = {0, 0, 2, 1, 0, 2, 2, 0, 2, 2};

static const UBYTE dx_prim[10] =
    {
        W3D_PRIMITIVE_POINTS,    // GL_POINTS
        W3D_PRIMITIVE_LINES,     // GL_LINES
        W3D_PRIMITIVE_LINELOOP,  // GL_LINE_LOOP
        W3D_PRIMITIVE_LINESTRIP, // GL_LINE_STRIP
        W3D_PRIMITIVE_TRIANGLES, // GL_TRIANGLES
        W3D_PRIMITIVE_TRISTRIP,  // GL_TRIANGLE_STRIP
        W3D_PRIMITIVE_TRIFAN,    // GL_TRIANGLE_FAN
        0xFF,                    // GL_QUADS -> tris
        0xFF,                    // GL_QUAD_STRIP -> tris
        W3D_PRIMITIVE_TRIFAN     // GL_POLYGON
};

// the locked span is staged once and reused until unlocked
void GLLockArrays(GLcontext context, GLuint first, GLsizei count)
{
    if (!context->VertexArrayPipeline)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (context->CurrentPrimitive != GL_BASE)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if ((GLint)first < 0 || count < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    context->ArraysLocked = GL_TRUE;
    context->LockStaged = GL_FALSE;
    context->LockXformed = GL_FALSE;
    context->LockFirst = first;
    context->LockCount = (GLuint)count;
}

void GLUnlockArrays(GLcontext context)
{
    context->ArraysLocked = GL_FALSE;
    context->LockStaged = GL_FALSE;
    context->LockXformed = GL_FALSE;
}

static GLuint ve_at(GLenum type, const GLvoid *indices, GLuint i)
{
    if (type == GL_UNSIGNED_BYTE)
        return ((const GLubyte *)indices)[i];

    if (type == GL_UNSIGNED_SHORT)
        return ((const UWORD *)indices)[i];

    return ((const ULONG *)indices)[i];
}

static ULONG dx_itype(GLenum type)
{
    if (type == GL_UNSIGNED_BYTE)
        return W3D_INDEX_UBYTE;

    if (type == GL_UNSIGNED_SHORT)
        return W3D_INDEX_UWORD;

    return W3D_INDEX_ULONG;
}

static GLboolean dx_quads(GLcontext context, GLenum mode, GLenum type, const GLvoid *indices, GLuint count, GLuint base)
{
    UWORD *ib = context->IdxBuf;
    const GLuint cap = MGL_IDXBUF_MULT * (context->VertexBufferSize + MGL_CLIP_SLACK);
    const GLuint room = cap / 6;
    const GLuint istep = mode == GL_QUADS ? 4 : 2;
    const GLuint oc = mode == GL_QUADS ? 2 : 3;
    const GLuint od = mode == GL_QUADS ? 3 : 2;
    const GLuint nq = mode == GL_QUADS ? (GLuint)count / 4 : (GLuint)count >= 4 ? ((GLuint)count - 2) / 2
                                                                                : 0;
    GLuint k = 0;

    while (k < nq)
    {
        GLuint q, e = 0;
        const GLuint lim = nq - k < room ? nq - k : room;

        if (indices)
        {
            for (q = 0; q < lim; q++)
            {
                const GLuint s = (k + q) * istep;
                const GLuint a = ve_at(type, indices, s);
                const GLuint b = ve_at(type, indices, s + 1);
                const GLuint c = ve_at(type, indices, s + oc);
                const GLuint d = ve_at(type, indices, s + od);

                if ((a | b | c | d) > 0xFFFF)
                {
                    mgl_error(context, GL_INVALID_VALUE);
                    return GL_FALSE;
                }

                ib[e] = (UWORD)a;
                ib[e + 1] = (UWORD)b;
                ib[e + 2] = (UWORD)c;
                ib[e + 3] = (UWORD)a;
                ib[e + 4] = (UWORD)c;
                ib[e + 5] = (UWORD)d;

                e += 6;
            }
        }
        else
        {
            for (q = 0; q < lim; q++)
            {
                const GLuint s = base + (k + q) * istep;
                const GLuint a = s;
                const GLuint b = s + 1;
                const GLuint c = s + oc;
                const GLuint d = s + od;

                ib[e] = (UWORD)a;
                ib[e + 1] = (UWORD)b;
                ib[e + 2] = (UWORD)c;
                ib[e + 3] = (UWORD)a;
                ib[e + 4] = (UWORD)c;
                ib[e + 5] = (UWORD)d;

                e += 6;
            }
        }

        W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_TRIANGLES, W3D_INDEX_UWORD, e, ib);

        k += lim;
    }

    return GL_TRUE;
}

static void dx_seq(GLcontext context, ULONG prim, GLuint count, GLuint base)
{
    const GLuint icap = context->VertexBufferSize + MGL_CLIP_SLACK;
    const GLuint cap = MGL_IDXBUF_MULT * icap;
    UWORD *ib = context->IdxBuf;
    GLuint pos = 0;

    if (base + count <= icap)
    {
        W3D_DrawElements(context->w3dContext, prim, W3D_INDEX_UWORD, count, (void *)(context->IdentIdx + base));

        return;
    }

    if (prim == W3D_PRIMITIVE_TRIFAN)
    {
        while (pos < count)
        {
            GLuint run = count - pos;
            GLuint e = 0;

            if (pos)
            {
                ib[e++] = (UWORD)base;

                if (run + 1 > cap)
                    run = cap - 1;

                for (; e <= run; e++)
                    ib[e] = (UWORD)(base + pos + e - 1);
            }
            else
            {
                if (run > cap)
                    run = cap;

                for (; e < run; e++)
                    ib[e] = (UWORD)(base + e);
            }

            W3D_DrawElements(context->w3dContext, prim, W3D_INDEX_UWORD, run + (pos ? 1 : 0), ib);

            pos += run;
        }

        return;
    }

    const GLuint lap = prim == W3D_PRIMITIVE_TRISTRIP ? 2 : prim == W3D_PRIMITIVE_LINESTRIP || prim == W3D_PRIMITIVE_LINELOOP ? 1
                                                                                                                              : 0;
    const ULONG wprim = prim == W3D_PRIMITIVE_LINELOOP ? W3D_PRIMITIVE_LINESTRIP : prim;

    while (pos < count)
    {
        GLuint run = count - pos;
        GLuint e;

        if (run > cap)
            run = cap;

        for (e = 0; e < run; e++)
            ib[e] = (UWORD)(base + pos + e);

        W3D_DrawElements(context->w3dContext, wprim, W3D_INDEX_UWORD, run, ib);

        if (run >= count - pos)
            break;

        pos += run - lap;
    }

    if (prim == W3D_PRIMITIVE_LINELOOP && count > 1)
    {
        ib[0] = (UWORD)(base + count - 1);
        ib[1] = (UWORD)base;

        W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_LINES, W3D_INDEX_UWORD, 2, ib);
    }
}

static GLboolean dx_ok(GLcontext context)
{
    const GLbitfield cs = context->ClientState;

    if (context->ArrVertType != GL_FLOAT || context->ArrVertSize < 3)
        return GL_FALSE;

    if ((cs & GLCS_TEXTURE) && (context->ArrayPointer.ctex[0].type != GL_FLOAT || context->ArrayPointer.ctex[0].size < 3))
        return GL_FALSE;

    if ((cs & GLCS_COLOR) && !va_colormode(context))
        return GL_FALSE;

    return GL_TRUE;
}

void dx_draw(GLcontext context, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLuint base)
{
    int lk;

    if (context->CullFace_State && context->CurrentCullFace == GL_FRONT_AND_BACK)
        return;

    if (!dx_ok(context))
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (!indices && base + (GLuint)count > 0x10000)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (!context->ArrayClientBound)
        va_bindptr(context);

    lk = dx_lock(context);

    if (lk < 0)
        return;

    if (dx_prim[mode] != 0xFF)
    {
        if (indices)
            W3D_DrawElements(context->w3dContext, dx_prim[mode], dx_itype(type), (ULONG)count, (void *)indices);
        else
            dx_seq(context, dx_prim[mode], (GLuint)count, base);
    }
    else
        dx_quads(context, mode, type, indices, (GLuint)count, base);

    dx_unlock(context, lk);
}

GLboolean ef_On(GLcontext context, GLenum mode)
{
    return context->CurPolygonMode == GL_LINE && (context->ClientState & GLCS_EDGEFLAG) && context->EdgeFlagPtr && mode >= GL_TRIANGLES;
}

static void ef_edge(GLcontext context, UWORD *eb, GLuint *epos, GLuint u, GLuint v, GLuint cap, const UBYTE *fp0, GLsizei fstride)
{
    if (fp0 && !*((const GLboolean *)(fp0 + (size_t)u * (size_t)fstride)))
        return;

    if (*epos > cap - 2)
    {
        GLDrawElements(context, GL_LINES, (GLsizei)*epos, GL_UNSIGNED_SHORT, eb);
        *epos = 0;
    }

    eb[(*epos)++] = (UWORD)u;
    eb[(*epos)++] = (UWORD)v;
}

void ef_Run(GLcontext context, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLuint vbase)
{
    UWORD *eb = context->EdgeIdx;
    const GLuint efcap = 2 * (context->VertexBufferSize + MGL_CLIP_SLACK);
    const UBYTE *efp0 = (const UBYTE *)context->EdgeFlagPtr;
    const GLsizei efstride = context->EdgeFlagStride;
    GLuint epos = 0;
    GLuint v0;
    GLuint v1;
    GLuint v2;
    GLuint v3;
    GLuint i;

    if (type == GL_UNSIGNED_INT && indices)
    {
        const ULONG *ul = (const ULONG *)indices;
        GLuint imax = 0;

        for (i = 0; i < (GLuint)count; i++)
        {
            if (ul[i] > imax)
                imax = ul[i];
        }

        if (imax > 0xFFFF)
        {
            mgl_error(context, GL_INVALID_VALUE);
            return;
        }
    }

#define EF_I(j) (indices ? ve_at(type, indices, j) : vbase + (j))

    switch (mode)
    {
    case GL_TRIANGLES:
        for (i = 0; i + 2 < (GLuint)count; i += 3)
        {
            v0 = EF_I(i);
            v1 = EF_I(i + 1);
            v2 = EF_I(i + 2);

            ef_edge(context, eb, &epos, v0, v1, efcap, efp0, efstride);
            ef_edge(context, eb, &epos, v1, v2, efcap, efp0, efstride);
            ef_edge(context, eb, &epos, v2, v0, efcap, efp0, efstride);
        }

        break;

    case GL_TRIANGLE_STRIP:
        for (i = 0; i + 2 < (GLuint)count; i++)
        {
            v0 = EF_I(i);
            v1 = EF_I(i + 1);
            v2 = EF_I(i + 2);

            ef_edge(context, eb, &epos, v0, v1, efcap, NULL, 0);
            ef_edge(context, eb, &epos, v1, v2, efcap, NULL, 0);
            ef_edge(context, eb, &epos, v2, v0, efcap, NULL, 0);
        }

        break;

    case GL_TRIANGLE_FAN:
        for (i = 1; i + 1 < (GLuint)count; i++)
        {
            v0 = EF_I(0);
            v1 = EF_I(i);
            v2 = EF_I(i + 1);

            ef_edge(context, eb, &epos, v0, v1, efcap, NULL, 0);
            ef_edge(context, eb, &epos, v1, v2, efcap, NULL, 0);
            ef_edge(context, eb, &epos, v2, v0, efcap, NULL, 0);
        }

        break;

    case GL_QUADS:
        for (i = 0; i + 3 < (GLuint)count; i += 4)
        {
            v0 = EF_I(i);
            v1 = EF_I(i + 1);
            v2 = EF_I(i + 2);
            v3 = EF_I(i + 3);

            ef_edge(context, eb, &epos, v0, v1, efcap, efp0, efstride);
            ef_edge(context, eb, &epos, v1, v2, efcap, efp0, efstride);
            ef_edge(context, eb, &epos, v2, v3, efcap, efp0, efstride);
            ef_edge(context, eb, &epos, v3, v0, efcap, efp0, efstride);
        }

        break;

    case GL_QUAD_STRIP:
        for (i = 0; i + 3 < (GLuint)count; i += 2)
        {
            v0 = EF_I(i);
            v1 = EF_I(i + 1);
            v2 = EF_I(i + 2);
            v3 = EF_I(i + 3);

            ef_edge(context, eb, &epos, v0, v1, efcap, NULL, 0);
            ef_edge(context, eb, &epos, v1, v3, efcap, NULL, 0);
            ef_edge(context, eb, &epos, v3, v2, efcap, NULL, 0);
            ef_edge(context, eb, &epos, v2, v0, efcap, NULL, 0);
        }

        break;

    default: /* GL_POLYGON */
        for (i = 1; i < (GLuint)count; i++)
            ef_edge(context, eb, &epos, EF_I(i - 1), EF_I(i), efcap, efp0, efstride);

        if (count > 1)
            ef_edge(context, eb, &epos, EF_I(count - 1), EF_I(0), efcap, efp0, efstride);

        break;
    }

    if (epos)
        GLDrawElements(context, GL_LINES, (GLsizei)epos, GL_UNSIGNED_SHORT, eb);

#undef EF_I
}

void GLDrawElements(GLcontext context, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    const MGLAPointer *ap = &context->ArrayPointer;
    const GLbitfield cs = context->ClientState;
    const GLuint cap = MGL_IDXBUF_MULT * (context->VertexBufferSize + MGL_CLIP_SLACK);
    const GLuint wsize = context->VertexBufferSize - MGL_EXTRA_MAX;
    UWORD *ib = context->IdxBuf;
    GLboolean qsave;
    GLboolean locked;
    GLuint pos;
    GLubyte align;
    GLubyte nseed;
    GLboolean fan;

    if (mode > GL_POLYGON)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (count < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (context->CurrentPrimitive != GL_BASE)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT && type != GL_UNSIGNED_INT)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (!count || !indices)
        return;

    if (ef_On(context, mode))
    {
        ef_Run(context, mode, count, type, indices, 0);
        return;
    }

    if (!(cs & GLCS_VERTEX) || !ap->verts || ((cs & GLCS_TEXTURE) && !ap->ctex[0].ptr) || ((cs & GLCS_COLOR) && !ap->colors) || ((cs & GLCS_NORMAL) && !context->ArrNormal))
        return;

    GLuint i;

    for (i = 1; i < context->TexUnits; i++)
    {
        if ((context->TexCSMask & (1u << i)) && !ap->ctex[i].ptr)
            return;
    }

    if (!context->VertexArrayPipeline)
    {
        dx_draw(context, mode, count, type, indices, 0);
        return;
    }

    // uword slots carry indices, values that cannot fit are rejected once per draw
    if (type == GL_UNSIGNED_INT)
    {
        const ULONG *ul = (const ULONG *)indices;
        GLuint i;
        GLuint imax = 0;

        for (i = 0; i < (GLuint)count; i++)
        {
            if (ul[i] > imax)
                imax = ul[i];
        }

        if (imax > 0xFFFF)
        {
            mgl_error(context, GL_INVALID_VALUE);
            return;
        }
    }

    // a draw never mutates current state, the flag covers only this batch
    qsave = context->CurrentTexQValid;

    if (cs & GLCS_TEXTURE)
        context->CurrentTexQValid = context->ArrayPointer.ctex[0].size == 4;

    align = ve_align[mode];
    nseed = ve_seeds[mode];
    fan = (mode == GL_TRIANGLE_FAN || mode == GL_POLYGON || mode == GL_LINE_LOOP);
    locked = (context->ArraysLocked && context->LockCount <= wsize);

    context->CurrentPrimitive = mode;
    context->VertexBufferPointer = 0;
    context->EmitBase = 0;
    context->StripBase = 0;
    context->NormalBufferPointer = 0;

    if (!locked)
    {
        context->LockStaged = GL_FALSE;
        context->LockXformed = GL_FALSE;
    }

    pos = 0;

    // index window is staged contiguous, seeds and straddlers go to the 4 extra slots
    while (pos < (GLuint)count)
    {
        GLuint ex[MGL_EXTRA_MAX];

        if (locked)
        {
            if (!context->LockXformed || context->Lighting_State || context->TextureGenS_State || context->TextureGenT_State)
            {
                va_StageRange(context, context->LockFirst, context->LockCount, MGL_EXTRA_MAX);

                context->LockStaged = GL_TRUE;
                context->LockXformed = GL_FALSE;
            }
        }

        const GLuint vp = locked ? context->LockFirst : ve_at(type, indices, pos);
        const GLuint w0 = locked ? vp : (vp > (wsize >> 1) ? vp - (wsize >> 1) : 0);
        const GLuint wspan = locked ? context->LockCount : wsize;
        const GLuint seeds = pos ? nseed : 0;
        const GLuint slab = (GLuint)count - pos < cap - seeds ? (GLuint)count - pos : cap - seeds;
        GLuint w1 = w0;
        GLuint exn = seeds;
        GLuint len = 0;
        GLuint k = seeds;
        GLuint e;
        int full = 0;

        if (pos)
        {
            if (fan)
            {
                ex[0] = ve_at(type, indices, 0);
                ex[1] = ve_at(type, indices, pos - 1);
            }
            else
            {
                for (e = 0; e < seeds; e++)
                    ex[e] = ve_at(type, indices, pos - seeds + e);
            }
        }

        for (e = 0; e < seeds; e++)
            ib[e] = (UWORD)e;

        // raw indices are converted once per slice, the scan remaps them in place
        switch (type)
        {
        case GL_UNSIGNED_BYTE:
            for (e = 0; e < slab; e++)
                ib[seeds + e] = ((const GLubyte *)indices)[pos + e];

            break;

        case GL_UNSIGNED_SHORT:
            for (e = 0; e < slab; e++)
                ib[seeds + e] = ((const UWORD *)indices)[pos + e];

            break;

        default:
            for (e = 0; e < slab; e++)
                ib[seeds + e] = (UWORD)((const ULONG *)indices)[pos + e];

            break;
        }

        if (locked)
        {
            // lock membership needs no window tracking
            while (pos + len < (GLuint)count && k + align <= cap)
            {
                for (e = 0; e < align; e++)
                {
                    GLuint v, s;

                    if (pos + len + e >= (GLuint)count)
                    {
                        full = 1;
                        break;
                    }

                    v = ib[k + e];

                    if (v - w0 < wspan)
                        s = MGL_EXTRA_MAX + (v - w0);
                    else if (exn < MGL_EXTRA_MAX)
                    {
                        s = exn;
                        ex[exn++] = v;
                    }
                    else
                    {
                        full = 1;
                        break;
                    }

                    ib[k + e] = (UWORD)s;
                }

                if (full)
                    break;

                len += align;
                k += align;
            }
        }
        else
        {
            while (pos + len < (GLuint)count && k + align <= cap)
            {
                for (e = 0; e < align; e++)
                {
                    GLuint v, s;

                    if (pos + len + e >= (GLuint)count)
                    {
                        full = 1;
                        break;
                    }

                    v = ib[k + e];

                    if (v - w0 < wspan)
                    {
                        s = MGL_EXTRA_MAX + (v - w0);

                        if (v > w1)
                            w1 = v;
                    }
                    else if (exn < MGL_EXTRA_MAX)
                    {
                        s = exn;
                        ex[exn++] = v;
                    }
                    else
                    {
                        full = 1;
                        break;
                    }

                    ib[k + e] = (UWORD)s;
                }

                if (full)
                    break;

                len += align;
                k += align;
            }
        }

        if (!len)
            break;

        if (!locked)
        {
            va_StageRange(context, w0, w1 - w0 + 1, MGL_EXTRA_MAX);

            context->VertexBufferPointer = MGL_EXTRA_MAX + (w1 - w0 + 1);
        }
        else
            context->VertexBufferPointer = MGL_EXTRA_MAX + context->LockCount;

        for (e = 0; e < exn; e++)
            va_Stage(context, ex[e], e);

        context->StripBase = (pos + seeds) & 1;
        context->EmitSkip = seeds;
        context->LockExtra = exn;

        d_Submit(context, ib, seeds + len, (pos + len == (GLuint)count));

        pos += len;
    }

    context->CurrentPrimitive = GL_BASE;
    context->CurrentTexQValid = qsave;
}

void GLDrawElementsBaseVertex(GLcontext context, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex)
{
    MGLAPointer *ap = &context->ArrayPointer;

    if (basevertex)
    {
        GLuint i;

        if (ap->verts)
            ap->verts += basevertex * ap->vertexstride;

        if (ap->colors)
            ap->colors += basevertex * ap->colorstride;

        for (i = 0; i < context->TexUnits; i++)
        {
            if (ap->ctex[i].ptr)
                ap->ctex[i].ptr += basevertex * ap->ctex[i].stride;
        }

        if (context->ArrNormal)
            context->ArrNormal += basevertex * context->ArrNormalStride;

        if (context->EdgeFlagPtr)
            context->EdgeFlagPtr = (const UBYTE *)context->EdgeFlagPtr + basevertex * context->EdgeFlagStride;

        if (!context->VertexArrayPipeline)
            va_bindptr(context);

        context->LockStaged = GL_FALSE;
        context->LockXformed = GL_FALSE;

        GLDrawElements(context, mode, count, type, indices);

        if (ap->verts)
            ap->verts -= basevertex * ap->vertexstride;

        if (ap->colors)
            ap->colors -= basevertex * ap->colorstride;

        for (i = 0; i < context->TexUnits; i++)
        {
            if (ap->ctex[i].ptr)
                ap->ctex[i].ptr -= basevertex * ap->ctex[i].stride;
        }

        if (context->ArrNormal)
            context->ArrNormal -= basevertex * context->ArrNormalStride;

        if (context->EdgeFlagPtr)
            context->EdgeFlagPtr = (const UBYTE *)context->EdgeFlagPtr - basevertex * context->EdgeFlagStride;

        if (!context->VertexArrayPipeline)
            va_bindptr(context);

        return;
    }

    GLDrawElements(context, mode, count, type, indices);
}

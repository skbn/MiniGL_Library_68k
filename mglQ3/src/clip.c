#include "mglint.h"

void cl_FrontOrder(GLcontext context)
{
    ULONG order = context->CurrentFrontFace == GL_CCW ? W3D_CW : W3D_CCW;

    if (context->ClipUpperLeft)
        order ^= 1;

    if (context->CullFace_State && context->CurrentCullFace == GL_FRONT)
        order ^= 1;

    W3D_SetFrontFace(context->w3dContext, order);
}

void GLFrontFace(GLcontext context, GLenum mode)
{
    switch (mode)
    {
    case GL_CW:
    case GL_CCW:
        if (context->CurrentFrontFace != mode)
        {
            context->CurrentFrontFace = mode;

            cl_FrontOrder(context);
        }

        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

void GLCullFace(GLcontext context, GLenum mode)
{
    switch (mode)
    {
    case GL_BACK:
    case GL_FRONT:
    case GL_FRONT_AND_BACK:
        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->CurrentCullFace = mode;

    // w3d cannot express front_and_back, those tris go through cpu cull
    if (context->CullFace_State)
        context->CullHW = mode != GL_FRONT_AND_BACK && W3D_SetState(context->w3dContext, W3D_CULLFACE, W3D_ENABLE) == W3D_SUCCESS;

    cl_FrontOrder(context);
}

// screen y runs downwards so a GL_CCW face reads as CW on the raster
static void cl_lerp(GLcontext context, const MGLVertex *a, const MGLVertex *b, MGLVertex *r, float t)
{
    const GLenum sm = context->ShadeModel;
    const GLuint tcm = context->TCopyMax;
    GLuint i;

    r->bx = a->bx + t * (b->bx - a->bx);
    r->by = a->by + t * (b->by - a->by);
    r->bz = a->bz + t * (b->bz - a->bz);
    r->bw = a->bw + t * (b->bw - a->bw);
    r->normal = a->normal;

    r->v.u = a->v.u + t * (b->v.u - a->v.u);
    r->v.v = a->v.v + t * (b->v.v - a->v.v);
    r->v.tex3d = a->v.tex3d + t * (b->v.tex3d - a->v.tex3d);

    if (sm == GL_SMOOTH)
    {
        r->v.color.r = a->v.color.r + t * (b->v.color.r - a->v.color.r);
        r->v.color.g = a->v.color.g + t * (b->v.color.g - a->v.color.g);
        r->v.color.b = a->v.color.b + t * (b->v.color.b - a->v.color.b);
        r->v.color.a = a->v.color.a + t * (b->v.color.a - a->v.color.a);
    }
    else
    {
        // flat keeps the inside vertex's color on produced verts
        r->v.color = a->v.color;
    }

    if (context->CurrentTexQValid)
        r->q = a->q + t * (b->q - a->q);

    for (i = 0; i < tcm; i++)
    {
        r->tcoord[i].s = a->tcoord[i].s + t * (b->tcoord[i].s - a->tcoord[i].s);
        r->tcoord[i].t = a->tcoord[i].t + t * (b->tcoord[i].t - a->tcoord[i].t);
    }
}

static ULONG cl_outcode(const MGLVertex *v)
{
    const float w = v->bw;
    ULONG code = 0;

    if (w < MGL_CLIP_EPS)
        code |= MGL_CLIP_NEGW;

    if (-w > v->bx)
        code |= MGL_CLIP_LEFT;
    else if (v->bx > w)
        code |= MGL_CLIP_RIGHT;

    if (-w > v->by)
        code |= MGL_CLIP_BOTTOM;
    else if (v->by > w)
        code |= MGL_CLIP_TOP;

    if (-w > v->bz)
        code |= MGL_CLIP_BACK;
    else if (v->bz > w)
        code |= MGL_CLIP_FRONT;

    return code;
}

// a is the inside vertex, b the outside one in all edge fns
static void cl_edge_negw(GLcontext context, const MGLVertex *a, const MGLVertex *b, MGLVertex *r)
{
    const float t = (MGL_CLIP_EPS - a->bw) / (b->bw - a->bw);

    cl_lerp(context, a, b, r, t);

    r->bw = MGL_CLIP_EPS;
    r->outcode = cl_outcode(r);
}

static void cl_edge_left(GLcontext context, const MGLVertex *a, const MGLVertex *b, MGLVertex *r)
{
    const float da = a->bx + a->bw;
    const float t = da / (da - (b->bx + b->bw));

    cl_lerp(context, a, b, r, t);

    r->bx = -r->bw;
    r->outcode = cl_outcode(r);
}

static void cl_edge_right(GLcontext context, const MGLVertex *a, const MGLVertex *b, MGLVertex *r)
{
    const float da = a->bw - a->bx;
    const float t = da / (da - (b->bw - b->bx));

    cl_lerp(context, a, b, r, t);

    r->bx = r->bw;
    r->outcode = cl_outcode(r);
}

static void cl_edge_bottom(GLcontext context, const MGLVertex *a, const MGLVertex *b, MGLVertex *r)
{
    const float da = a->by + a->bw;
    const float t = da / (da - (b->by + b->bw));

    cl_lerp(context, a, b, r, t);

    r->by = -r->bw;
    r->outcode = cl_outcode(r);
}

static void cl_edge_top(GLcontext context, const MGLVertex *a, const MGLVertex *b, MGLVertex *r)
{
    const float da = a->bw - a->by;
    const float t = da / (da - (b->bw - b->by));

    cl_lerp(context, a, b, r, t);

    r->by = r->bw;
    r->outcode = cl_outcode(r);
}

static void cl_edge_front(GLcontext context, const MGLVertex *a, const MGLVertex *b, MGLVertex *r)
{
    const float da = a->bw - a->bz;
    const float t = da / (da - (b->bw - b->bz));

    cl_lerp(context, a, b, r, t);

    r->bz = r->bw;
    r->outcode = cl_outcode(r);
}

static void cl_edge_back(GLcontext context, const MGLVertex *a, const MGLVertex *b, MGLVertex *r)
{
    const float da = a->bw + a->bz;
    const float t = da / (da - (b->bw + b->bz));

    cl_lerp(context, a, b, r, t);

    r->bz = -r->bw;
    r->outcode = cl_outcode(r);
}

static GLuint cl_pass(GLcontext context, const UWORD *in, GLuint n, UWORD *out, ULONG plane, GLuint *freep, void (*edge)(GLcontext, const MGLVertex *, const MGLVertex *, MGLVertex *))
{
    MGLVertex *vb = context->VertexBuffer;
    const GLuint cap = context->VertexBufferSize + MGL_CLIP_SLACK;
    GLuint prev = in[n - 1];
    const MGLVertex *pv = &vb[prev];
    ULONG pout = pv->outcode & plane;
    GLuint m = 0;
    GLuint i;

    for (i = 0; i < n; i++)
    {
        const GLuint cur = in[i];
        const MGLVertex *cv = &vb[cur];
        const ULONG cout = cv->outcode & plane;

        if (cout != pout)
        {
            MGLVertex *r;

            // concave input can cross many times, stop before the slack ends
            if (*freep >= cap || m >= cap)
                return 0;

            r = &vb[*freep];

            if (pout)
                edge(context, cv, pv, r);
            else
                edge(context, pv, cv, r);

            out[m++] = (UWORD)(*freep);
            (*freep)++;
        }

        if (!cout)
        {
            if (m >= cap)
                return 0;

            out[m++] = (UWORD)cur;
        }

        pout = cout;
        pv = cv;
    }

    return m;
}

GLuint cl_ClipIndex(GLcontext context, const UWORD *in, GLuint n, ULONG orc, UWORD **outp, GLuint *freep)
{
    const GLuint cap = context->VertexBufferSize + MGL_CLIP_SLACK;
    UWORD *la = context->ClipIdx;
    UWORD *lb = la + cap;
    const UWORD *cur = in;
    UWORD *dst = lb;
    GLuint free = context->VertexBufferSize;
    GLuint m = n;

    if (n > cap - 4)
    {
        *outp = NULL;
        *freep = free;
        return 0;
    }

    orc &= context->ClipFlags;

    if (orc & MGL_CLIP_NEGW)
    {
        m = cl_pass(context, cur, m, dst, MGL_CLIP_NEGW, &free, cl_edge_negw);
        cur = dst;
        dst = dst == lb ? la : lb;
        orc &= ~MGL_CLIP_NEGW;
    }

    if (m && (orc & MGL_CLIP_LEFT))
    {
        m = cl_pass(context, cur, m, dst, MGL_CLIP_LEFT, &free, cl_edge_left);
        cur = dst;
        dst = dst == lb ? la : lb;
        orc &= ~MGL_CLIP_LEFT;
    }

    if (m && (orc & MGL_CLIP_RIGHT))
    {
        m = cl_pass(context, cur, m, dst, MGL_CLIP_RIGHT, &free, cl_edge_right);
        cur = dst;
        dst = dst == lb ? la : lb;
        orc &= ~MGL_CLIP_RIGHT;
    }

    if (m && (orc & MGL_CLIP_TOP))
    {
        m = cl_pass(context, cur, m, dst, MGL_CLIP_TOP, &free, cl_edge_top);
        cur = dst;
        dst = dst == lb ? la : lb;
        orc &= ~MGL_CLIP_TOP;
    }

    if (m && (orc & MGL_CLIP_BOTTOM))
    {
        m = cl_pass(context, cur, m, dst, MGL_CLIP_BOTTOM, &free, cl_edge_bottom);
        cur = dst;
        dst = dst == lb ? la : lb;
        orc &= ~MGL_CLIP_BOTTOM;
    }

    if (m && (orc & MGL_CLIP_FRONT))
    {
        m = cl_pass(context, cur, m, dst, MGL_CLIP_FRONT, &free, cl_edge_front);
        cur = dst;
        dst = dst == lb ? la : lb;
        orc &= ~MGL_CLIP_FRONT;
    }

    if (m && (orc & MGL_CLIP_BACK))
    {
        m = cl_pass(context, cur, m, dst, MGL_CLIP_BACK, &free, cl_edge_back);
        cur = dst;
    }

    *outp = (UWORD *)cur;
    *freep = free;

    return m;
}

#include "mglint.h"
#include <math.h>

#define MGL_SMARTLOCK_SECS 0.05

static void d_emit_tri(GLcontext context, ULONG prim, UWORD *eb, GLuint *epos, GLuint a, GLuint b, GLuint d);
static int d_backface(GLcontext context, GLuint a, GLuint b, GLuint d);
static void d_emit_lloop(GLcontext context, const UWORD *in, GLuint n, GLuint orall, GLboolean final);
static void d_flatpt(GLcontext context, GLuint s);
static void d_flatseg(GLcontext context, GLuint a, GLuint b, int byv);

// varied staged colors force one driver color per prim under flat
static GLuint d_flatvar;
static GLuint d_flatvalid;
static W3D_Color d_flatlast;

// push one prim's provoking color, skip when the driver already has it
static void d_flatcol(GLcontext context, GLuint prov)
{
    W3D_Color *c = &context->VertexBuffer[prov].v.color;
    const ULONG *w = (const ULONG *)c;
    const ULONG *l = (const ULONG *)&d_flatlast;

    if (d_flatvalid && w[0] == l[0] && w[1] == l[1] && w[2] == l[2] && w[3] == l[3])
        return;

    d_flatlast = *c;
    d_flatvalid = 1;

    W3D_SetCurrentColor(context->w3dContext, c);
}

// transform dispatched once per submit on matrix flags, clip coords stay in bx..bw
static void d_xgen(MGLVertex *v, const float *m, GLuint n)
{
    GLuint i;

    for (i = 0; i < n; i++, v++)
    {
        const float x = v->bx;
        const float y = v->by;
        const float z = v->bz;
        const float w = v->bw;

        v->bx = m[0] * x + m[4] * y + m[8] * z + m[12] * w;
        v->by = m[1] * x + m[5] * y + m[9] * z + m[13] * w;
        v->bz = m[2] * x + m[6] * y + m[10] * z + m[14] * w;
        v->bw = m[3] * x + m[7] * y + m[11] * z + m[15] * w;
    }
}

static void d_xgenw(MGLVertex *v, const float *m, GLuint n)
{
    GLuint i;

    for (i = 0; i < n; i++, v++)
    {
        const float x = v->bx;
        const float y = v->by;
        const float z = v->bz;

        v->bx = m[0] * x + m[4] * y + m[8] * z + m[12];
        v->by = m[1] * x + m[5] * y + m[9] * z + m[13];
        v->bz = m[2] * x + m[6] * y + m[10] * z + m[14];
        v->bw = m[3] * x + m[7] * y + m[11] * z + m[15];
    }
}

static void d_xaff(MGLVertex *v, const float *m, GLuint n)
{
    GLuint i;

    for (i = 0; i < n; i++, v++)
    {
        const float x = v->bx;
        const float y = v->by;
        const float z = v->bz;

        v->bx = m[0] * x + m[4] * y + m[8] * z + m[12];
        v->by = m[1] * x + m[5] * y + m[9] * z + m[13];
        v->bz = m[2] * x + m[6] * y + m[10] * z + m[14];
    }
}

static void d_xdiag(MGLVertex *v, const float *m, GLuint n)
{
    GLuint i;

    for (i = 0; i < n; i++, v++)
    {
        v->bx = m[0] * v->bx + m[12];
        v->by = m[5] * v->by + m[13];
        v->bz = m[10] * v->bz + m[14];
    }
}

// outcode and window mapping in one pass, or of codes feeds the reject tests
static GLuint d_screenv(GLcontext context, GLuint from, GLuint n)
{
    MGLVertex *v = context->VertexBuffer + from;
    const float ax = context->ax;
    const float ay = context->ay;
    const float sx = context->sx;
    const float sy = context->ClipUpperLeft ? -context->sy : context->sy;
    const float az = context->az;
    const float sz = context->sz;
    const float zo = context->PolygonOffsetFill_State ? context->ZOffset : 0.0f;
    const int czo = context->ClipZO;
    const int tq = context->CurrentTexQValid;
    GLuint orc = 0;
    GLuint i;

    for (i = from; i < n; i++, v++)
    {
        const float w = v->bw;
        const float x = v->bx;
        const float y = v->by;
        float z = v->bz;
        ULONG code = 0;

        if (czo)
            v->bz = z = z + z - w;

        if (w < MGL_CLIP_EPS)
            code |= MGL_CLIP_NEGW;

        if (-w > x)
            code |= MGL_CLIP_LEFT;
        else if (x > w)
            code |= MGL_CLIP_RIGHT;

        if (-w > y)
            code |= MGL_CLIP_BOTTOM;
        else if (y > w)
            code |= MGL_CLIP_TOP;

        if (-w > z)
            code |= MGL_CLIP_BACK;
        else if (z > w)
            code |= MGL_CLIP_FRONT;

        v->outcode = code;
        orc |= code;

        if (!(code & MGL_CLIP_NEGW))
        {
            const float iw = 1.0f / w;

            v->v.x = ax + x * iw * sx;
            v->v.y = ay - y * iw * sy;
            v->v.z = (W3D_Double)(az + z * iw * sz + zo);
            v->v.w = tq ? v->q : iw;
        }
    }

    return orc;
}

// clip produced verts only need the window mapping, their outcode is done
static void d_vtos(GLcontext context, MGLVertex *v)
{
    const float iw = 1.0f / v->bw;

    v->v.x = context->ax + v->bx * iw * context->sx;
    v->v.y = context->ay - v->by * iw * (context->ClipUpperLeft ? -context->sy : context->sy);
    v->v.z = (W3D_Double)(context->az + v->bz * iw * context->sz + (context->PolygonOffsetFill_State ? context->ZOffset : 0.0f));
    v->v.w = context->CurrentTexQValid ? v->q : iw;
}

// h = normalize(l_hat + v), v is -p_e for a local viewer or (0,0,1) else
static inline void d_lhvec(float *hx, float *hy, float *hz, float lx, float ly, float lz, const MGLEye *e, GLuint lv)
{
    float sx = lx;
    float sy = ly;
    float sz = lz + 1.0f;

    if (lv)
    {
        const float iv = 1.0f / sqrtf(e->ex * e->ex + e->ey * e->ey + e->ez * e->ez + 1e-30f);

        sx = lx - e->ex * iv;
        sy = ly - e->ey * iv;
        sz = lz - e->ez * iv;
    }

    const float ih = 1.0f / sqrtf(sx * sx + sy * sy + sz * sz + 1e-30f);

    *hx = sx * ih;
    *hy = sy * ih;
    *hz = sz * ih;
}

// ambient plus diffuse accumulate, ndf carries att*n.l or zero
static inline void d_lacc(W3D_Color *c, const MGLLight *l, const MGLMaterial *m, float att, float ndf)
{
    c->r += att * l->ambient.r * m->ambient.r + ndf * l->diffuse.r * m->diffuse.r;
    c->g += att * l->ambient.g * m->ambient.g + ndf * l->diffuse.g * m->diffuse.g;
    c->b += att * l->ambient.b * m->ambient.b + ndf * l->diffuse.b * m->diffuse.b;
    c->a += att * l->ambient.a * m->ambient.a + ndf * l->diffuse.a * m->diffuse.a;
}

// specular term accumulate for one light
static inline void d_lspacc(W3D_Color *spc, const MGLLight *l, const MGLMaterial *m, float att, float sp)
{
    // shininess 0 is the default, powf(x, 0) is just att
    sp = att * (m->shininess != 0.0f ? powf(sp, m->shininess) : 1.0f);

    spc->r += sp * l->specular.r * m->specular.r;
    spc->g += sp * l->specular.g * m->specular.g;
    spc->b += sp * l->specular.b * m->specular.b;
}

// half vector plus spec accumulate, l is the per-vertex light direction
static inline void d_lspec(W3D_Color *spc, const MGLLight *l, const MGLMaterial *m, const MGLEye *e, float nx, float ny, float nz, float lx, float ly, float lz, float att, GLuint lv)
{
    float hx;
    float hy;
    float hz;
    float sp;

    d_lhvec(&hx, &hy, &hz, lx, ly, lz, e, lv);

    sp = nx * hx + ny * hy + nz * hz;

    if (sp > 0.0f)
        d_lspacc(spc, l, m, att, sp);
}

// accumulate one light per vertex, col is the lit color array with stride, spc takes the specular term so separate-specular can run its own pass
static void d_lit_dir(const MGLLight *l, const char *me, GLuint mstride, const MGLEye *ey, W3D_Color *col, GLuint cstride, W3D_Color *spc, GLuint n, int spec, int flip, GLuint lv)
{
    const float lx = l->pos[0];
    const float ly = l->pos[1];
    const float lz = l->pos[2];
    const float fs = flip ? -1.0f : 1.0f;
    float hx = 0.0f;
    float hy = 0.0f;
    float hz = 0.0f;
    GLuint i;

    // nonlocal viewer makes the half vector a per-light constant
    if (!lv)
        d_lhvec(&hx, &hy, &hz, lx, ly, lz, ey, 0);

    char *cp = (char *)col;

    for (i = 0; i < n; i++, ey++, me += mstride, cp += cstride, spc++)
    {
        const MGLMaterial *m = (const MGLMaterial *)me;
        const float nx = fs * ey->nx;
        const float ny = fs * ey->ny;
        const float nz = fs * ey->nz;
        const float nd = nx * lx + ny * ly + nz * lz;
        W3D_Color *c = (W3D_Color *)cp;

        d_lacc(c, l, m, 1.0f, nd > 0.0f ? nd : 0.0f);

        if (spec && nd > 0.0f)
        {
            float sp;

            if (lv)
                d_lhvec(&hx, &hy, &hz, lx, ly, lz, ey, 1);

            sp = nx * hx + ny * hy + nz * hz;

            if (sp > 0.0f)
                d_lspacc(spc, l, m, 1.0f, sp);
        }
    }
}

static void d_lit_pos(const MGLLight *l, const char *me, GLuint mstride, const MGLEye *ey, W3D_Color *col, GLuint cstride, W3D_Color *spc, GLuint n, int spec, int flip, GLuint lv)
{
    const float px = l->pos[0];
    const float py = l->pos[1];
    const float pz = l->pos[2];
    const float fs = flip ? -1.0f : 1.0f;
    const float att = l->att0;
    char *cp = (char *)col;
    GLuint i;

    for (i = 0; i < n; i++, ey++, me += mstride, cp += cstride, spc++)
    {
        const MGLMaterial *m = (const MGLMaterial *)me;
        const float nx = fs * ey->nx;
        const float ny = fs * ey->ny;
        const float nz = fs * ey->nz;
        float lx = px - ey->ex;
        float ly = py - ey->ey;
        float lz = pz - ey->ez;
        const float il = 1.0f / sqrtf(lx * lx + ly * ly + lz * lz + 1e-30f);
        const float nd = (nx * lx + ny * ly + nz * lz) * il;
        W3D_Color *c = (W3D_Color *)cp;

        lx *= il;
        ly *= il;
        lz *= il;

        d_lacc(c, l, m, att, nd > 0.0f ? att * nd : 0.0f);

        if (spec && nd > 0.0f)
            d_lspec(spc, l, m, ey, nx, ny, nz, lx, ly, lz, att, lv);
    }
}

static void d_lit_posa(const MGLLight *l, const char *me, GLuint mstride, const MGLEye *ey, W3D_Color *col, GLuint cstride, W3D_Color *spc, GLuint n, int spec, int flip, GLuint lv)
{
    const float px = l->pos[0];
    const float py = l->pos[1];
    const float pz = l->pos[2];
    const float fs = flip ? -1.0f : 1.0f;
    const float kc = l->kc;
    const float kl = l->kl;
    const float kq = l->kq;
    char *cp = (char *)col;
    GLuint i;

    for (i = 0; i < n; i++, ey++, me += mstride, cp += cstride, spc++)
    {
        const MGLMaterial *m = (const MGLMaterial *)me;
        const float nx = fs * ey->nx;
        const float ny = fs * ey->ny;
        const float nz = fs * ey->nz;
        float lx = px - ey->ex;
        float ly = py - ey->ey;
        float lz = pz - ey->ez;
        const float d2 = lx * lx + ly * ly + lz * lz + 1e-30f;
        const float d = sqrtf(d2);
        const float il = 1.0f / d;
        const float nd = (nx * lx + ny * ly + nz * lz) * il;
        const float att = 1.0f / (kc + kl * d + kq * d2);
        W3D_Color *c = (W3D_Color *)cp;

        lx *= il;
        ly *= il;
        lz *= il;

        d_lacc(c, l, m, att, nd > 0.0f ? att * nd : 0.0f);

        if (spec && nd > 0.0f)
            d_lspec(spc, l, m, ey, nx, ny, nz, lx, ly, lz, att, lv);
    }
}

static void d_lit_spot(const MGLLight *l, const char *me, GLuint mstride, const MGLEye *ey, W3D_Color *col, GLuint cstride, W3D_Color *spc, GLuint n, int spec, int flip, GLuint lv)
{
    const float px = l->pos[0];
    const float py = l->pos[1];
    const float pz = l->pos[2];
    const float fs = flip ? -1.0f : 1.0f;
    const float att0 = l->att0;
    const float dx = l->dir[0];
    const float dy = l->dir[1];
    const float dz = l->dir[2];
    const float cosco = l->spotCos;
    const float sexp = l->spotExp;
    char *cp = (char *)col;
    GLuint i;

    for (i = 0; i < n; i++, ey++, me += mstride, cp += cstride, spc++)
    {
        const MGLMaterial *m = (const MGLMaterial *)me;
        const float nx = fs * ey->nx;
        const float ny = fs * ey->ny;
        const float nz = fs * ey->nz;
        float lx = px - ey->ex;
        float ly = py - ey->ey;
        float lz = pz - ey->ez;
        const float il = 1.0f / sqrtf(lx * lx + ly * ly + lz * lz + 1e-30f);
        const float cone = -((lx * dx + ly * dy + lz * dz) * il);
        float nd;
        float att;
        W3D_Color *c;

        if (cone < cosco)
            continue;

        att = att0 * (sexp != 0.0f ? powf(cone, sexp) : 1.0f);
        nd = (nx * lx + ny * ly + nz * lz) * il;
        lx *= il;
        ly *= il;
        lz *= il;
        c = (W3D_Color *)cp;

        d_lacc(c, l, m, att, nd > 0.0f ? att * nd : 0.0f);

        if (spec && nd > 0.0f)
            d_lspec(spc, l, m, ey, nx, ny, nz, lx, ly, lz, att, lv);
    }
}

static void d_lit_spota(const MGLLight *l, const char *me, GLuint mstride, const MGLEye *ey, W3D_Color *col, GLuint cstride, W3D_Color *spc, GLuint n, int spec, int flip, GLuint lv)
{
    const float px = l->pos[0];
    const float py = l->pos[1];
    const float pz = l->pos[2];
    const float fs = flip ? -1.0f : 1.0f;
    const float kc = l->kc;
    const float kl = l->kl;
    const float kq = l->kq;
    const float dx = l->dir[0];
    const float dy = l->dir[1];
    const float dz = l->dir[2];
    const float cosco = l->spotCos;
    const float sexp = l->spotExp;
    char *cp = (char *)col;
    GLuint i;

    for (i = 0; i < n; i++, ey++, me += mstride, cp += cstride, spc++)
    {
        const MGLMaterial *m = (const MGLMaterial *)me;
        const float nx = fs * ey->nx;
        const float ny = fs * ey->ny;
        const float nz = fs * ey->nz;
        float lx = px - ey->ex;
        float ly = py - ey->ey;
        float lz = pz - ey->ez;
        const float d2 = lx * lx + ly * ly + lz * lz + 1e-30f;
        const float d = sqrtf(d2);
        const float il = 1.0f / d;
        const float cone = -((lx * dx + ly * dy + lz * dz) * il);
        float nd;
        float att;
        W3D_Color *c;

        if (cone < cosco)
            continue;

        att = (sexp != 0.0f ? powf(cone, sexp) : 1.0f) / (kc + kl * d + kq * d2);
        nd = (nx * lx + ny * ly + nz * lz) * il;
        lx *= il;
        ly *= il;
        lz *= il;
        c = (W3D_Color *)cp;

        d_lacc(c, l, m, att, nd > 0.0f ? att * nd : 0.0f);

        if (spec && nd > 0.0f)
            d_lspec(spc, l, m, ey, nx, ny, nz, lx, ly, lz, att, lv);
    }
}

// merge spec into primary unless separate, clamp, and report specular-any
static int d_lmerge(W3D_Color *col, GLuint cstride, const W3D_Color *sp, GLuint i0, GLuint nv, int sep)
{
    const char *cp = (const char *)col + i0 * cstride;
    int any = 0;
    GLuint i;

    for (i = i0; i < nv; i++, cp += cstride)
    {
        W3D_Color *c = (W3D_Color *)cp;

        if (sep)
        {
            if (sp->r != 0.0f || sp->g != 0.0f || sp->b != 0.0f)
                any = 1;
        }
        else
        {
            c->r += sp->r;
            c->g += sp->g;
            c->b += sp->b;
        }

        if (c->r > 1.0f)
            c->r = 1.0f;
        else if (c->r < 0.0f)
            c->r = 0.0f;

        if (c->g > 1.0f)
            c->g = 1.0f;
        else if (c->g < 0.0f)
            c->g = 0.0f;

        if (c->b > 1.0f)
            c->b = 1.0f;
        else if (c->b < 0.0f)
            c->b = 0.0f;

        if (c->a > 1.0f)
            c->a = 1.0f;
        else if (c->a < 0.0f)
            c->a = 0.0f;

        sp++;
    }

    return any;
}

// fold lights into v.color, back-face and specular land in their own arrays
static int d_light(GLcontext context, GLuint i0, GLuint nv)
{
    MGLVertex *vb = context->VertexBuffer;
    MGLEye *ey = context->EyeScratch;
    const MGLNormal *nb = context->NormalBuffer;
    const float *mv = context->ModelView[context->ModelViewNr].v;
    const int two = context->ModelTwoSide;
    const int sep = context->ModelSepSpec;
    const int cm = context->ColorMaterial_State;
    const GLuint cap = context->VertexBufferSize + MGL_CLIP_SLACK;
    const int ren = context->Normalize_State || context->Rescale_State;
    const int lv = context->ModelLocalViewer;
    const int trf = context->ColorMaterialFace != GL_BACK;
    const int trb = context->ColorMaterialFace != GL_FRONT;
    const int nl = context->LightMask != 0;
    const float ar = context->ModelAmbient.r;
    const float ag = context->ModelAmbient.g;
    const float ab = context->ModelAmbient.b;
    const float aa = context->ModelAmbient.a;
    const MGLMaterial *mb0 = context->Material + 0;
    const MGLMaterial *mb1 = context->Material + 1;
    GLfloat ir[9];
    GLfloat rs = 0.0f;
    int specany;
    int sides;
    int l;
    int s;
    GLuint i;

    // no enabled light leaves only the base ambient+emission term
    if (nl)
    {
        mglUpdateInvRot(context);

        memcpy(ir, context->InvRot, sizeof ir);

        // uniform-scale modelviews rescale normals by 1/scale, zero when exact
        rs = context->Rescale_State ? mv[2] * mv[2] + mv[6] * mv[6] + mv[10] * mv[10] : 0.0f;
        rs = rs > 0.0f ? 1.0f / sqrtf(rs) : 1.0f;
    }

    for (i = i0; i < nv; i++)
    {
        MGLVertex *v = vb + i;
        const W3D_Color src = v->v.color;
        const MGLMaterial *m0 = mb0;
        const MGLMaterial *m1 = mb1;
        W3D_Color *lb;
        W3D_Color *sf;
        W3D_Color *sb;

        if (nl)
        {
            MGLEye *e = ey + i;
            const MGLNormal *nn = nb + v->normal;
            const float x = v->bx;
            const float y = v->by;
            const float z = v->bz;
            const float w = v->bw;
            float nx = ir[0] * nn->x + ir[3] * nn->y + ir[6] * nn->z;
            float ny = ir[1] * nn->x + ir[4] * nn->y + ir[7] * nn->z;
            float nz = ir[2] * nn->x + ir[5] * nn->y + ir[8] * nn->z;

            e->ex = mv[0] * x + mv[4] * y + mv[8] * z + mv[12] * w;
            e->ey = mv[1] * x + mv[5] * y + mv[9] * z + mv[13] * w;
            e->ez = mv[2] * x + mv[6] * y + mv[10] * z + mv[14] * w;

            if (ren)
            {
                const float il = nx * nx + ny * ny + nz * nz;
                const float rl = il > 0.0f ? rs / sqrtf(il) : 0.0f;

                nx *= rl;
                ny *= rl;
                nz *= rl;
            }

            e->nx = nx;
            e->ny = ny;
            e->nz = nz;
        }

        if (cm)
        {
            MGLMaterial *me = context->MeScratch + i;

            *me = context->Material[0];

            if (trf)
                lt_cmtrack(context->ColorMaterialMode, me, &src);

            m0 = me;

            if (two)
            {
                MGLMaterial *mb = context->MeScratch + cap + i;

                *mb = context->Material[1];

                if (trb)
                    lt_cmtrack(context->ColorMaterialMode, mb, &src);

                m1 = mb;
            }
        }

        v->v.color.r = m0->emission.r + ar * m0->ambient.r;
        v->v.color.g = m0->emission.g + ag * m0->ambient.g;
        v->v.color.b = m0->emission.b + ab * m0->ambient.b;
        v->v.color.a = m0->emission.a + aa * m0->ambient.a;

        if (two)
        {
            lb = context->LitBack + i;
            sb = context->LitSpecB + i;

            lb->r = m1->emission.r + ar * m1->ambient.r;
            lb->g = m1->emission.g + ag * m1->ambient.g;
            lb->b = m1->emission.b + ab * m1->ambient.b;
            lb->a = m1->emission.a + aa * m1->ambient.a;

            sb->r = sb->g = sb->b = sb->a = 0.0f;
        }

        sf = context->LitSpec + i;

        sf->r = sf->g = sf->b = sf->a = 0.0f;
    }

    sides = two ? 2 : 1;

    for (l = 0; l < MGL_MAX_LIGHTS; l++)
    {
        const MGLLight *li = context->Light + l;
        const int lsp = li->specular.r != 0.0f || li->specular.g != 0.0f || li->specular.b != 0.0f;

        if (!(context->LightMask & (1u << l)))
            continue;

        for (s = 0; s < sides; s++)
        {
            const MGLMaterial *mt = context->Material + s;
            const int spec = lsp && (mt->specular.r != 0.0f || mt->specular.g != 0.0f || mt->specular.b != 0.0f || (cm && context->ColorMaterialMode == GL_SPECULAR && (s ? trb : trf)));
            const char *me = (const char *)(cm ? context->MeScratch + s * cap + i0 : context->Material + s);
            const GLuint ms = cm ? (GLuint)sizeof(MGLMaterial) : 0;
            W3D_Color *spc = (s ? context->LitSpecB : context->LitSpec) + i0;
            W3D_Color *col = s ? context->LitBack + i0 : &vb[i0].v.color;
            const GLuint cs = s ? (GLuint)sizeof(W3D_Color) : (GLuint)sizeof(MGLVertex);
            const MGLEye *e0 = ey + i0;
            const GLuint n = nv - i0;
            const int fl = li->flags;

            if (fl & MGL_LF_DIR)
                d_lit_dir(li, me, ms, e0, col, cs, spc, n, spec, s, lv);
            else if (fl & MGL_LF_SPOT)
            {
                if (fl & MGL_LF_ATTEN)
                    d_lit_spota(li, me, ms, e0, col, cs, spc, n, spec, s, lv);
                else
                    d_lit_spot(li, me, ms, e0, col, cs, spc, n, spec, s, lv);
            }
            else if (fl & MGL_LF_ATTEN)
                d_lit_posa(li, me, ms, e0, col, cs, spc, n, spec, s, lv);
            else
                d_lit_pos(li, me, ms, e0, col, cs, spc, n, spec, s, lv);
        }
    }

    specany = d_lmerge(&vb[0].v.color, sizeof(MGLVertex), context->LitSpec, i0, nv, sep);

    if (!two)
        return specany;

    if (d_lmerge(context->LitBack, sizeof(W3D_Color), context->LitSpecB, i0, nv, sep))
        specany |= 2;

    return specany;
}

static void d_texgen(GLcontext context, MGLVertex *v, GLuint n)
{
    const int gs = context->TextureGenS_State;
    const int gt = context->TextureGenT_State;
    const GLenum ms = context->TexGenModeS;
    const GLenum mt = context->TexGenModeT;
    const int sph = (gs && ms == GL_SPHERE_MAP) || (gt && mt == GL_SPHERE_MAP);
    const int eye = sph || (gs && ms == GL_EYE_LINEAR) || (gt && mt == GL_EYE_LINEAR);
    const MGLNormal *nb = context->NormalBuffer;
    GLfloat mv[16];
    GLfloat ir[9];
    GLfloat os[4];
    GLfloat ot[4];
    GLfloat es[4];
    GLfloat et[4];
    GLuint i;

    if (sph)
        mglUpdateInvRot(context);

    if (eye)
        memcpy(mv, context->ModelView[context->ModelViewNr].v, sizeof mv);

    if (sph)
        memcpy(ir, context->InvRot, sizeof ir);

    memcpy(os, context->ObjectPlaneS, sizeof os);
    memcpy(ot, context->ObjectPlaneT, sizeof ot);
    memcpy(es, context->EyePlaneS, sizeof es);
    memcpy(et, context->EyePlaneT, sizeof et);

    for (i = 0; i < n; i++, v++)
    {
        const float x = v->bx;
        const float y = v->by;
        const float z = v->bz;
        const float w = v->bw;
        float ex = 0.0f;
        float ey = 0.0f;
        float ez = 0.0f;
        float ew = 1.0f;
        float rs = 0.5f;
        float rt = 0.5f;

        if (eye)
        {
            ex = mv[0] * x + mv[4] * y + mv[8] * z + mv[12] * w;
            ey = mv[1] * x + mv[5] * y + mv[9] * z + mv[13] * w;
            ez = mv[2] * x + mv[6] * y + mv[10] * z + mv[14] * w;
            ew = mv[3] * x + mv[7] * y + mv[11] * z + mv[15] * w;
        }

        if (sph)
        {
            const MGLNormal *nn = nb + v->normal;
            float nx = ir[0] * nn->x + ir[3] * nn->y + ir[6] * nn->z;
            float ny = ir[1] * nn->x + ir[4] * nn->y + ir[7] * nn->z;
            float nz = ir[2] * nn->x + ir[5] * nn->y + ir[8] * nn->z;
            float il = nx * nx + ny * ny + nz * nz;
            float ux = 0.0f;
            float uy = 0.0f;
            float uz = 0.0f;
            float dp;
            float m2;

            if (il > 0.0f)
            {
                il = 1.0f / sqrtf(il);

                nx *= il;
                ny *= il;
                nz *= il;
            }

            il = ex * ex + ey * ey + ez * ez;

            if (il > 0.0f)
            {
                il = 1.0f / sqrtf(il);

                ux = ex * il;
                uy = ey * il;
                uz = ez * il;
            }

            dp = 2.0f * (nx * ux + ny * uy + nz * uz);

            ux -= dp * nx;
            uy -= dp * ny;
            uz -= dp * nz;

            m2 = 2.0f * sqrtf(ux * ux + uy * uy + (uz + 1.0f) * (uz + 1.0f));

            if (m2 > 0.0f)
            {
                const float im2 = 1.0f / m2;

                rs = ux * im2 + 0.5f;
                rt = uy * im2 + 0.5f;
            }
        }

        if (gs)
        {
            if (ms == GL_SPHERE_MAP)
                v->v.u = (W3D_Float)rs;
            else if (ms == GL_OBJECT_LINEAR)
                v->v.u = (W3D_Float)(os[0] * x + os[1] * y + os[2] * z + os[3] * w);
            else
                v->v.u = (W3D_Float)(es[0] * ex + es[1] * ey + es[2] * ez + es[3] * ew);
        }

        if (gt)
        {
            if (mt == GL_SPHERE_MAP)
                v->v.v = (W3D_Float)rt;
            else if (mt == GL_OBJECT_LINEAR)
                v->v.v = (W3D_Float)(ot[0] * x + ot[1] * y + ot[2] * z + ot[3] * w);
            else
                v->v.v = (W3D_Float)(et[0] * ex + et[1] * ey + et[2] * ez + et[3] * ew);
        }
    }
}

static void d_projscratch(GLcontext context, GLuint free)
{
    MGLVertex *vb = context->VertexBuffer;
    GLuint i;

    for (i = context->VertexBufferSize; i < free; i++)
        d_vtos(context, &vb[i]);
}

// clip one prim and emit what survives, pending batch flushes first
static void d_clip_emit(GLcontext context, ULONG prim, const UWORD *in, GLuint n, ULONG oc, GLuint prov)
{
    UWORD *out;
    GLuint free;
    GLuint m = cl_ClipIndex(context, in, n, oc, &out, &free);
    GLuint need = 1;

    if (prim == W3D_PRIMITIVE_TRIFAN || prim == W3D_PRIMITIVE_LINELOOP)
        need = 3;
    else if (prim == W3D_PRIMITIVE_LINES)
        need = 2;

    if (m < need)
        return;

    d_projscratch(context, free);

    if (prim == W3D_PRIMITIVE_TRIFAN && context->CullFace_State && !context->CullHW && d_backface(context, out[0], out[1], out[2]))
        return;

    if (d_flatvar)
    {
        GLuint i;

        if (prim == W3D_PRIMITIVE_POINTS)
        {
            for (i = 0; i < m; i++)
                d_flatpt(context, out[i]);

            return;
        }

        if (prim == W3D_PRIMITIVE_LINELOOP)
        {
            for (i = 0; i < m; i++)
                d_flatseg(context, out[i], out[i + 1 == m ? 0 : i + 1], context->LineWidth != 1.0f);

            return;
        }

        d_flatcol(context, prov);
    }

    W3D_DrawElements(context->w3dContext, prim, W3D_INDEX_UWORD, m, out);
}

// winding is flipped on screen, CW reads as front for GL_CCW
static int d_backface(GLcontext context, GLuint a, GLuint b, GLuint d)
{
    const MGLVertex *vb = context->VertexBuffer;

    if (context->CurrentCullFace == GL_FRONT_AND_BACK)
        return 1;

    const float x0 = vb[a].v.x;
    const float y0 = vb[a].v.y;
    const float mina = context->MinTriArea;
    float ar = (vb[b].v.x - x0) * (vb[d].v.y - y0) - (vb[d].v.x - x0) * (vb[b].v.y - y0);
    int back;

    if (context->CurrentFrontFace == GL_CW)
        ar = -ar;

    if (context->ClipUpperLeft)
        ar = -ar;

    back = ar >= 0.0f;

    return (context->CurrentCullFace == GL_BACK ? back : !back) || (ar < mina && ar > -mina);
}

static void d_point_byv(GLcontext context, const MGLVertex *v)
{
    W3D_Point p;

    p.v1 = v->v;
    p.tex = context->w3dTexBuffer[context->CurrentBinding];
    p.pointsize = context->CurrentPointSize;

    W3D_DrawPoint(context->w3dContext, &p);
}

static void d_line_byv(GLcontext context, const MGLVertex *a, const MGLVertex *b)
{
    W3D_Line l;

    l.v1 = a->v;
    l.v2 = b->v;
    l.tex = context->w3dTexBuffer[context->CurrentBinding];
    l.linewidth = context->LineWidth;
    l.st_enable = W3D_FALSE;
    l.st_pattern = 0;
    l.st_factor = 1;

    W3D_DrawLine(context->w3dContext, &l);
}

// varied flat shading emits one prim per call with its provoking color
static void d_flattri(GLcontext context, GLuint a, GLuint b, GLuint d)
{
    UWORD l[3];

    l[0] = (UWORD)a;
    l[1] = (UWORD)b;
    l[2] = (UWORD)d;

    d_flatcol(context, d);

    W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_TRIANGLES, W3D_INDEX_UWORD, 3, l);
}

static void d_flatquad(GLcontext context, GLuint a, GLuint b, GLuint c, GLuint d, GLuint prov)
{
    UWORD l[6];

    l[0] = (UWORD)a;
    l[1] = (UWORD)b;
    l[2] = (UWORD)c;
    l[3] = (UWORD)a;
    l[4] = (UWORD)c;
    l[5] = (UWORD)d;

    d_flatcol(context, prov);

    W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_TRIANGLES, W3D_INDEX_UWORD, 6, l);
}

static void d_flatseg(GLcontext context, GLuint a, GLuint b, int byv)
{
    d_flatcol(context, b);

    if (byv)
    {
        d_line_byv(context, &context->VertexBuffer[a], &context->VertexBuffer[b]);
        return;
    }

    UWORD l[2];

    l[0] = (UWORD)a;
    l[1] = (UWORD)b;

    W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_LINES, W3D_INDEX_UWORD, 2, l);
}

static void d_flatpt(GLcontext context, GLuint s)
{
    d_flatcol(context, s);

    if (context->CurrentPointSize != 1.0f)
    {
        d_point_byv(context, &context->VertexBuffer[s]);
        return;
    }

    UWORD p = (UWORD)s;

    W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_POINTS, W3D_INDEX_UWORD, 1, &p);
}

// one batch flush, the emit batch always holds a single w3d prim per submit
static void d_flush(GLcontext context, ULONG prim, UWORD *eb, GLuint *epos)
{
    if (*epos)
    {
        W3D_DrawElements(context->w3dContext, prim, W3D_INDEX_UWORD, *epos, eb);

        *epos = 0;
    }
}

// outline of one prim appended to the batch as lines or points
static void d_outline(GLcontext context, ULONG eprim, const UWORD *in, GLuint n, UWORD *eb, GLuint *epos)
{
    const MGLVertex *vb = context->VertexBuffer;
    const GLuint cap = context->VertexBufferSize + MGL_CLIP_SLACK;
    GLuint i;

    if (d_flatvar)
    {
        if (eprim == W3D_PRIMITIVE_POINTS)
        {
            for (i = 0; i < n; i++)
                d_flatpt(context, in[i]);
        }
        else
        {
            for (i = 0; i < n; i++)
                d_flatseg(context, in[i], in[i + 1 == n ? 0 : i + 1], context->LineWidth != 1.0f);
        }

        return;
    }

    if (eprim == W3D_PRIMITIVE_POINTS)
    {
        if (context->CurrentPointSize != 1.0f)
        {
            for (i = 0; i < n; i++)
                d_point_byv(context, &vb[in[i]]);

            return;
        }

        for (i = 0; i < n; i++)
        {
            if (*epos >= cap)
                d_flush(context, eprim, eb, epos);

            eb[(*epos)++] = in[i];
        }

        return;
    }

    if (context->LineWidth != 1.0f)
    {
        for (i = 0; i < n; i++)
            d_line_byv(context, &vb[in[i]], &vb[in[i + 1 == n ? 0 : i + 1]]);

        return;
    }

    for (i = 0; i < n; i++)
    {
        if (*epos > cap - 2)
            d_flush(context, eprim, eb, epos);

        eb[(*epos)++] = in[i];
        eb[(*epos)++] = in[i + 1 == n ? 0 : i + 1];
    }
}

// reject or clip one tri before its emit
static int d_classtri(GLcontext context, GLuint a, GLuint b, GLuint d, int cull, ULONG cmask, ULONG cprim, ULONG eprim, UWORD *eb, GLuint *epos)
{
    const MGLVertex *vb = context->VertexBuffer;
    const ULONG ac = vb[a].outcode & vb[b].outcode & vb[d].outcode;
    const ULONG oc = vb[a].outcode | vb[b].outcode | vb[d].outcode;
    UWORD l[3];

    if (ac)
        return 1;

    if (oc & cmask)
    {
        if (!(oc & MGL_CLIP_NEGW) && cull && d_backface(context, a, b, d))
            return 1;

        d_flush(context, eprim, eb, epos);

        l[0] = (UWORD)a;
        l[1] = (UWORD)b;
        l[2] = (UWORD)d;

        d_clip_emit(context, cprim, l, 3, oc, d);

        return 2;
    }

    if (cull && d_backface(context, a, b, d))
        return 1;

    return 0;
}

static void d_emit_pointrange(GLcontext context, const UWORD *idx, GLuint a, GLuint b, UWORD *eb, GLuint *epos)
{
    const MGLVertex *vb = context->VertexBuffer;
    const GLuint cap = context->VertexBufferSize + MGL_CLIP_SLACK;

    if (d_flatvar)
    {
        for (; a < b; a++)
            d_flatpt(context, idx[a]);

        return;
    }

    if (context->CurrentPointSize != 1.0f)
    {
        for (; a < b; a++)
            d_point_byv(context, &vb[idx[a]]);

        return;
    }

    for (; a < b; a++)
    {
        if (*epos >= cap)
            d_flush(context, W3D_PRIMITIVE_POINTS, eb, epos);

        eb[(*epos)++] = idx[a];
    }
}

// in walks an index list of slots, NULL means the plain 0..n sequence
static void d_emit_tris(GLcontext context, const UWORD *in, GLuint n, GLuint orall)
{
    const ULONG cmask = context->ClipFlags;
    const int cull = context->CullFace_State && !context->CullHW;
    const int pmode = context->CurPolygonMode;
    const ULONG eprim = pmode == GL_POINT ? W3D_PRIMITIVE_POINTS : pmode == GL_LINE ? W3D_PRIMITIVE_LINES
                                                                                    : W3D_PRIMITIVE_TRIANGLES;
    const ULONG cprim = pmode == GL_POINT ? W3D_PRIMITIVE_POINTS : pmode == GL_LINE ? W3D_PRIMITIVE_LINELOOP
                                                                                    : W3D_PRIMITIVE_TRIFAN;
    UWORD *eb = context->ClipIdx;
    const UWORD *idx = in ? in : context->IdentIdx;
    const GLuint cap = context->VertexBufferSize + MGL_CLIP_SLACK;
    GLuint epos = 0;
    GLuint i;

    if (!orall && !cull && pmode == GL_FILL && !d_flatvar)
    {
        const GLuint n3 = n - n % 3;

        if (n3)
            W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_TRIANGLES, W3D_INDEX_UWORD, n3, in ? (UWORD *)in : context->IdentIdx);

        return;
    }

    for (i = 0; i + 2 < n; i += 3)
    {
        const GLuint a = idx[i];
        const GLuint b = idx[i + 1];
        const GLuint d = idx[i + 2];

        if (d_classtri(context, a, b, d, cull, cmask, cprim, eprim, eb, &epos))
            continue;

        if (eprim == W3D_PRIMITIVE_TRIANGLES)
        {
            if (d_flatvar)
            {
                d_flattri(context, a, b, d);
                continue;
            }

            if (epos > cap - 3)
                d_flush(context, eprim, eb, &epos);

            eb[epos++] = (UWORD)a;
            eb[epos++] = (UWORD)b;
            eb[epos++] = (UWORD)d;
        }
        else
        {
            UWORD l[3];

            l[0] = (UWORD)a;
            l[1] = (UWORD)b;
            l[2] = (UWORD)d;

            if (!d_flatvar && eprim == W3D_PRIMITIVE_LINES && context->LineWidth == 1.0f)
                W3D_DrawElements(context->w3dContext, cprim, W3D_INDEX_UWORD, 3, l);
            else
                d_outline(context, eprim, l, 3, eb, &epos);
        }
    }

    d_flush(context, eprim, eb, &epos);
}

static void d_emit_tstrip(GLcontext context, const UWORD *in, GLuint n, GLuint orall)
{
    const ULONG cmask = context->ClipFlags;
    const int cull = context->CullFace_State && !context->CullHW;
    const int pmode = context->CurPolygonMode;
    const GLuint sbase = context->StripBase;
    const ULONG eprim = pmode == GL_POINT ? W3D_PRIMITIVE_POINTS : pmode == GL_LINE ? W3D_PRIMITIVE_LINES
                                                                                    : W3D_PRIMITIVE_TRIANGLES;
    const ULONG cprim = pmode == GL_POINT ? W3D_PRIMITIVE_POINTS : pmode == GL_LINE ? W3D_PRIMITIVE_LINELOOP
                                                                                    : W3D_PRIMITIVE_TRIFAN;
    UWORD *eb = context->ClipIdx;
    const UWORD *idx = in ? in : context->IdentIdx;
    GLuint epos = 0;
    GLuint next = in ? context->EmitSkip : context->EmitBase;
    GLuint i;

    if (!orall && !cull && pmode == GL_FILL && !(sbase & 1) && !d_flatvar)
    {
        if (n >= 3)
            W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_TRISTRIP, W3D_INDEX_UWORD, n, in ? (UWORD *)in : context->IdentIdx);

        return;
    }

    for (i = 0; i + 2 < n; i++)
    {
        const GLuint par = (sbase + i) & 1;
        const GLuint a = idx[i + par];
        const GLuint b = idx[i + 1 - par];
        const GLuint d = idx[i + 2];

        if (d_classtri(context, a, b, d, cull, cmask, cprim, eprim, eb, &epos))
            continue;

        if (eprim == W3D_PRIMITIVE_TRIANGLES)
        {
            if (d_flatvar)
                d_flattri(context, a, b, d);
            else
                d_emit_tri(context, eprim, eb, &epos, a, b, d);
        }
        else if (eprim == W3D_PRIMITIVE_POINTS)
        {
            d_emit_pointrange(context, idx, next > i ? next : i, i + 3, eb, &epos);

            next = i + 3;
        }
        else
        {
            UWORD l[3];

            l[0] = (UWORD)a;
            l[1] = (UWORD)b;
            l[2] = (UWORD)d;

            if (!d_flatvar && context->LineWidth == 1.0f)
                W3D_DrawElements(context->w3dContext, cprim, W3D_INDEX_UWORD, 3, l);
            else
                d_outline(context, eprim, l, 3, eb, &epos);
        }
    }

    d_flush(context, eprim, eb, &epos);
}

static void d_emit_fan(GLcontext context, const UWORD *in, GLuint n, GLuint orall)
{
    const ULONG cmask = context->ClipFlags;
    const int cull = context->CullFace_State && !context->CullHW;
    const int pmode = context->CurPolygonMode;
    const ULONG eprim = pmode == GL_POINT ? W3D_PRIMITIVE_POINTS : pmode == GL_LINE ? W3D_PRIMITIVE_LINES
                                                                                    : W3D_PRIMITIVE_TRIANGLES;
    const ULONG cprim = pmode == GL_POINT ? W3D_PRIMITIVE_POINTS : pmode == GL_LINE ? W3D_PRIMITIVE_LINELOOP
                                                                                    : W3D_PRIMITIVE_TRIFAN;
    UWORD *eb = context->ClipIdx;
    const UWORD *idx = in ? in : context->IdentIdx;
    const GLuint a = idx[0];
    GLuint epos = 0;
    GLuint next = in ? context->EmitSkip : context->EmitBase;
    GLuint i;

    if (!orall && !cull && pmode == GL_FILL && !d_flatvar)
    {
        if (n >= 3)
            W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_TRIFAN, W3D_INDEX_UWORD, n, in ? (UWORD *)in : context->IdentIdx);

        return;
    }

    for (i = 1; i + 1 < n; i++)
    {
        const GLuint b = idx[i];
        const GLuint d = idx[i + 1];

        if (d_classtri(context, a, b, d, cull, cmask, cprim, eprim, eb, &epos))
            continue;

        if (eprim == W3D_PRIMITIVE_TRIANGLES)
        {
            if (d_flatvar)
                d_flattri(context, a, b, d);
            else
                d_emit_tri(context, eprim, eb, &epos, a, b, d);
        }
        else if (eprim == W3D_PRIMITIVE_POINTS)
        {
            if (!next)
                d_emit_pointrange(context, idx, 0, 1, eb, &epos);

            d_emit_pointrange(context, idx, next > i ? next : i, i + 2, eb, &epos);

            next = i + 2;
        }
        else
        {
            UWORD l[3];

            l[0] = (UWORD)a;
            l[1] = (UWORD)b;
            l[2] = (UWORD)d;

            if (!d_flatvar && context->LineWidth == 1.0f)
                W3D_DrawElements(context->w3dContext, cprim, W3D_INDEX_UWORD, 3, l);
            else
                d_outline(context, eprim, l, 3, eb, &epos);
        }
    }

    d_flush(context, eprim, eb, &epos);
}

static void d_emit_tri(GLcontext context, ULONG prim, UWORD *eb, GLuint *epos, GLuint a, GLuint b, GLuint d)
{
    const GLuint cap = context->VertexBufferSize + MGL_CLIP_SLACK;

    if (*epos > cap - 3)
        d_flush(context, prim, eb, epos);

    eb[(*epos)++] = (UWORD)a;
    eb[(*epos)++] = (UWORD)b;
    eb[(*epos)++] = (UWORD)d;
}

// quads and quad strips decompose into two wound tris per quad
static void d_emit_quads(GLcontext context, const UWORD *in, GLuint n, GLuint orall)
{
    const MGLVertex *vb = context->VertexBuffer;
    const ULONG cmask = context->ClipFlags;
    const int cull = context->CullFace_State && !context->CullHW;
    const int pmode = context->CurPolygonMode;
    const ULONG eprim = pmode == GL_POINT ? W3D_PRIMITIVE_POINTS : pmode == GL_LINE ? W3D_PRIMITIVE_LINES
                                                                                    : W3D_PRIMITIVE_TRIANGLES;
    const ULONG cprim = pmode == GL_POINT ? W3D_PRIMITIVE_POINTS : pmode == GL_LINE ? W3D_PRIMITIVE_LINELOOP
                                                                                    : W3D_PRIMITIVE_TRIFAN;
    UWORD *eb = context->ClipIdx;
    const UWORD *idx = in ? in : context->IdentIdx;
    const GLuint cap = context->VertexBufferSize + MGL_CLIP_SLACK;
    GLuint epos = 0;
    GLuint i;

    for (i = 0; i + 3 < n; i += 4)
    {
        const GLuint q0 = idx[i];
        const GLuint q1 = idx[i + 1];
        const GLuint q2 = idx[i + 2];
        const GLuint q3 = idx[i + 3];
        UWORD l[4];

        l[0] = (UWORD)q0;
        l[1] = (UWORD)q1;
        l[2] = (UWORD)q2;
        l[3] = (UWORD)q3;

        if (orall)
        {
            const ULONG ac = vb[q0].outcode & vb[q1].outcode & vb[q2].outcode & vb[q3].outcode;
            const ULONG oc = vb[q0].outcode | vb[q1].outcode | vb[q2].outcode | vb[q3].outcode;

            if (ac)
                continue;

            if (oc & cmask)
            {
                if (!(oc & MGL_CLIP_NEGW) && cull && d_backface(context, q0, q1, q2))
                    continue;

                d_flush(context, eprim, eb, &epos);
                d_clip_emit(context, cprim, l, 4, oc, q3);

                continue;
            }
        }

        if (pmode != GL_FILL)
        {
            if (!cull || !d_backface(context, q0, q1, q2))
            {
                if (!d_flatvar && eprim == W3D_PRIMITIVE_LINES && context->LineWidth == 1.0f)
                    W3D_DrawElements(context->w3dContext, cprim, W3D_INDEX_UWORD, 4, l);
                else
                    d_outline(context, eprim, l, 4, eb, &epos);
            }

            continue;
        }

        if (cull)
        {
            if (!d_backface(context, q0, q1, q2))
            {
                if (d_flatvar)
                {
                    d_flatquad(context, q0, q1, q2, q3, q3);
                }
                else
                {
                    d_emit_tri(context, eprim, eb, &epos, q0, q1, q2);
                    d_emit_tri(context, eprim, eb, &epos, q0, q2, q3);
                }
            }

            continue;
        }

        if (d_flatvar)
        {
            d_flatquad(context, q0, q1, q2, q3, q3);
            continue;
        }

        if (epos > cap - 6)
            d_flush(context, eprim, eb, &epos);

        eb[epos++] = (UWORD)q0;
        eb[epos++] = (UWORD)q1;
        eb[epos++] = (UWORD)q2;
        eb[epos++] = (UWORD)q0;
        eb[epos++] = (UWORD)q2;
        eb[epos++] = (UWORD)q3;
    }

    d_flush(context, eprim, eb, &epos);
}

static void d_emit_qstrip(GLcontext context, const UWORD *in, GLuint n, GLuint orall)
{
    const MGLVertex *vb = context->VertexBuffer;
    const ULONG cmask = context->ClipFlags;
    const int cull = context->CullFace_State && !context->CullHW;
    const int pmode = context->CurPolygonMode;
    const ULONG eprim = pmode == GL_POINT ? W3D_PRIMITIVE_POINTS : pmode == GL_LINE ? W3D_PRIMITIVE_LINES
                                                                                    : W3D_PRIMITIVE_TRIANGLES;
    const ULONG cprim = pmode == GL_POINT ? W3D_PRIMITIVE_POINTS : pmode == GL_LINE ? W3D_PRIMITIVE_LINELOOP
                                                                                    : W3D_PRIMITIVE_TRIFAN;
    UWORD *eb = context->ClipIdx;
    const UWORD *idx = in ? in : context->IdentIdx;
    GLuint epos = 0;
    GLuint next = in ? context->EmitSkip : context->EmitBase;
    GLuint i;

    for (i = 0; i + 3 < n; i += 2)
    {
        const GLuint q0 = idx[i];
        const GLuint q1 = idx[i + 1];
        const GLuint q2 = idx[i + 2];
        const GLuint q3 = idx[i + 3];
        UWORD l[4];

        l[0] = (UWORD)q0;
        l[1] = (UWORD)q1;
        l[2] = (UWORD)q3;
        l[3] = (UWORD)q2;

        if (orall)
        {
            const ULONG ac = vb[q0].outcode & vb[q1].outcode & vb[q2].outcode & vb[q3].outcode;
            const ULONG oc = vb[q0].outcode | vb[q1].outcode | vb[q2].outcode | vb[q3].outcode;

            if (ac)
                continue;

            if (oc & cmask)
            {
                if (!(oc & MGL_CLIP_NEGW) && cull && d_backface(context, q0, q1, q3))
                    continue;

                d_flush(context, eprim, eb, &epos);
                d_clip_emit(context, cprim, l, 4, oc, q3);

                continue;
            }
        }

        if (pmode != GL_FILL)
        {
            if (!cull || !d_backface(context, q0, q1, q3))
            {
                if (eprim == W3D_PRIMITIVE_POINTS)
                {
                    d_emit_pointrange(context, idx, next > i ? next : i, i + 4, eb, &epos);

                    next = i + 4;
                }
                else if (!d_flatvar && context->LineWidth == 1.0f)
                    W3D_DrawElements(context->w3dContext, cprim, W3D_INDEX_UWORD, 4, l);
                else
                    d_outline(context, eprim, l, 4, eb, &epos);
            }

            continue;
        }

        if (cull)
        {
            if (!d_backface(context, q0, q1, q3))
            {
                if (d_flatvar)
                {
                    d_flatquad(context, q0, q1, q3, q2, q3);
                }
                else
                {
                    d_emit_tri(context, eprim, eb, &epos, q0, q1, q3);
                    d_emit_tri(context, eprim, eb, &epos, q0, q3, q2);
                }
            }

            continue;
        }

        if (d_flatvar)
        {
            d_flatquad(context, q0, q1, q3, q2, q3);
            continue;
        }

        d_emit_tri(context, eprim, eb, &epos, q0, q1, q3);
        d_emit_tri(context, eprim, eb, &epos, q0, q3, q2);
    }

    d_flush(context, eprim, eb, &epos);
}

static void d_emit_poly(GLcontext context, const UWORD *in, GLuint n, GLuint orall, GLboolean final)
{
    const MGLVertex *vb = context->VertexBuffer;
    const ULONG cmask = context->ClipFlags;
    const int pmode = context->CurPolygonMode;
    const UWORD *idx = in ? in : context->IdentIdx;
    ULONG ac = ~0UL;
    GLuint i;

    if (n < 3)
        return;

    if (orall)
    {
        for (i = 0; i < n; i++)
            ac &= vb[idx[i]].outcode;

        if (ac)
            return;
    }

    if (context->CullFace_State && !context->CullHW && !(orall & MGL_CLIP_NEGW) && d_backface(context, idx[0], idx[1], idx[2]))
        return;

    if (pmode == GL_LINE)
    {
        d_emit_lloop(context, in, n, orall, final);
        return;
    }

    if (orall & cmask)
    {
        d_clip_emit(context, pmode == GL_POINT ? W3D_PRIMITIVE_POINTS : W3D_PRIMITIVE_TRIFAN, (UWORD *)idx, n, orall, idx[n - 1]);
        return;
    }

    if (pmode == GL_FILL)
    {
        if (d_flatvar)
            d_flatcol(context, idx[n - 1]);

        W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_TRIFAN, W3D_INDEX_UWORD, n, in ? (UWORD *)in : context->IdentIdx);

        return;
    }

    const GLuint psk = in ? context->EmitSkip : context->EmitBase;

    if (d_flatvar)
    {
        for (i = psk; i < n; i++)
            d_flatpt(context, idx[i]);

        return;
    }

    W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_POINTS, W3D_INDEX_UWORD, n - psk, (in ? (UWORD *)in : context->IdentIdx) + psk);
}

static void d_emit_points(GLcontext context, const UWORD *in, GLuint n, GLuint orall)
{
    const MGLVertex *vb = context->VertexBuffer;
    const int byv = context->CurrentPointSize != 1.0f;
    UWORD *eb = context->ClipIdx;
    const UWORD *idx = in ? in : context->IdentIdx;
    const GLuint cap = context->VertexBufferSize + MGL_CLIP_SLACK;
    GLuint epos = 0;
    GLuint i;

    if (!orall && !byv && !d_flatvar)
    {
        if (n)
            W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_POINTS, W3D_INDEX_UWORD, n, in ? (UWORD *)in : context->IdentIdx);

        return;
    }

    for (i = 0; i < n; i++)
    {
        const GLuint s = idx[i];

        if (vb[s].outcode)
            continue;

        if (d_flatvar)
        {
            d_flatpt(context, s);
            continue;
        }

        if (byv)
        {
            d_point_byv(context, &vb[s]);
            continue;
        }

        if (epos >= cap)
            d_flush(context, W3D_PRIMITIVE_POINTS, eb, &epos);

        eb[epos++] = (UWORD)s;
    }

    d_flush(context, W3D_PRIMITIVE_POINTS, eb, &epos);
}

// segments and strips share the pair path, the loop adds the wrap edge
static void d_segemit(GLcontext context, GLuint a, GLuint b, int byv, ULONG cmask, UWORD *eb, GLuint *epos)
{
    const MGLVertex *vb = context->VertexBuffer;
    const GLuint cap = context->VertexBufferSize + MGL_CLIP_SLACK;
    const ULONG oc = vb[a].outcode | vb[b].outcode;

    if (vb[a].outcode & vb[b].outcode)
        return;

    if (oc & cmask)
    {
        UWORD l[2];
        UWORD *out;
        GLuint free;
        GLuint m;

        if (*epos)
            d_flush(context, W3D_PRIMITIVE_LINES, eb, epos);

        l[0] = (UWORD)a;
        l[1] = (UWORD)b;

        m = cl_ClipIndex(context, l, 2, oc, &out, &free);

        if (m < 2)
            return;

        d_projscratch(context, free);

        if (d_flatvar)
            d_flatcol(context, b);

        if (byv)
            d_line_byv(context, &vb[out[0]], &vb[out[1]]);
        else
            W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_LINES, W3D_INDEX_UWORD, m, out);

        return;
    }

    if (d_flatvar)
    {
        d_flatseg(context, a, b, byv);
        return;
    }

    if (byv)
    {
        d_line_byv(context, &vb[a], &vb[b]);
        return;
    }

    if (*epos > cap - 2)
        d_flush(context, W3D_PRIMITIVE_LINES, eb, epos);

    eb[(*epos)++] = (UWORD)a;
    eb[(*epos)++] = (UWORD)b;
}

static void d_emit_lines(GLcontext context, const UWORD *in, GLuint n, GLuint orall)
{
    const ULONG cmask = context->ClipFlags;
    const int byv = context->LineWidth != 1.0f;
    UWORD *eb = context->ClipIdx;
    const UWORD *idx = in ? in : context->IdentIdx;
    GLuint epos = 0;
    GLuint i;

    if (!orall && !byv && !d_flatvar)
    {
        const GLuint n2 = n - n % 2;

        if (n2)
            W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_LINES, W3D_INDEX_UWORD, n2, in ? (UWORD *)in : context->IdentIdx);

        return;
    }

    for (i = 0; i + 1 < n; i += 2)
        d_segemit(context, idx[i], idx[i + 1], byv, cmask, eb, &epos);

    d_flush(context, W3D_PRIMITIVE_LINES, eb, &epos);
}

static void d_emit_lstrip(GLcontext context, const UWORD *in, GLuint n, GLuint orall)
{
    const ULONG cmask = context->ClipFlags;
    const int byv = context->LineWidth != 1.0f;
    UWORD *eb = context->ClipIdx;
    const UWORD *idx = in ? in : context->IdentIdx;
    GLuint epos = 0;
    GLuint i;

    if (!orall && !byv && !d_flatvar)
    {
        if (n >= 2)
            W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_LINESTRIP, W3D_INDEX_UWORD, n, in ? (UWORD *)in : context->IdentIdx);

        return;
    }

    for (i = 0; i + 1 < n; i++)
        d_segemit(context, idx[i], idx[i + 1], byv, cmask, eb, &epos);

    d_flush(context, W3D_PRIMITIVE_LINES, eb, &epos);
}

static void d_emit_lloop(GLcontext context, const UWORD *in, GLuint n, GLuint orall, GLboolean final)
{
    const ULONG cmask = context->ClipFlags;
    const int byv = context->LineWidth != 1.0f;
    UWORD *eb = context->ClipIdx;
    const UWORD *idx = in ? in : context->IdentIdx;
    GLuint epos = 0;
    GLuint i;

    // seeds lead the walk space, position 0 is only the wrap target
    const GLuint i0 = context->EmitBase || (in && context->EmitSkip) ? 1 : 0;

    if (!orall && !byv && !context->EmitBase && !(in && context->EmitSkip) && !d_flatvar)
    {
        if (n >= 2)
        {
            if (final && n >= 3)
                W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_LINELOOP, W3D_INDEX_UWORD, n, in ? (UWORD *)in : context->IdentIdx);
            else
                W3D_DrawElements(context->w3dContext, W3D_PRIMITIVE_LINESTRIP, W3D_INDEX_UWORD, n, in ? (UWORD *)in : context->IdentIdx);
        }

        return;
    }

    for (i = i0; i + 1 < n; i++)
        d_segemit(context, idx[i], idx[i + 1], byv, cmask, eb, &epos);

    if (final && n >= 3)
        d_segemit(context, idx[n - 1], idx[0], byv, cmask, eb, &epos);

    d_flush(context, W3D_PRIMITIVE_LINES, eb, &epos);
}

// mode dispatch shared by both passes of a multitex submit
static void d_emit(GLcontext context, const UWORD *in, GLuint n, GLuint orall, GLboolean final)
{
    switch (context->CurrentPrimitive)
    {
    case GL_POINTS:
        d_emit_points(context, in, n, orall);
        break;

    case GL_LINES:
        d_emit_lines(context, in, n, orall);
        break;

    case GL_LINE_STRIP:
        d_emit_lstrip(context, in, n, orall);
        break;

    case GL_LINE_LOOP:
        d_emit_lloop(context, in, n, orall, final);
        break;

    case GL_TRIANGLES:
        d_emit_tris(context, in, n, orall);
        break;

    case GL_TRIANGLE_STRIP:
        d_emit_tstrip(context, in, n, orall);
        break;

    case GL_TRIANGLE_FAN:
        d_emit_fan(context, in, n, orall);
        break;

    case GL_QUADS:
        d_emit_quads(context, in, n, orall);
        break;

    case GL_QUAD_STRIP:
        d_emit_qstrip(context, in, n, orall);
        break;

    case GL_POLYGON:
        d_emit_poly(context, in, n, orall, final);
        break;
    }
}

static void mtex_unitspec(GLenum env, ULONG *wenv, ULONG *src, ULONG *dst)
{
    switch (env)
    {
    case GL_REPLACE:
        *wenv = W3D_REPLACE;
        *src = W3D_ONE;
        *dst = W3D_ZERO;
        break;

    case GL_DECAL:
        *wenv = W3D_REPLACE;
        *src = W3D_SRC_ALPHA;
        *dst = W3D_ONE_MINUS_SRC_ALPHA;
        break;

    case GL_BLEND:
        *wenv = W3D_BLEND;
        *src = W3D_ONE;
        *dst = W3D_ZERO;
        break;

    default: // GL_MODULATE out * T_k = fb * frag
        *wenv = W3D_REPLACE;
        *src = W3D_DST_COLOR;
        *dst = W3D_ZERO;
        break;
    }
}

// extra lighting passes reuse the emit machinery with colors swapped in cullface 0 means no facing test, nonzero forces a cpu test for that side
static void d_lpass(GLcontext context, const UWORD *in, GLuint n, GLuint orall, GLboolean final, GLuint nv, const W3D_Color *src, GLenum cullface, int add)
{
    MGLVertex *vb = context->VertexBuffer;
    const GLboolean cs = context->CullFace_State;
    const GLenum cf = context->CurrentCullFace;
    const int chw = context->CullHW;
    GLuint i;

    for (i = 0; i < nv; i++)
        vb[i].v.color = src[i];

    if (cullface)
    {
        context->CullFace_State = GL_TRUE;
        context->CurrentCullFace = cullface;
    }
    else
        context->CullFace_State = GL_FALSE;

    context->CullHW = GL_FALSE;

    W3D_SetState(context->w3dContext, W3D_CULLFACE, W3D_DISABLE);
    W3D_SetZCompareMode(context->w3dContext, W3D_Z_EQUAL);

    if (add)
    {
        W3D_SetState(context->w3dContext, W3D_TEXMAPPING, W3D_DISABLE);
        W3D_SetState(context->w3dContext, W3D_BLENDING, W3D_ENABLE);
        W3D_SetBlendMode(context->w3dContext, W3D_ONE, W3D_ONE);
    }

    d_emit(context, in, n, orall, final);

    if (add)
    {
        W3D_SetState(context->w3dContext, W3D_TEXMAPPING, (context->Texture2D_State[0] && context->w3dTexBuffer[context->CurrentBinding]) ? W3D_ENABLE : W3D_DISABLE);
        W3D_SetState(context->w3dContext, W3D_BLENDING, context->Blend_State ? W3D_ENABLE : W3D_DISABLE);
        W3D_SetBlendMode(context->w3dContext, mgl_blend_src(context->CurBlendSrc), mgl_blend_dst(context->CurBlendDst));
    }

    W3D_SetZCompareMode(context->w3dContext, mgl_depthw(context->CurDepthFunc));

    if (chw)
        W3D_SetState(context->w3dContext, W3D_CULLFACE, W3D_ENABLE);

    context->CullFace_State = cs;
    context->CurrentCullFace = cf;
    context->CullHW = chw;
}

int dx_lock(GLcontext context)
{
#ifdef AUTOMATIC_LOCKING_ENABLE
    if (context->LockMode == MGL_LOCK_AUTOMATIC && !context->w3dLocked)
    {
        if (W3D_LockHardware(context->w3dContext) != W3D_SUCCESS)
            return -1;

        context->w3dLocked = GL_TRUE;
        context->SmartLock = GL_TRUE;
    }
    else if (context->LockMode == MGL_LOCK_SMART && !context->w3dLocked)
    {
        if (W3D_LockHardware(context->w3dContext) != W3D_SUCCESS)
            return -1;

        context->w3dLocked = GL_TRUE;
        context->SmartLock = GL_TRUE;

        TMA_Start(&context->LockTime);

        return 1;
    }
    else if (context->LockMode == MGL_LOCK_SMART && context->SmartLock)
        return 1;
    else if (context->LockMode == MGL_LOCK_MANUAL && !context->w3dLocked)
    {
        if (W3D_LockHardware(context->w3dContext) != W3D_SUCCESS)
            return -1;

        context->w3dLocked = GL_TRUE;

        return 2;
    }

    return 0;
#else
    if (!context->w3dLocked)
    {
        if (W3D_LockHardware(context->w3dContext) != W3D_SUCCESS)
            return -1;

        context->w3dLocked = GL_TRUE;

        return 2;
    }

    return 0;
#endif
}

void dx_unlock(GLcontext context, int st)
{
#ifdef AUTOMATIC_LOCKING_ENABLE
    if (st == 2 || (context->LockMode == MGL_LOCK_AUTOMATIC && context->SmartLock))
    {
        W3D_UnLockHardware(context->w3dContext);

        context->w3dLocked = GL_FALSE;
        context->SmartLock = GL_FALSE;
    }
    else if (st == 1 && TMA_GetTime(&context->LockTime) > MGL_SMARTLOCK_SECS)
    {
        W3D_UnLockHardware(context->w3dContext);

        context->w3dLocked = GL_FALSE;
        context->SmartLock = GL_FALSE;
    }
#else
    if (st == 2)
    {
        W3D_UnLockHardware(context->w3dContext);

        context->w3dLocked = GL_FALSE;
    }
#endif
}

void d_Submit(GLcontext context, const UWORD *in, GLuint icount, GLboolean final)
{
    const GLuint n = in ? icount : context->VertexBufferPointer;
    const GLuint nv = context->VertexBufferPointer;
    const GLuint i0 = context->EmitBase;
    const GLenum mode = context->CurrentPrimitive;
    MGLVertex *vb = context->VertexBuffer;
    const float *m = context->CombinedMatrix.v;
    GLuint orall;
    GLuint keep;
    GLuint i;
    GLuint xhi;
    int lspec = 0;
    GLboolean reused;

    if (!n)
        return;

    const GLboolean cval = context->CombinedValid;

    mglUpdateCombined(context);

    if (context->LockXformed && (!cval || context->Lighting_State || context->TextureGenS_State || context->TextureGenT_State))
    {
        va_StageRange(context, context->LockFirst, context->LockCount, MGL_EXTRA_MAX);

        context->LockXformed = GL_FALSE;
    }

    reused = context->LockXformed && context->LockStaged;
    xhi = reused ? context->LockExtra : nv;

    if (context->TextureGenS_State || context->TextureGenT_State)
        d_texgen(context, vb + i0, xhi - i0);

    if (context->Lighting_State)
        lspec = d_light(context, i0, xhi);

    if (!(context->CombinedMatrix.flags & MGLMAT_IDENTITY))
    {
        if (context->CombinedMatrix.flags & MGLMAT_ORTHO)
        {
            if (!(m[1] || m[2] || m[4] || m[6] || m[8] || m[9]))
                d_xdiag(vb + i0, m, xhi - i0);
            else
                d_xaff(vb + i0, m, xhi - i0);
        }
        else if (context->WOne_Hint)
        {
            d_xgenw(vb + i0, m, xhi - i0);
        }
        else
        {
            d_xgen(vb + i0, m, xhi - i0);
        }
    }

    // hint consumed for this window, the next stage re-accumulates it
    context->WOne_Hint = GL_TRUE;
    context->LockXformed = context->LockStaged && !context->Lighting_State && !context->TextureGenS_State && !context->TextureGenT_State;
    orall = d_screenv(context, i0, xhi);

    for (i = 0; i < i0; i++)
        orall |= vb[i].outcode;

    if (reused)
        orall |= context->LockOrall;
    else if (context->LockXformed)
    {
        GLuint worall = 0;

        for (i = MGL_EXTRA_MAX; i < nv; i++)
            worall |= vb[i].outcode;

        context->LockOrall = worall;
    }

    // one scan per submit decides uniform vs per-prim colors under flat
    d_flatvar = 0;
    d_flatvalid = 0;

    if (context->ShadeModel == GL_FLAT)
    {
        const ULONG *c0 = (const ULONG *)&vb[0].v.color;

        for (i = 1; i < nv; i++)
        {
            const ULONG *ck = (const ULONG *)&vb[i].v.color;

            if (ck[0] != c0[0] || ck[1] != c0[1] || ck[2] != c0[2] || ck[3] != c0[3])
            {
                d_flatvar = 1;
                break;
            }
        }

        // two-side passes carry their own colors, flat stays per-vertex
        if (!d_flatvar && context->Lighting_State && context->ModelTwoSide)
            d_flatvar = 1;
    }

    if (context->ShadeModel == GL_FLAT && context->UpdateCurrentColor)
    {
        context->UpdateCurrentColor = GL_FALSE;

        W3D_SetCurrentColor(context->w3dContext, &context->CurrentColor);
    }

    if (context->Fog_State)
    {
        const GLfloat *p = CurrentP->v;

        // w-space fog bounds go stale when the projection terms move
        if (context->FogDirty || p[OF_43] != context->FogP43 || p[OF_44] != context->FogP44)
        {
            context->FogP43 = p[OF_43];
            context->FogP44 = p[OF_44];
            context->FogDirty = GL_FALSE;

            fog_Set(context);
        }
    }

    // culling front and back drops the whole draw
    if (!(context->CullFace_State && context->CurrentCullFace == GL_FRONT_AND_BACK))
    {
        const int lk = dx_lock(context);

        if (lk >= 0)
        {
            const GLint bind0 = context->CurrentBinding;
            const int istris = mode == GL_TRIANGLES || mode == GL_TRIANGLE_STRIP || mode == GL_TRIANGLE_FAN || mode == GL_QUADS || mode == GL_QUAD_STRIP || mode == GL_POLYGON;
            const int ltwo = context->Lighting_State && context->ModelTwoSide && istris;
            const int lpre = ltwo && context->CullFace_State && context->CurrentCullFace == GL_FRONT;
            GLuint k;
            GLuint ran = 0;

            // culling to back faces draws them with their own material
            if (ltwo || lspec)
            {
                for (i = 0; i < nv; i++)
                    context->LitSave[i] = vb[i].v.color;
            }

            if (lpre)
            {
                for (i = 0; i < nv; i++)
                    vb[i].v.color = context->LitBack[i];
            }

            d_emit(context, in, n, orall, final);

            for (k = 1; k <= context->TCopyMax; k++)
            {
                const GLint vbname = context->VirtualBinding[k];
                W3D_Texture *vt = context->w3dTexBuffer[vbname];
                const int armed = context->mtexArmed && k == 1;
                const int offw = (int)offsetof(MGLVertex, v.w) - ((int)offsetof(MGLVertex, tcoord) + (int)(k - 1) * (int)sizeof(struct TCoord_t));
                ULONG penv, psrc, pdst;
                ULONG oenv;

                if (!vt || (!armed && !context->Texture2D_State[k]))
                    continue;

                if (armed)
                {
                    penv = context->mtexEnv;
                    psrc = context->mtexSrc;
                    pdst = context->mtexDst;
                }
                else
                    mtex_unitspec(context->TexEnv[k], &penv, &psrc, &pdst);

                context->CurrentBinding = vbname;

                W3D_TexCoordPointer(context->w3dContext, &vb[0].tcoord[k - 1], sizeof(MGLVertex), 0, (int)sizeof(W3D_Float), offw, W3D_TEXCOORD_NORMALIZED);
                W3D_BindTexture(context->w3dContext, 0, vt);
                W3D_SetTexEnv(context->w3dContext, vt, penv, penv == W3D_BLEND ? &context->TexEnvColor[k] : NULL);
                W3D_SetBlendMode(context->w3dContext, psrc, pdst);
                W3D_SetState(context->w3dContext, W3D_TEXMAPPING, W3D_ENABLE);
                W3D_SetState(context->w3dContext, W3D_BLENDING, W3D_ENABLE);
                W3D_SetZCompareMode(context->w3dContext, W3D_Z_LEQUAL);

                d_emit(context, in, n, orall, final);

                ran = 1;

                oenv = mgl_envw(context->TexEnv[0]);

                W3D_SetTexEnv(context->w3dContext, vt, oenv, oenv == W3D_BLEND ? &context->TexEnvColor[0] : NULL);
            }

            context->mtexArmed = GL_FALSE;

            if (ran)
            {
                context->CurrentBinding = bind0;

                W3D_SetZCompareMode(context->w3dContext, mgl_depthw(context->CurDepthFunc));
                W3D_SetState(context->w3dContext, W3D_TEXMAPPING, (context->Texture2D_State[0] && context->w3dTexBuffer[bind0]) ? W3D_ENABLE : W3D_DISABLE);
                W3D_SetState(context->w3dContext, W3D_BLENDING, context->Blend_State ? W3D_ENABLE : W3D_DISABLE);
                W3D_SetBlendMode(context->w3dContext, mgl_blend_src(context->CurBlendSrc), mgl_blend_dst(context->CurBlendDst));
                W3D_BindTexture(context->w3dContext, 0, context->w3dTexBuffer[bind0]);
                W3D_TexCoordPointer(context->w3dContext, &vb[0].v.u, sizeof(MGLVertex), 0, 4, -4, W3D_TEXCOORD_NORMALIZED);
            }

            if (ltwo || lspec)
            {
                if (!(context->DepthTest_State && context->CurDepthFunc == GL_NEVER))
                {
                    const int drawF = !ltwo || !context->CullFace_State || context->CurrentCullFace == GL_BACK;
                    const int drawB = ltwo && (!context->CullFace_State || context->CurrentCullFace == GL_FRONT);

                    if (ltwo && !context->CullFace_State)
                        d_lpass(context, in, n, orall, final, nv, context->LitBack, GL_FRONT, 0);

                    if (drawF && (lspec & 1))
                        d_lpass(context, in, n, orall, final, nv, context->LitSpec, ltwo ? GL_BACK : (context->CullFace_State ? context->CurrentCullFace : 0), 1);

                    if (drawB && (lspec & 2))
                        d_lpass(context, in, n, orall, final, nv, context->LitSpecB, GL_FRONT, 1);
                }

                for (i = 0; i < nv; i++)
                    vb[i].v.color = context->LitSave[i];
            }
        }

        dx_unlock(context, lk);
    }

    if (final || in)
    {
        // indexed submits own the window across chunks, nothing carries here
        if (!in)
        {
            context->VertexBufferPointer = 0;
            context->EmitBase = 0;
            context->StripBase = 0;
        }

        return;
    }

    // overflow flush keeps the verts a chained prim still needs, lit colors ride along or the extra passes read another vert's
    keep = 0;

    switch (mode)
    {
    case GL_TRIANGLES:
        keep = n % 3;

        for (i = 0; i < keep; i++)
            vb[i] = vb[n - keep + i];

        break;

    case GL_QUADS:
        keep = n % 4;

        for (i = 0; i < keep; i++)
            vb[i] = vb[n - keep + i];

        break;

    case GL_QUAD_STRIP:
        keep = n >= 4 ? n - 2 * ((n - 2) / 2) : n;

        for (i = 0; i < keep; i++)
            vb[i] = vb[n - keep + i];

        break;

    case GL_TRIANGLE_STRIP:
        if (n > 2)
            context->StripBase += n - 2;

        keep = n > 2 ? 2 : n;

        for (i = 0; i < keep; i++)
            vb[i] = vb[n - keep + i];

        break;

    case GL_TRIANGLE_FAN:
    case GL_POLYGON:
    case GL_LINE_LOOP:
        keep = n > 1 ? 2 : n;

        if (n > 1)
            vb[1] = vb[n - 1];

        break;

    case GL_LINE_STRIP:
        keep = 1;
        vb[0] = vb[n - 1];
        break;

    case GL_LINES:
        keep = n & 1;

        if (keep)
            vb[0] = vb[n - 1];

        break;
    }

    if (keep && context->Lighting_State)
    {
        switch (mode)
        {
        case GL_TRIANGLE_FAN:
        case GL_POLYGON:
        case GL_LINE_LOOP:
            if (n > 1)
            {
                context->LitSpec[1] = context->LitSpec[n - 1];
                context->LitBack[1] = context->LitBack[n - 1];
                context->LitSpecB[1] = context->LitSpecB[n - 1];
            }

            break;

        case GL_LINE_STRIP:
            context->LitSpec[0] = context->LitSpec[n - 1];
            context->LitBack[0] = context->LitBack[n - 1];
            context->LitSpecB[0] = context->LitSpecB[n - 1];
            break;

        case GL_LINES:
            if (keep)
            {
                context->LitSpec[0] = context->LitSpec[n - 1];
                context->LitBack[0] = context->LitBack[n - 1];
                context->LitSpecB[0] = context->LitSpecB[n - 1];
            }

            break;

        default:
            for (i = 0; i < keep; i++)
            {
                context->LitSpec[i] = context->LitSpec[n - keep + i];
                context->LitBack[i] = context->LitBack[n - keep + i];
                context->LitSpecB[i] = context->LitSpecB[n - keep + i];
            }

            break;
        }
    }

    for (i = 0; i < keep; i++)
    {
        // an array normal rides its old slot while the buffer still has room
        if (vb[i].normal >= context->VertexBufferSize && context->NormalBufferPointer + 1 < context->VertexBufferSize * 2)
        {
            context->NormalBuffer[++context->NormalBufferPointer] = context->NormalBuffer[vb[i].normal];
            vb[i].normal = context->NormalBufferPointer;
        }
    }

    context->VertexBufferPointer = keep;
    context->EmitBase = keep;
}

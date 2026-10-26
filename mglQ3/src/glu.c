#include <mgl/gl.h>
#include "mglint.h"
#include <string.h>
#include <math.h>

#define GLU_HALF_RAD 0.0087266463f

static void glu_cross(float *r, const float *a, const float *b)
{
    r[0] = a[1] * b[2] - a[2] * b[1];
    r[1] = a[2] * b[0] - a[0] * b[2];
    r[2] = a[0] * b[1] - a[1] * b[0];
}

static void glu_norm(float *v)
{
    float m = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    if (m == 0.0f)
        return;

    m = 1.0f / m;

    v[0] *= m;
    v[1] *= m;
    v[2] *= m;
}

void GLULookAt(GLfloat ex, GLfloat ey, GLfloat ez, GLfloat cx, GLfloat cy, GLfloat cz, GLfloat ux, GLfloat uy, GLfloat uz)
{
    GLfloat f[3];
    GLfloat u[3];
    GLfloat s[3];
    GLfloat m[16];

    f[0] = cx - ex;
    f[1] = cy - ey;
    f[2] = cz - ez;
    u[0] = ux;
    u[1] = uy;
    u[2] = uz;

    glu_norm(f);
    glu_norm(u);

    glu_cross(s, f, u);
    glu_norm(s);

    glu_cross(u, s, f);

    memset(m, 0, sizeof m);

    m[0] = s[0];
    m[4] = s[1];
    m[8] = s[2];
    m[1] = u[0];
    m[5] = u[1];
    m[9] = u[2];
    m[2] = -f[0];
    m[6] = -f[1];
    m[10] = -f[2];
    m[15] = 1.0f;

    GLMultMatrixf(mini_CurrentContext, m);
    GLTranslatef(mini_CurrentContext, -ex, -ey, -ez);
}

void GLUPerspective(GLfloat fovy, GLfloat aspect, GLfloat znear, GLfloat zfar)
{
    const float r = fovy * GLU_HALF_RAD;
    const float ymax = znear * sinf(r) / cosf(r);
    const float xmax = ymax * aspect;

    GLFrustum(mini_CurrentContext, -xmax, xmax, -ymax, ymax, znear, zfar);
}

static void glu_scale(const UBYTE *s, GLint sw, GLint sh, UBYTE *d, GLint dw, GLint dh)
{
    GLint x;
    GLint y;
    GLint c;

    UBYTE *o = d;

    for (y = 0; y < dh; y++)
    {
        const float fy = ((float)y + 0.5f) * (float)sh / (float)dh - 0.5f;
        const GLint y0 = (GLint)floorf(fy);
        const GLint y1 = y0 + 1 < sh ? y0 + 1 : sh - 1;
        const GLint cy0 = y0 < 0 ? 0 : y0;
        const float ty = fy - (float)y0;
        const UBYTE *r0 = s + (size_t)cy0 * sw * 4;
        const UBYTE *r1 = s + (size_t)y1 * sw * 4;

        for (x = 0; x < dw; x++, o += 4)
        {
            const float fx = ((float)x + 0.5f) * (float)sw / (float)dw - 0.5f;
            const GLint x0 = (GLint)floorf(fx);
            const GLint x1 = x0 + 1 < sw ? x0 + 1 : sw - 1;
            const GLint cx0 = x0 < 0 ? 0 : x0;
            const float tx = fx - (float)x0;
            const UBYTE *p00 = r0 + cx0 * 4;
            const UBYTE *p01 = r0 + x1 * 4;
            const UBYTE *p10 = r1 + cx0 * 4;
            const UBYTE *p11 = r1 + x1 * 4;

            for (c = 0; c < 4; c++)
            {
                const float top = (float)p00[c] + ((float)p01[c] - (float)p00[c]) * tx;
                const float bot = (float)p10[c] + ((float)p11[c] - (float)p10[c]) * tx;

                o[c] = (UBYTE)(top + (bot - top) * ty + 0.5f);
            }
        }
    }
}

static void glu_half(const UBYTE *s, GLint sw, GLint sh, UBYTE *d)
{
    const GLint dw = sw > 1 ? sw / 2 : 1;
    const GLint dh = sh > 1 ? sh / 2 : 1;
    GLint x;
    GLint y;
    GLint c;

    UBYTE *o = d;

    for (y = 0; y < dh; y++)
    {
        const GLint y0 = y * 2;
        const GLint y1 = y0 + 1 < sh ? y0 + 1 : y0;
        const UBYTE *r0 = s + (size_t)y0 * sw * 4;
        const UBYTE *r1 = s + (size_t)y1 * sw * 4;
        const UBYTE *px0 = r0;
        const UBYTE *px1 = r1;

        for (x = 0; x < dw; x++, o += 4, px0 += 8, px1 += 8)
        {
            const UBYTE *p00 = px0;
            const UBYTE *p01 = x * 2 + 1 < sw ? px0 + 4 : px0;
            const UBYTE *p10 = px1;
            const UBYTE *p11 = x * 2 + 1 < sw ? px1 + 4 : px1;

            for (c = 0; c < 4; c++)
                o[c] = (UBYTE)(((int)p00[c] + p01[c] + p10[c] + p11[c] + 2) >> 2);
        }
    }
}

GLint GLUBuild2DMipmaps(GLcontext context, GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *data)
{
    GLint save[5];
    UBYTE *pool;
    UBYTE *cur;
    UBYTE *next;
    GLenum e;
    GLint level;
    GLint w;
    GLint h;
    GLint ret = 0;
    int sid = 0;
    int sbpp = 0;

    if (!context)
        return GLU_INVALID_OPERATION;

    if (target != GL_TEXTURE_2D)
        return GLU_INVALID_ENUM;

    if (!data || width < 1 || height < 1)
        return GLU_INVALID_VALUE;

    e = tex_srcfmt(format, type, &sid, &sbpp);

    if (e != GL_NO_ERROR)
        return GLU_INVALID_ENUM;

    if (sid == SRC_IDX8)
        return GLU_INVALID_OPERATION; // index texels have no channels to average

    if (width > (1 << 14) || height > (1 << 14))
        return GLU_OUT_OF_MEMORY;

    w = 1;

    while (w < width)
        w <<= 1;

    h = 1;

    while (h < height)
        h <<= 1;

    if (width - (w >> 1) < w - width)
        w >>= 1;

    if (height - (h >> 1) < h - height)
        h >>= 1;

    pool = (UBYTE *)malloc((size_t)w * (size_t)h * 8);

    if (!pool)
        return GLU_OUT_OF_MEMORY;

    cur = pool;
    next = pool + (size_t)w * (size_t)h * 4;

    tex_decode_pixels(context, data, sid, sbpp, width, height, cur);

    if (w != width || h != height)
    {
        glu_scale(cur, width, height, next, w, h);

        UBYTE *t = cur;
        cur = next;
        next = t;
    }

    tex_unpk0(context, save);

    const GLboolean nomip = context->NoMipMapping;
    const GLenum err0 = context->CurrentError;

    context->NoMipMapping = GL_FALSE;

    context->CurrentError = GL_NO_ERROR;

    for (level = 0;; level++)
    {
        UBYTE *t;

        GLTexImage2D(context, target, level, internalFormat, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, cur);

        if (context->CurrentError != GL_NO_ERROR)
        {
            ret = GLU_INVALID_OPERATION;
            break;
        }

        if (w == 1 && h == 1)
            break;

        glu_half(cur, w, h, next);

        t = cur;
        cur = next;
        next = t;

        if (w > 1)
            w >>= 1;

        if (h > 1)
            h >>= 1;
    }

    context->NoMipMapping = nomip;

    if (context->CurrentError == GL_NO_ERROR)
        context->CurrentError = err0;

    tex_unpk1(context, save);

    free(pool);

    return ret;
}

GLint GLUScaleImage(GLcontext context, GLenum format, GLint win, GLint hin, GLenum typein, const GLvoid *datain, GLint wout, GLint hout, GLenum typeout, GLvoid *dataout)
{
    const UBYTE *s = (const UBYTE *)datain;
    UBYTE *d = (UBYTE *)dataout;
    UBYTE *pool;
    UBYTE *in;
    UBYTE *out;
    int isid;
    int ibpp;
    int osid;
    int obpp;
    int x;
    int y;

    if (!context)
        return GLU_INVALID_OPERATION;

    if (!datain || !dataout)
        return GLU_INVALID_OPERATION;

    if (win < 0 || hin < 0 || wout < 0 || hout < 0)
        return GLU_INVALID_VALUE;

    if (!win || !hin || !wout || !hout)
        return 0;

    if (tex_srcfmt(format, typein, &isid, &ibpp) != GL_NO_ERROR)
        return GLU_INVALID_ENUM;

    if (tex_srcfmt(format, typeout, &osid, &obpp) != GL_NO_ERROR)
        return GLU_INVALID_ENUM;

    if (win == wout && hin == hout && typein == typeout)
    {
        memcpy(d, s, (size_t)wout * (size_t)hout * (size_t)obpp);
        return 0;
    }

    if (isid == SRC_IDX8)
    {
        for (y = 0; y < hout; y++)
        {
            const GLint sy0 = y * hin / hout;
            GLint sy1 = (y + 1) * hin / hout;
            UBYTE *dr = d + (size_t)y * wout;

            if (sy1 == sy0)
                sy1 = sy0 + 1;

            for (x = 0; x < wout; x++)
            {
                const GLint sx0 = x * win / wout;
                GLint sx1 = (x + 1) * win / wout;
                GLint sy;
                GLint sx;
                GLuint sum = 0;

                if (sx1 == sx0)
                    sx1 = sx0 + 1;

                for (sy = sy0; sy < sy1; sy++)
                {
                    const UBYTE *sr = s + (size_t)sy * win + sx0;

                    for (sx = sx0; sx < sx1; sx++, sr++)
                        sum += *sr;
                }

                dr[x] = (UBYTE)(sum / ((GLuint)(sy1 - sy0) * (GLuint)(sx1 - sx0)));
            }
        }

        return 0;
    }

    if ((ULONG)win * (ULONG)hin > 0x3FFFFFFFUL || (ULONG)wout * (ULONG)hout > 0x3FFFFFFFUL)
        return GLU_OUT_OF_MEMORY;

    pool = malloc((size_t)win * (size_t)hin * 4 + (size_t)wout * (size_t)hout * 4);

    if (!pool)
        return GLU_OUT_OF_MEMORY;

    in = pool;
    out = pool + (size_t)win * (size_t)hin * 4;

    for (y = 0; y < hin; y++)
    {
        const UBYTE *sr = s + (size_t)y * win * ibpp;
        UBYTE *ir = in + (size_t)y * win * 4;

        for (x = 0; x < win; x++, sr += ibpp, ir += 4)
            tex_decode(sr, isid, 0, ir);
    }

    glu_scale(in, win, hin, out, wout, hout);

    for (y = 0; y < hout; y++)
    {
        const UBYTE *ir = out + (size_t)y * wout * 4;
        UBYTE *dr = d + (size_t)y * wout * obpp;

        for (x = 0; x < wout; x++, ir += 4, dr += obpp)
            tex_sencode(dr, osid, 0, ir);
    }

    free(pool);

    return 0;
}

static void glu_matvec(const GLdouble *m, GLdouble *v)
{
    const GLdouble x = m[0] * v[0] + m[4] * v[1] + m[8] * v[2] + m[12] * v[3];
    const GLdouble y = m[1] * v[0] + m[5] * v[1] + m[9] * v[2] + m[13] * v[3];
    const GLdouble z = m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14] * v[3];
    const GLdouble w = m[3] * v[0] + m[7] * v[1] + m[11] * v[2] + m[15] * v[3];

    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = w;
}

static void glu_matmul(const GLdouble *a, const GLdouble *b, GLdouble *c)
{
    GLdouble r[16];
    int i;
    int j;
    int k;

    for (j = 0; j < 4; j++)
    {
        for (i = 0; i < 4; i++)
        {
            GLdouble s = 0.0;

            for (k = 0; k < 4; k++)
                s += a[k * 4 + i] * b[j * 4 + k];

            r[j * 4 + i] = s;
        }
    }

    memcpy(c, r, sizeof r);
}

static int glu_invert4(const GLdouble *m, GLdouble *inv)
{
    GLdouble det;
    int i;

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0.0)
        return 0;

    det = 1.0 / det;

    for (i = 0; i < 16; i++)
        inv[i] *= det;

    return 1;
}

void GLUOrtho2D(GLcontext context, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top)
{
    if (!context)
        return;

    GLOrtho(context, left, right, bottom, top, -1.0, 1.0);
}

GLint GLUProject(GLcontext context, GLdouble objX, GLdouble objY, GLdouble objZ, const GLdouble *model, const GLdouble *proj, const GLint *viewport, GLdouble *winX, GLdouble *winY, GLdouble *winZ)
{
    GLdouble v[4];

    if (!model || !proj || !viewport || !winX || !winY || !winZ)
        return GL_FALSE;

    v[0] = objX;
    v[1] = objY;
    v[2] = objZ;
    v[3] = 1.0;

    glu_matvec(model, v);
    glu_matvec(proj, v);

    if (v[3] == 0.0)
        return GL_FALSE;

    v[3] = 1.0 / v[3];

    *winX = viewport[0] + (1.0 + v[0] * v[3]) * viewport[2] / 2.0;
    *winY = viewport[1] + (1.0 + v[1] * v[3]) * viewport[3] / 2.0;
    *winZ = (1.0 + v[2] * v[3]) / 2.0;

    return GL_TRUE;
}

GLint GLUUnProject(GLcontext context, GLdouble winX, GLdouble winY, GLdouble winZ, const GLdouble *model, const GLdouble *proj, const GLint *viewport, GLdouble *objX, GLdouble *objY, GLdouble *objZ)
{
    GLdouble m[16];
    GLdouble inv[16];
    GLdouble v[4];

    if (!model || !proj || !viewport || !objX || !objY || !objZ)
        return GL_FALSE;

    if (!viewport[2] || !viewport[3])
        return GL_FALSE;

    glu_matmul(proj, model, m);

    if (!glu_invert4(m, inv))
        return GL_FALSE;

    v[0] = (winX - (GLdouble)viewport[0]) * 2.0 / viewport[2] - 1.0;
    v[1] = (winY - (GLdouble)viewport[1]) * 2.0 / viewport[3] - 1.0;
    v[2] = winZ * 2.0 - 1.0;
    v[3] = 1.0;

    glu_matvec(inv, v);

    if (v[3] == 0.0)
        return GL_FALSE;

    *objX = v[0] / v[3];
    *objY = v[1] / v[3];
    *objZ = v[2] / v[3];

    return GL_TRUE;
}

void GLUPickMatrix(GLcontext context, GLdouble x, GLdouble y, GLdouble width, GLdouble height, const GLint *viewport)
{
    if (!context || !viewport)
        return;

    if (width <= 0.0 || height <= 0.0)
        return;

    GLTranslatef(context, (GLfloat)((viewport[2] - 2.0 * (x - viewport[0])) / width), (GLfloat)((viewport[3] - 2.0 * (y - viewport[1])) / height), 0.0f);
    GLScalef(context, (GLfloat)(viewport[2] / width), (GLfloat)(viewport[3] / height), 1.0f);
}

const GLubyte *GLUGetString(GLenum name)
{
    if (name == GLU_VERSION)
        return (const GLubyte *)"1.3";

    if (name == GLU_EXTENSIONS)
        return (const GLubyte *)"";

    return NULL;
}

#include "mglint.h"
#include <math.h>

#define M_DEG (float)(3.14159265358979323846 / 180.0)

static void m_Identity(Matrix *m)
{
    int i;

    for (i = 0; i < 16; i++)
        m->v[i] = 0.0f;

    m->v[OF_11] = 1.0f;
    m->v[OF_22] = 1.0f;
    m->v[OF_33] = 1.0f;
    m->v[OF_44] = 1.0f;

    m->flags = MGLMAT_IDENTITY;
}

static int m_Classify(const GLfloat *v)
{
    if (v[3] != 0.0f || v[7] != 0.0f || v[11] != 0.0f || v[15] != 1.0f)
        return MGLMAT_GENERAL;

    if (v[1] != 0.0f || v[2] != 0.0f || v[4] != 0.0f || v[6] != 0.0f || v[8] != 0.0f || v[9] != 0.0f || v[12] != 0.0f || v[13] != 0.0f || v[14] != 0.0f || v[0] != 1.0f || v[5] != 1.0f || v[10] != 1.0f)
        return MGLMAT_ORTHO;

    return MGLMAT_IDENTITY;
}

static void m_Load(Matrix *m, const GLfloat *v)
{
    memcpy(m->v, v, sizeof(m->v));

    m->flags = m_Classify(v);
}

static void m_Mul(GLfloat *d, const GLfloat *a, const GLfloat *b)
{
    int c;

    for (c = 0; c < 4; c++)
    {
        const GLfloat *bcol = b + c * 4;
        GLfloat *dcol = d + c * 4;

        dcol[0] = a[0] * bcol[0] + a[4] * bcol[1] + a[8] * bcol[2] + a[12] * bcol[3];
        dcol[1] = a[1] * bcol[0] + a[5] * bcol[1] + a[9] * bcol[2] + a[13] * bcol[3];
        dcol[2] = a[2] * bcol[0] + a[6] * bcol[1] + a[10] * bcol[2] + a[14] * bcol[3];
        dcol[3] = a[3] * bcol[0] + a[7] * bcol[1] + a[11] * bcol[2] + a[15] * bcol[3];
    }
}

static void m_Invalidate(GLcontext context)
{
    context->CombinedValid = GL_FALSE;

    if (context->CurrentMatrixMode == GL_MODELVIEW)
        context->InvRotValid = GL_FALSE;
}

static void m_Finish(GLcontext context, Matrix *dst)
{
    dst->flags = m_Classify(dst->v);

    SMATRIX(context);

    m_Invalidate(context);
}

static void m_Apply(GLcontext context, const GLfloat *v)
{
    Matrix *dst = OMATRIX(context);
    Matrix *cur = CMATRIX(context);

    m_Mul(dst->v, cur->v, v);

    m_Finish(context, dst);
}

static void m_ApplyRot(GLcontext context, const GLfloat *v)
{
    Matrix *dst = OMATRIX(context);
    Matrix *cur = CMATRIX(context);
    const GLfloat *a = cur->v;

    dst->v[0] = a[0] * v[0] + a[4] * v[1] + a[8] * v[2];
    dst->v[1] = a[1] * v[0] + a[5] * v[1] + a[9] * v[2];
    dst->v[2] = a[2] * v[0] + a[6] * v[1] + a[10] * v[2];
    dst->v[3] = a[3] * v[0] + a[7] * v[1] + a[11] * v[2];
    dst->v[4] = a[0] * v[4] + a[4] * v[5] + a[8] * v[6];
    dst->v[5] = a[1] * v[4] + a[5] * v[5] + a[9] * v[6];
    dst->v[6] = a[2] * v[4] + a[6] * v[5] + a[10] * v[6];
    dst->v[7] = a[3] * v[4] + a[7] * v[5] + a[11] * v[6];
    dst->v[8] = a[0] * v[8] + a[4] * v[9] + a[8] * v[10];
    dst->v[9] = a[1] * v[8] + a[5] * v[9] + a[9] * v[10];
    dst->v[10] = a[2] * v[8] + a[6] * v[9] + a[10] * v[10];
    dst->v[11] = a[3] * v[8] + a[7] * v[9] + a[11] * v[10];
    dst->v[12] = a[12];
    dst->v[13] = a[13];
    dst->v[14] = a[14];
    dst->v[15] = a[15];

    m_Finish(context, dst);
}

static void m_ApplyTrans(GLcontext context, GLfloat x, GLfloat y, GLfloat z)
{
    Matrix *dst = OMATRIX(context);
    Matrix *cur = CMATRIX(context);
    const GLfloat *a = cur->v;

    memcpy(dst->v, a, 12 * sizeof(GLfloat));

    dst->v[12] = a[0] * x + a[4] * y + a[8] * z + a[12];
    dst->v[13] = a[1] * x + a[5] * y + a[9] * z + a[13];
    dst->v[14] = a[2] * x + a[6] * y + a[10] * z + a[14];
    dst->v[15] = a[3] * x + a[7] * y + a[11] * z + a[15];

    m_Finish(context, dst);
}

static void m_ApplyScale(GLcontext context, GLfloat x, GLfloat y, GLfloat z)
{
    Matrix *dst = OMATRIX(context);
    Matrix *cur = CMATRIX(context);
    const GLfloat *a = cur->v;

    dst->v[0] = a[0] * x;
    dst->v[1] = a[1] * x;
    dst->v[2] = a[2] * x;
    dst->v[3] = a[3] * x;
    dst->v[4] = a[4] * y;
    dst->v[5] = a[5] * y;
    dst->v[6] = a[6] * y;
    dst->v[7] = a[7] * y;
    dst->v[8] = a[8] * z;
    dst->v[9] = a[9] * z;
    dst->v[10] = a[10] * z;
    dst->v[11] = a[11] * z;
    dst->v[12] = a[12];
    dst->v[13] = a[13];
    dst->v[14] = a[14];
    dst->v[15] = a[15];

    m_Finish(context, dst);
}

static void m_RotateAxis(GLcontext context, GLfloat s, GLfloat c, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat v[16];
    GLfloat t;
    GLfloat l;

    l = sqrtf(x * x + y * y + z * z);

    if (l == 0.0f)
        return;

    l = 1.0f / l;
    x *= l;
    y *= l;
    z *= l;

    t = 1.0f - c;

    v[0] = x * x * t + c;
    v[1] = x * y * t + z * s;
    v[2] = x * z * t - y * s;
    v[4] = y * x * t - z * s;
    v[5] = y * y * t + c;
    v[6] = y * z * t + x * s;
    v[8] = z * x * t + y * s;
    v[9] = z * y * t - x * s;
    v[10] = z * z * t + c;

    m_ApplyRot(context, v);
}

static float m_DegReduce(float angle)
{
    // mathieeesingtrans sinf/cosf return garbage past ~2*PI
    if (angle >= 360.0f || angle <= -360.0f)
        angle -= 360.0f * floorf(angle * (1.0f / 360.0f));

    return angle;
}

void mglUpdateCombined(GLcontext context)
{
    if (context->CombinedValid)
        return;

    m_Mul(context->CombinedMatrix.v, context->Projection[context->ProjectionNr].v, context->ModelView[context->ModelViewNr].v);

    context->CombinedMatrix.flags = m_Classify(context->CombinedMatrix.v);
    context->CombinedValid = GL_TRUE;
}

void mglUpdateInvRot(GLcontext context)
{
    const GLfloat *m = context->ModelView[context->ModelViewNr].v;
    GLfloat *r = context->InvRot;
    GLfloat det;
    int i;

    if (context->InvRotValid)
        return;

    r[0] = m[5] * m[10] - m[9] * m[6];
    r[1] = m[8] * m[6] - m[4] * m[10];
    r[2] = m[4] * m[9] - m[8] * m[5];
    r[3] = m[9] * m[2] - m[1] * m[10];
    r[4] = m[0] * m[10] - m[8] * m[2];
    r[5] = m[8] * m[1] - m[0] * m[9];
    r[6] = m[1] * m[6] - m[5] * m[2];
    r[7] = m[4] * m[2] - m[0] * m[6];
    r[8] = m[0] * m[5] - m[4] * m[1];

    det = m[0] * r[0] + m[1] * r[1] + m[2] * r[2];

    if (det == 0.0f)
    {
        memset(r, 0, 9 * sizeof(GLfloat));

        r[0] = r[4] = r[8] = 1.0f;
    }
    else
    {
        const GLfloat idet = 1.0f / det;

        for (i = 0; i < 9; i++)
            r[i] *= idet;
    }

    context->InvRotValid = GL_TRUE;
}

void GLMatrixMode(GLcontext context, GLenum mode)
{
    switch (mode)
    {
    case GL_MODELVIEW:
    case GL_PROJECTION:
        context->CurrentMatrixMode = mode;
        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

void GLLoadIdentity(GLcontext context)
{
    if (context->CurrentMatrixMode == GL_MODELVIEW)
        m_Identity(CurrentMV);
    else
        m_Identity(CurrentP);

    m_Invalidate(context);
}

void GLMatrixInit(GLcontext context)
{
    context->ModelViewStackPointer = 0;
    context->ProjectionStackPointer = 0;
    context->ModelViewNr = 0;
    context->ProjectionNr = 0;
    context->CombinedValid = GL_FALSE;
    context->InvRotValid = GL_FALSE;

    context->CurrentMatrixMode = GL_PROJECTION;

    GLLoadIdentity(context);

    context->CurrentMatrixMode = GL_MODELVIEW;

    GLLoadIdentity(context);
}

void GLLoadMatrixd(GLcontext context, const GLdouble *m)
{
    GLfloat v[16];
    int i;

    for (i = 0; i < 16; i++)
        v[i] = (GLfloat)m[i];

    m_Load(CMATRIX(context), v);

    m_Invalidate(context);
}

void GLLoadMatrixf(GLcontext context, const GLfloat *m)
{
    m_Load(CMATRIX(context), m);

    m_Invalidate(context);
}

void GLMultMatrixd(GLcontext context, const GLdouble *m)
{
    GLfloat v[16];
    int i;

    for (i = 0; i < 16; i++)
        v[i] = (GLfloat)m[i];

    m_Apply(context, v);
}

void GLMultMatrixf(GLcontext context, const GLfloat *m)
{
    m_Apply(context, m);
}

void GLPushMatrix(GLcontext context)
{
    if (context->CurrentMatrixMode == GL_MODELVIEW)
    {
        if (context->ModelViewStackPointer < MODELVIEW_STACK_SIZE)
            context->ModelViewStack[context->ModelViewStackPointer++] = *(CMATRIX(context));
        else
            mgl_error(context, GL_STACK_OVERFLOW);
    }
    else
    {
        if (context->ProjectionStackPointer < PROJECTION_STACK_SIZE)
            context->ProjectionStack[context->ProjectionStackPointer++] = *(CMATRIX(context));
        else
            mgl_error(context, GL_STACK_OVERFLOW);
    }
}

void GLPopMatrix(GLcontext context)
{
    if (context->CurrentMatrixMode == GL_MODELVIEW)
    {
        if (context->ModelViewStackPointer > 0)
        {
            *(CMATRIX(context)) = context->ModelViewStack[--context->ModelViewStackPointer];

            context->CombinedValid = GL_FALSE;
            context->InvRotValid = GL_FALSE;
        }
        else
            mgl_error(context, GL_STACK_UNDERFLOW);
    }
    else
    {
        if (context->ProjectionStackPointer > 0)
        {
            *(CMATRIX(context)) = context->ProjectionStack[--context->ProjectionStackPointer];

            context->CombinedValid = GL_FALSE;
        }
        else
            mgl_error(context, GL_STACK_UNDERFLOW);
    }
}

void GLOrtho(GLcontext context, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    GLfloat v[16];
    GLfloat rl;
    GLfloat tb;
    GLfloat fn;

    rl = (GLfloat)(right - left);
    tb = (GLfloat)(top - bottom);
    fn = (GLfloat)(zFar - zNear);

    if (rl == 0.0f || tb == 0.0f || fn == 0.0f)
    {
        mgl_error(context, GL_INVALID_VALUE);

        return;
    }

    memset(v, 0, sizeof v);

    v[OF_11] = 2.0f / rl;
    v[OF_22] = 2.0f / tb;
    v[OF_33] = -2.0f / fn;
    v[OF_14] = (GLfloat)(-(right + left)) / rl;
    v[OF_24] = (GLfloat)(-(top + bottom)) / tb;
    v[OF_34] = (GLfloat)(-(zFar + zNear)) / fn;
    v[OF_44] = 1.0f;

    m_Apply(context, v);
}

void GLFrustum(GLcontext context, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    GLfloat v[16];

    if (left == right || bottom == top || zNear == zFar || zNear <= 0.0 || zFar <= 0.0)
    {
        mgl_error(context, GL_INVALID_VALUE);

        return;
    }

    memset(v, 0, sizeof v);

    v[OF_11] = (GLfloat)(2.0 * zNear / (right - left));
    v[OF_22] = (GLfloat)(2.0 * zNear / (top - bottom));
    v[OF_13] = (GLfloat)((right + left) / (right - left));
    v[OF_23] = (GLfloat)((top + bottom) / (top - bottom));
    v[OF_33] = (GLfloat)(-(zFar + zNear) / (zFar - zNear));
    v[OF_43] = -1.0f;
    v[OF_34] = (GLfloat)(-2.0 * zFar * zNear / (zFar - zNear));

    m_Apply(context, v);
}

void GLRotated(GLcontext context, GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
    GLRotatef(context, (GLfloat)angle, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void GLRotatef(GLcontext context, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat r;

    r = m_DegReduce(angle) * M_DEG;

    m_RotateAxis(context, sinf(r), cosf(r), x, y, z);
}

void GLRotatefEXT(GLcontext context, GLfloat angle, const GLint xyz)
{
    GLfloat r;

    r = m_DegReduce(angle) * M_DEG;

    m_RotateAxis(context, sinf(r), cosf(r), (xyz & GLROT_100) ? 1.0f : 0.0f, (xyz & GLROT_010) ? 1.0f : 0.0f, (xyz & GLROT_001) ? 1.0f : 0.0f);
}

void GLRotatefEXTs(GLcontext context, GLfloat sin_an, GLfloat cos_an, const GLint xyz)
{
    m_RotateAxis(context, sin_an, cos_an, (xyz & GLROT_100) ? 1.0f : 0.0f, (xyz & GLROT_010) ? 1.0f : 0.0f, (xyz & GLROT_001) ? 1.0f : 0.0f);
}

void GLScaled(GLcontext context, GLdouble x, GLdouble y, GLdouble z)
{
    GLScalef(context, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void GLScalef(GLcontext context, GLfloat x, GLfloat y, GLfloat z)
{
    m_ApplyScale(context, x, y, z);
}

void GLTranslated(GLcontext context, GLdouble x, GLdouble y, GLdouble z)
{
    GLTranslatef(context, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void GLTranslatef(GLcontext context, GLfloat x, GLfloat y, GLfloat z)
{
    m_ApplyTrans(context, x, y, z);
}

void MGLPrintMatrix(GLcontext context, int mode)
{
}

void MGLPrintMatrixStack(GLcontext context, int mode)
{
}

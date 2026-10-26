#include "mglint.h"
#include <math.h>

#define LT_D2R 0.017453292519943295769f
#define LT_R2D 57.2957795130823208768f

// color-material mode feeds the staged color into the tracked components
void lt_cmtrack(GLenum mode, MGLMaterial *m, const W3D_Color *c)
{
    if (mode == GL_AMBIENT_AND_DIFFUSE)
    {
        m->ambient = *c;
        m->diffuse = *c;
        return;
    }

    (&m->ambient)[mode == GL_EMISSION ? 3 : mode - GL_AMBIENT] = *c;
}

static void lt_color(W3D_Color *c, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    c->r = r;
    c->g = g;
    c->b = b;
    c->a = a;
}

static void lt_getc(const W3D_Color *c, GLfloat *p)
{
    p[0] = c->r;
    p[1] = c->g;
    p[2] = c->b;
    p[3] = c->a;
}

void light_SetDefaults(GLcontext context)
{
    int i;

    for (i = 0; i < MGL_MAX_LIGHTS; i++)
    {
        MGLLight *l = &context->Light[i];

        lt_color(&l->ambient, 0.0f, 0.0f, 0.0f, 1.0f);
        lt_color(&l->diffuse, i ? 0.0f : 1.0f, i ? 0.0f : 1.0f, i ? 0.0f : 1.0f, 1.0f);
        lt_color(&l->specular, i ? 0.0f : 1.0f, i ? 0.0f : 1.0f, i ? 0.0f : 1.0f, 1.0f);

        l->pos[0] = 0.0f;
        l->pos[1] = 0.0f;
        l->pos[2] = 1.0f;
        l->pos[3] = 0.0f;

        l->dir[0] = 0.0f;
        l->dir[1] = 0.0f;
        l->dir[2] = -1.0f;

        l->spotExp = 0.0f;
        l->spotCos = -1.0f;
        l->kc = 1.0f;
        l->kl = 0.0f;
        l->kq = 0.0f;
        l->att0 = 1.0f;
        l->flags = MGL_LF_DIR;
    }

    for (i = 0; i < 2; i++)
    {
        MGLMaterial *m = &context->Material[i];

        lt_color(&m->ambient, 0.2f, 0.2f, 0.2f, 1.0f);
        lt_color(&m->diffuse, 0.8f, 0.8f, 0.8f, 1.0f);
        lt_color(&m->specular, 0.0f, 0.0f, 0.0f, 1.0f);
        lt_color(&m->emission, 0.0f, 0.0f, 0.0f, 1.0f);

        m->shininess = 0.0f;
    }

    lt_color(&context->ModelAmbient, 0.2f, 0.2f, 0.2f, 1.0f);

    context->Lighting_State = GL_FALSE;
    context->LightMask = 0;
    context->ModelLocalViewer = GL_FALSE;
    context->ModelTwoSide = GL_FALSE;
    context->ModelSepSpec = GL_FALSE;
    context->Normalize_State = GL_FALSE;
    context->Rescale_State = GL_FALSE;
    context->ColorMaterial_State = GL_FALSE;
    context->ColorMaterialFace = GL_FRONT_AND_BACK;
    context->ColorMaterialMode = GL_AMBIENT_AND_DIFFUSE;
}

static int lt_lightidx(GLcontext context, GLenum light)
{
    if (light < GL_LIGHT0 || light > GL_LIGHT7)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return -1;
    }

    return (int)(light - GL_LIGHT0);
}

static int lt_faceidx(GLcontext context, GLenum face)
{
    switch (face)
    {
    case GL_FRONT:
    case GL_BACK:
        return face == GL_BACK;

    case GL_FRONT_AND_BACK:
        return 2;
    }

    mgl_error(context, GL_INVALID_ENUM);
    return -1;
}

static void lt_fixflags(MGLLight *l)
{
    l->flags = 0;

    if (l->pos[3] == 0.0f)
        l->flags |= MGL_LF_DIR;

    if (l->kl != 0.0f || l->kq != 0.0f)
        l->flags |= MGL_LF_ATTEN;
    else
        l->att0 = l->kc > 0.0f ? 1.0f / l->kc : 1.0f;

    if (l->spotCos != -1.0f)
        l->flags |= MGL_LF_SPOT;
}

void GLLightf(GLcontext context, GLenum light, GLenum pname, GLfloat param)
{
    const GLfloat p[4] = {param, 0.0f, 0.0f, 0.0f};

    GLLightfv(context, light, pname, p);
}

void GLLightfv(GLcontext context, GLenum light, GLenum pname, const GLfloat *params)
{
    const int idx = lt_lightidx(context, light);
    MGLLight *l;
    const float *m;
    float il;

    if (idx < 0)
        return;

    l = &context->Light[idx];

    switch (pname)
    {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
        lt_color(&(&l->ambient)[pname - GL_AMBIENT], params[0], params[1], params[2], params[3]);
        break;

    case GL_POSITION:
        m = CurrentMV->v;

        l->pos[0] = m[0] * params[0] + m[4] * params[1] + m[8] * params[2] + m[12] * params[3];
        l->pos[1] = m[1] * params[0] + m[5] * params[1] + m[9] * params[2] + m[13] * params[3];
        l->pos[2] = m[2] * params[0] + m[6] * params[1] + m[10] * params[2] + m[14] * params[3];
        l->pos[3] = m[3] * params[0] + m[7] * params[1] + m[11] * params[2] + m[15] * params[3];

        if (l->pos[3] == 0.0f)
        {
            il = l->pos[0] * l->pos[0] + l->pos[1] * l->pos[1] + l->pos[2] * l->pos[2];

            if (il > 0.0f)
            {
                il = 1.0f / sqrtf(il);

                l->pos[0] *= il;
                l->pos[1] *= il;
                l->pos[2] *= il;
            }
        }

        lt_fixflags(l);
        break;

    case GL_SPOT_DIRECTION:
        m = CurrentMV->v;

        l->dir[0] = m[0] * params[0] + m[4] * params[1] + m[8] * params[2];
        l->dir[1] = m[1] * params[0] + m[5] * params[1] + m[9] * params[2];
        l->dir[2] = m[2] * params[0] + m[6] * params[1] + m[10] * params[2];

        il = l->dir[0] * l->dir[0] + l->dir[1] * l->dir[1] + l->dir[2] * l->dir[2];

        if (il > 0.0f)
        {
            il = 1.0f / sqrtf(il);

            l->dir[0] *= il;
            l->dir[1] *= il;
            l->dir[2] *= il;
        }

        break;

    case GL_SPOT_EXPONENT:
        if (params[0] < 0.0f || params[0] > 128.0f)
        {
            mgl_error(context, GL_INVALID_VALUE);
            return;
        }

        l->spotExp = params[0];
        break;

    case GL_SPOT_CUTOFF:
        if ((params[0] < 0.0f || params[0] > 90.0f) && params[0] != 180.0f)
        {
            mgl_error(context, GL_INVALID_VALUE);
            return;
        }

        l->spotCos = params[0] == 180.0f ? -1.0f : cosf(params[0] * LT_D2R);

        lt_fixflags(l);
        break;

    case GL_CONSTANT_ATTENUATION:
    case GL_LINEAR_ATTENUATION:
    case GL_QUADRATIC_ATTENUATION:
        if (params[0] < 0.0f)
        {
            mgl_error(context, GL_INVALID_VALUE);
            return;
        }

        (&l->kc)[pname - GL_CONSTANT_ATTENUATION] = params[0];

        lt_fixflags(l);
        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

void GLMaterialf(GLcontext context, GLenum face, GLenum pname, GLfloat param)
{
    const GLfloat p[4] = {param, 0.0f, 0.0f, 0.0f};

    GLMaterialfv(context, face, pname, p);
}

void GLMaterialfv(GLcontext context, GLenum face, GLenum pname, const GLfloat *params)
{
    int fi = lt_faceidx(context, face);
    MGLMaterial *m;
    int i0;
    int i;

    if (fi < 0)
        return;

    m = &context->Material[0];
    i0 = fi == 2 ? 0 : fi;
    fi = fi == 2 ? 1 : fi;

    switch (pname)
    {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
        for (i = i0; i <= fi; i++)
            lt_color(&(&m[i].ambient)[pname == GL_EMISSION ? 3 : pname - GL_AMBIENT], params[0], params[1], params[2], params[3]);

        break;

    case GL_AMBIENT_AND_DIFFUSE:
        for (i = i0; i <= fi; i++)
        {
            lt_color(&m[i].ambient, params[0], params[1], params[2], params[3]);
            lt_color(&m[i].diffuse, params[0], params[1], params[2], params[3]);
        }

        break;

    case GL_SHININESS:
        if (params[0] < 0.0f || params[0] > 128.0f)
        {
            mgl_error(context, GL_INVALID_VALUE);
            return;
        }

        for (i = i0; i <= fi; i++)
            m[i].shininess = params[0];

        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

void GLLightModelf(GLcontext context, GLenum pname, GLfloat param)
{
    const GLfloat p[4] = {param, 0.0f, 0.0f, 0.0f};

    GLLightModelfv(context, pname, p);
}

void GLLightModelfv(GLcontext context, GLenum pname, const GLfloat *params)
{
    switch (pname)
    {
    case GL_LIGHT_MODEL_AMBIENT:
        lt_color(&context->ModelAmbient, params[0], params[1], params[2], params[3]);
        break;

    case GL_LIGHT_MODEL_LOCAL_VIEWER:
        context->ModelLocalViewer = params[0] != 0.0f;
        break;

    case GL_LIGHT_MODEL_TWO_SIDE:
        context->ModelTwoSide = params[0] != 0.0f;
        break;

    case GL_LIGHT_MODEL_COLOR_CONTROL:
        if (params[0] != (GLfloat)GL_SINGLE_COLOR && params[0] != (GLfloat)GL_SEPARATE_SPECULAR_COLOR)
        {
            mgl_error(context, GL_INVALID_ENUM);
            return;
        }

        context->ModelSepSpec = params[0] == (GLfloat)GL_SEPARATE_SPECULAR_COLOR;
        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

void GLColorMaterial(GLcontext context, GLenum face, GLenum mode)
{
    if (lt_faceidx(context, face) < 0)
        return;

    switch (mode)
    {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
    case GL_AMBIENT_AND_DIFFUSE:
        context->ColorMaterialMode = mode;
        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->ColorMaterialFace = face;
}

void GLGetLightfv(GLcontext context, GLenum light, GLenum pname, GLfloat *params)
{
    const int idx = lt_lightidx(context, light);
    const MGLLight *l;

    if (idx < 0)
        return;

    l = &context->Light[idx];

    switch (pname)
    {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
        lt_getc(&(&l->ambient)[pname - GL_AMBIENT], params);
        break;

    case GL_POSITION:
        params[0] = l->pos[0];
        params[1] = l->pos[1];
        params[2] = l->pos[2];
        params[3] = l->pos[3];
        break;

    case GL_SPOT_DIRECTION:
        params[0] = l->dir[0];
        params[1] = l->dir[1];
        params[2] = l->dir[2];
        break;

    case GL_SPOT_EXPONENT:
        params[0] = l->spotExp;
        break;

    case GL_SPOT_CUTOFF:
        params[0] = l->spotCos == -1.0f ? 180.0f : acosf(l->spotCos) * LT_R2D;
        break;

    case GL_CONSTANT_ATTENUATION:
    case GL_LINEAR_ATTENUATION:
    case GL_QUADRATIC_ATTENUATION:
        params[0] = (&l->kc)[pname - GL_CONSTANT_ATTENUATION];
        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

void GLGetMaterialfv(GLcontext context, GLenum face, GLenum pname, GLfloat *params)
{
    const int fi = lt_faceidx(context, face);
    const MGLMaterial *m;

    if (fi < 0)
        return;

    if (fi == 2)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    m = &context->Material[fi];

    switch (pname)
    {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
        lt_getc(&(&m->ambient)[pname == GL_EMISSION ? 3 : pname - GL_AMBIENT], params);
        break;

    case GL_SHININESS:
        params[0] = m->shininess;
        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

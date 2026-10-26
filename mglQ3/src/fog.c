#include "mglint.h"

static void fog_mode(GLcontext context, GLint m)
{
    switch (m)
    {
    case GL_LINEAR:
        context->w3dFogMode = W3D_FOG_LINEAR;
        break;
    case GL_EXP:
        context->w3dFogMode = W3D_FOG_EXP;
        break;
    case GL_EXP2:
        context->w3dFogMode = W3D_FOG_EXP_2;
        break;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->CurFogMode = (GLenum)m;
}

static void fog_param(GLcontext context, GLenum pname, const GLfloat *p)
{
    switch (pname)
    {
    case GL_FOG_MODE:
        fog_mode(context, (GLint)p[0]);
        break;

    case GL_FOG_DENSITY:
        if (p[0] < 0.0f)
        {
            mgl_error(context, GL_INVALID_VALUE);
            break;
        }

        context->w3dFog.fog_density = p[0];
        break;

    case GL_FOG_START:
        context->FogStart = p[0];
        break;

    case GL_FOG_END:
        context->FogEnd = p[0];
        break;

    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

void GLFogf(GLcontext context, GLenum pname, GLfloat param)
{
    context->FogDirty = GL_TRUE;

    fog_param(context, pname, &param);
}

void GLFogfv(GLcontext context, GLenum pname, GLfloat *param)
{
    context->FogDirty = GL_TRUE;

    if (pname == GL_FOG_COLOR)
    {
        context->w3dFog.fog_color.r = param[0];
        context->w3dFog.fog_color.g = param[1];
        context->w3dFog.fog_color.b = param[2];
        return;
    }

    fog_param(context, pname, param);
}

static float fog_w(float d, float p43, float p44)
{
    const float w = 1.0f / (p44 - d * p43);

    return w < 0.0f ? 0.0f : w > 1.0f ? 1.0f
                                      : w;
}

void fog_Set(GLcontext context)
{
    const GLfloat *p = CurrentP->v;
    const float p43 = p[OF_43];
    const float p44 = p[OF_44];

    context->w3dFog.fog_start = fog_w((float)context->FogStart, p43, p44);
    context->w3dFog.fog_end = fog_w((float)context->FogEnd, p43, p44);

    if (W3D_SetFogParams(context->w3dContext, &context->w3dFog, context->w3dFogMode) == (ULONG)W3D_UNSUPPORTEDFOG)
    {
        context->w3dFogMode = W3D_FOG_INTERPOLATED;

        W3D_SetFogParams(context->w3dContext, &context->w3dFog, context->w3dFogMode);
    }
}

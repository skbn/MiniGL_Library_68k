#include "mglint.h"

#define MGL_ZO_LSB (1.0f / 65536.0f)

// gl compare order differs from w3d order between EQUAL and GEQUAL
static const UBYTE mgl_alpha_map[8] =
    {
        W3D_A_NEVER,    /* GL_NEVER */
        W3D_A_LESS,     /* GL_LESS */
        W3D_A_EQUAL,    /* GL_EQUAL */
        W3D_A_LEQUAL,   /* GL_LEQUAL */
        W3D_A_GREATER,  /* GL_GREATER */
        W3D_A_NOTEQUAL, /* GL_NOTEQUAL */
        W3D_A_GEQUAL,   /* GL_GEQUAL */
        W3D_A_ALWAYS    /* GL_ALWAYS s*/
};

static const UBYTE mgl_stcmp_map[8] = {W3D_ST_NEVER, W3D_ST_LESS, W3D_ST_EQUAL, W3D_ST_LEQUAL, W3D_ST_GREATER, W3D_ST_NOTEQUAL, W3D_ST_GEQUAL, W3D_ST_ALWAYS};

void GLAlphaFunc(GLcontext context, GLenum func, GLclampf ref)
{
    W3D_Float wref = ref;

    if (func < GL_NEVER || func > GL_ALWAYS)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (wref < 0.0f)
        wref = 0.0f;
    else if (wref > 1.0f)
        wref = 1.0f;

    context->CurAlphaFunc = func;
    context->CurAlphaRef = wref;

    W3D_SetAlphaMode(context->w3dContext, mgl_alpha_map[func - GL_NEVER], &wref);
}

void GLStencilFunc(GLcontext context, GLenum func, GLint ref, GLuint mask)
{
    if (func < GL_NEVER || func > GL_ALWAYS)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->StencilFunc = func;
    context->StencilRef = ref;
    context->StencilValueMask = mask;

    if (context->StencilBits)
        W3D_SetStencilFunc(context->w3dContext, mgl_stcmp_map[func - GL_NEVER], ref, mask);
}

static ULONG mgl_stop(GLenum op)
{
    switch (op)
    {
    case GL_KEEP:
        return W3D_ST_KEEP;
    case GL_ZERO:
        return W3D_ST_ZERO;
    case GL_REPLACE:
        return W3D_ST_REPLACE;
    case GL_INCR:
        return W3D_ST_INCR;
    case GL_DECR:
        return W3D_ST_DECR;
    case GL_INVERT:
        return W3D_ST_INVERT;
    }

    return 0;
}

void GLStencilOp(GLcontext context, GLenum fail, GLenum zfail, GLenum zpass)
{
    const ULONG f = mgl_stop(fail);
    const ULONG zf = mgl_stop(zfail);
    const ULONG zp = mgl_stop(zpass);

    if (!(f && zf && zp))
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->StencilSfail = fail;
    context->StencilZfail = zfail;
    context->StencilZpass = zpass;

    if (context->StencilBits)
        W3D_SetStencilOp(context->w3dContext, f, zf, zp);
}

void GLClearStencil(GLcontext context, GLint s)
{
    context->StencilClear = s;
}

void GLStencilMask(GLcontext context, GLuint mask)
{
    context->StencilWriteMask = mask;
}

void GLBlendFunc(GLcontext context, GLenum sfactor, GLenum dfactor)
{
    const ULONG src = mgl_blend_src(sfactor);
    const ULONG dst = mgl_blend_dst(dfactor);

    if (!src || !dst)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->CurBlendSrc = sfactor;
    context->CurBlendDst = dfactor;

    W3D_SetBlendMode(context->w3dContext, src, dst);
}

void GLColorMask(GLcontext context, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    context->CurColorMask = (red != GL_FALSE) | (green != GL_FALSE) << 1 | (blue != GL_FALSE) << 2 | (alpha != GL_FALSE) << 3;

    W3D_SetColorMask(context->w3dContext, red, green, blue, alpha);
}

void GLLogicOp(GLcontext context, GLenum opcode)
{
    if (opcode < GL_CLEAR || opcode > GL_SET)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->LogicOp = opcode;

    if (context->LogicOpOK)
        W3D_SetLogicOp(context->w3dContext, opcode - GL_CLEAR + 1);
}

void GLRectf(GLcontext context, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
    if (context->CurrentPrimitive != GL_BASE)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    GLBegin(context, GL_POLYGON);
    GLVertex2f(context, x1, y1);
    GLVertex2f(context, x2, y1);
    GLVertex2f(context, x2, y2);
    GLVertex2f(context, x1, y2);
    GLEnd(context);
}

void GLPushAttrib(GLcontext context, GLbitfield mask)
{
    MGLAttrib *a;
    int i;

    if (context->CurrentPrimitive != GL_BASE)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (context->AttribTop >= MGL_ATTRIB_MAX)
    {
        mgl_error(context, GL_STACK_OVERFLOW);
        return;
    }

    a = &context->AttribStack[context->AttribTop++];
    a->mask = mask;

    a->en_alpha = context->AlphaTest_State;
    a->en_blend = context->Blend_State;
    a->en_cull = context->CullFace_State;
    a->en_depth = context->DepthTest_State;
    a->en_dither = context->Dither_State;
    a->en_fog = context->Fog_State;
    a->en_scissor = context->Scissor_State;
    a->en_stencil = context->Stencil_State;
    a->en_logic = context->LogicOp_State;
    a->en_points = context->PointSmooth_State;
    a->en_texgen_s = context->TextureGenS_State;
    a->en_texgen_t = context->TextureGenT_State;
    a->en_zoff = context->PolygonOffsetFill_State;
    a->en_wmask = context->CurWriteMask;
    a->en_normalize = context->Normalize_State;
    a->en_lighting = context->Lighting_State;
    a->en_colormat = context->ColorMaterial_State;
    a->en_rescale = context->Rescale_State;
    a->lightmask = context->LightMask;
    a->modelAmb = context->ModelAmbient;
    a->modelLV = context->ModelLocalViewer;
    a->modelTwo = context->ModelTwoSide;
    a->modelSep = context->ModelSepSpec;
    a->cmFace = context->ColorMaterialFace;
    a->cmMode = context->ColorMaterialMode;

    a->alphaFunc = context->CurAlphaFunc;
    a->alphaRef = context->CurAlphaRef;
    a->blendSrc = context->CurBlendSrc;
    a->blendDst = context->CurBlendDst;
    a->colorMask = context->CurColorMask;
    a->logicOp = context->LogicOp;
    a->depthFunc = context->CurDepthFunc;
    a->depthMask = context->DepthMask;
    a->depthClear = context->ClearDepth;
    a->stFunc = context->StencilFunc;
    a->stRef = context->StencilRef;
    a->stValueMask = context->StencilValueMask;
    a->stWriteMask = context->StencilWriteMask;
    a->stSfail = context->StencilSfail;
    a->stZfail = context->StencilZfail;
    a->stZpass = context->StencilZpass;
    a->stClear = context->StencilClear;
    a->scissor = context->scissor;
    a->fogMode = context->CurFogMode;
    a->fogStart = context->FogStart;
    a->fogEnd = context->FogEnd;
    a->polyMode = context->CurPolygonMode;
    a->cullFace = context->CurrentCullFace;
    a->pointSize = context->CurrentPointSize;
    a->lineWidth = context->LineWidth;
    a->clearColor = context->ClearColor;
    a->curColor[0] = context->CurrentColor.r;
    a->curColor[1] = context->CurrentColor.g;
    a->curColor[2] = context->CurrentColor.b;
    a->curColor[3] = context->CurrentColor.a;
    a->matrixMode = context->CurrentMatrixMode;
    a->ax = context->ax;
    a->ay = context->ay;
    a->sx = context->sx;
    a->sy = context->sy;
    a->clipFlags = context->ClipFlags;

    a->curBinding = context->CurrentBinding;
    a->activeTex = context->ActiveTexture;

    for (i = 0; i < MAX_TEXUNIT; i++)
    {
        a->texEnv[i] = context->TexEnv[i];
        a->virtBinding[i] = context->VirtualBinding[i];
        a->tex2d[i] = context->Texture2D_State[i];
    }

    for (i = 0; i < MGL_MAX_LIGHTS; i++)
        a->light[i] = context->Light[i];

    a->material[0] = context->Material[0];
    a->material[1] = context->Material[1];
}

void GLPopAttrib(GLcontext context)
{
    MGLAttrib *a;
    GLbitfield m;
    int i;

    if (context->CurrentPrimitive != GL_BASE)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (context->AttribTop <= 0)
    {
        mgl_error(context, GL_STACK_UNDERFLOW);
        return;
    }

    a = &context->AttribStack[--context->AttribTop];
    m = a->mask;

    if (m & GL_COLOR_BUFFER_BIT)
    {
        const GLfloat cc_r = (GLfloat)((a->clearColor >> 16) & 0xFF) / 255.0f;
        const GLfloat cc_g = (GLfloat)((a->clearColor >> 8) & 0xFF) / 255.0f;
        const GLfloat cc_b = (GLfloat)(a->clearColor & 0xFF) / 255.0f;
        const GLfloat cc_a = (GLfloat)((a->clearColor >> 24) & 0xFF) / 255.0f;

        GLClearColor(context, cc_r, cc_g, cc_b, cc_a);
        GLAlphaFunc(context, a->alphaFunc, a->alphaRef);
        GLBlendFunc(context, (GLenum)a->blendSrc, (GLenum)a->blendDst);
        GLColorMask(context, a->colorMask & 1, a->colorMask & 2, a->colorMask & 4, a->colorMask & 8);
        GLLogicOp(context, a->logicOp);

        MGLSetState(context, GL_ALPHA_TEST, a->en_alpha);
        MGLSetState(context, GL_BLEND, a->en_blend);
        MGLSetState(context, GL_DITHER, a->en_dither);
        MGLSetState(context, GL_COLOR_LOGIC_OP, a->en_logic);
    }

    if (m & GL_DEPTH_BUFFER_BIT)
    {
        GLDepthFunc(context, a->depthFunc);
        GLDepthMask(context, a->depthMask);
        GLClearDepth(context, a->depthClear);

        MGLSetState(context, GL_DEPTH_WRITEMASK, a->en_wmask);
        MGLSetState(context, GL_DEPTH_TEST, a->en_depth);
    }

    if (m & GL_STENCIL_BUFFER_BIT)
    {
        GLStencilFunc(context, a->stFunc, a->stRef, a->stValueMask);
        GLStencilMask(context, a->stWriteMask);
        GLStencilOp(context, a->stSfail, a->stZfail, a->stZpass);
        GLClearStencil(context, a->stClear);

        MGLSetState(context, GL_STENCIL_TEST, a->en_stencil);
    }

    if (m & GL_SCISSOR_BIT)
    {
        const GLfloat dh = (GLfloat)context->w3dContext->height;
        const GLint sc_x = a->scissor.left;
        const GLint sc_y = (GLint)(dh - a->scissor.top - a->scissor.height);

        GLScissor(context, sc_x, sc_y, a->scissor.width, a->scissor.height);
        MGLSetState(context, GL_SCISSOR_TEST, a->en_scissor);
    }

    if (m & GL_FOG_BIT)
    {
        GLFogf(context, GL_FOG_MODE, (GLfloat)a->fogMode);
        GLFogf(context, GL_FOG_START, (GLfloat)a->fogStart);
        GLFogf(context, GL_FOG_END, (GLfloat)a->fogEnd);
        MGLSetState(context, GL_FOG, a->en_fog);
    }

    if (m & GL_POLYGON_BIT)
    {
        GLCullFace(context, a->cullFace);
        MGLSetState(context, GL_CULL_FACE, a->en_cull);

        GLPolygonMode(context, GL_FRONT_AND_BACK, (GLenum)a->polyMode);
        MGLSetState(context, GL_POLYGON_OFFSET_FILL, a->en_zoff);
    }

    if (m & GL_POINT_BIT)
    {
        GLPointSize(context, a->pointSize);
        MGLSetState(context, GL_POINT_SMOOTH, a->en_points);
    }

    if (m & GL_LINE_BIT)
        GLLineWidth(context, a->lineWidth);

    if (m & GL_CURRENT_BIT)
        GLColor4f(context, a->curColor[0], a->curColor[1], a->curColor[2], a->curColor[3]);

    if (m & GL_TRANSFORM_BIT)
        GLMatrixMode(context, a->matrixMode);

    if (m & GL_VIEWPORT_BIT)
    {
        const GLfloat dh = (GLfloat)context->w3dContext->height;
        const GLint vp_x = (GLint)(a->ax - a->sx);
        const GLint vp_y = (GLint)(dh - a->ay - a->sy);
        const GLsizei vp_w = (GLsizei)(2.0f * a->sx);
        const GLsizei vp_h = (GLsizei)(2.0f * a->sy);

        GLViewport(context, vp_x, vp_y, vp_w, vp_h);

        context->ClipFlags = a->clipFlags;
    }

    if (m & GL_TEXTURE_BIT)
    {
        for (i = 0; i < (int)context->TexUnits; i++)
        {
            GLActiveTextureARB(context, i);
            GLBindTexture(context, GL_TEXTURE_2D, (GLuint)(i ? a->virtBinding[i] : a->curBinding));

            MGLSetState(context, GL_TEXTURE_2D, a->tex2d[i]);

            GLTexEnvi(context, GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, a->texEnv[i]);
        }

        GLActiveTextureARB(context, a->activeTex);

        MGLSetState(context, GL_TEXTURE_GEN_S, a->en_texgen_s);
        MGLSetState(context, GL_TEXTURE_GEN_T, a->en_texgen_t);
    }

    if (m & GL_LIGHTING_BIT)
    {
        for (i = 0; i < MGL_MAX_LIGHTS; i++)
            context->Light[i] = a->light[i];

        context->Material[0] = a->material[0];
        context->Material[1] = a->material[1];
        context->ModelAmbient = a->modelAmb;
        context->ModelLocalViewer = a->modelLV;
        context->ModelTwoSide = a->modelTwo;
        context->ModelSepSpec = a->modelSep;
        context->ColorMaterialFace = a->cmFace;
        context->ColorMaterialMode = a->cmMode;
        context->LightMask = a->lightmask;

        MGLSetState(context, GL_LIGHTING, a->en_lighting);
        MGLSetState(context, GL_COLOR_MATERIAL, a->en_colormat);
        MGLSetState(context, GL_NORMALIZE, a->en_normalize);
        MGLSetState(context, GL_RESCALE_NORMAL, a->en_rescale);
    }
}

void GLDrawBuffer(GLcontext context, GLenum mode)
{
    switch (mode)
    {
    case GL_NONE:
    case GL_FRONT:
    case GL_BACK:
    case GL_FRONT_AND_BACK:
        break;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    // shadow only, drawing always lands on the current draw region
    context->CurDrawBuffer = mode;
}

void GLReadBuffer(GLcontext context, GLenum mode)
{
    switch (mode)
    {
    case GL_NONE:
    case GL_FRONT:
    case GL_BACK:
    case GL_FRONT_AND_BACK:
        break;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->CurReadBuffer = mode;
}

static GLint mgl_cap_state(GLcontext context, GLenum cap)
{
    switch (cap)
    {
    case GL_DEPTH_TEST:
        return context->DepthTest_State;
    case GL_CULL_FACE:
        return context->CullFace_State;
    case GL_SCISSOR_TEST:
        return context->Scissor_State;
    case GL_FOG:
        return context->Fog_State;
    case GL_DITHER:
        return context->Dither_State;
    case GL_BLEND:
        return context->Blend_State;
    case GL_ALPHA_TEST:
        return context->AlphaTest_State;
    case GL_STENCIL_TEST:
        return context->Stencil_State;
    case GL_COLOR_LOGIC_OP:
        return context->LogicOp_State;
    case GL_EDGE_FLAG_ARRAY:
        return (context->ClientState & GLCS_EDGEFLAG) != 0;
    case GL_VERTEX_ARRAY:
        return (context->ClientState & GLCS_VERTEX) != 0;
    case GL_COLOR_ARRAY:
        return (context->ClientState & GLCS_COLOR) != 0;
    case GL_NORMAL_ARRAY:
        return (context->ClientState & GLCS_NORMAL) != 0;
    case GL_TEXTURE_COORD_ARRAY:
        return (context->TexCSMask >> context->ClientActiveTexture) & 1;
    case GL_POLYGON_OFFSET_FILL:
        return context->PolygonOffsetFill_State;
    case GL_POINT_SMOOTH:
        return context->PointSmooth_State;
    case GL_TEXTURE_2D:
        return context->Texture2D_State[context->ActiveTexture];
    case GL_TEXTURE_GEN_S:
        return context->TextureGenS_State;
    case GL_TEXTURE_GEN_T:
        return context->TextureGenT_State;
    case GL_DEPTH_WRITEMASK:
        return context->CurWriteMask;

    case GL_LIGHTING:
        return context->Lighting_State;
    case GL_LIGHT0:
    case GL_LIGHT1:
    case GL_LIGHT2:
    case GL_LIGHT3:
    case GL_LIGHT4:
    case GL_LIGHT5:
    case GL_LIGHT6:
    case GL_LIGHT7:
        return (context->LightMask >> (cap - GL_LIGHT0)) & 1;
    case GL_COLOR_MATERIAL:
        return context->ColorMaterial_State;
    case GL_NORMALIZE:
        return context->Normalize_State;
    case GL_RESCALE_NORMAL:
        return context->Rescale_State;
    case GL_COLOR_SUM:
    case GL_AUTO_NORMAL:
    case GL_LINE_SMOOTH:
    case GL_POLYGON_SMOOTH:
    case GL_LINE_STIPPLE:
    case GL_POLYGON_STIPPLE:
    case GL_INDEX_LOGIC_OP:
    case GL_TEXTURE_1D:
    case GL_TEXTURE_GEN_R:
    case GL_TEXTURE_GEN_Q:
        return 0;
    }

    return -1;
}

static GLint mgl_get_params(GLcontext context, GLenum pname, GLfloat *out)
{
    int i;

    switch (pname)
    {
    case GL_CURRENT_COLOR:
        out[0] = context->CurrentColor.r;
        out[1] = context->CurrentColor.g;
        out[2] = context->CurrentColor.b;
        out[3] = context->CurrentColor.a;
        return 4;

    case GL_COLOR_CLEAR_VALUE:
        out[0] = (GLfloat)((context->ClearColor >> 16) & 0xFF) / 255.0f;
        out[1] = (GLfloat)((context->ClearColor >> 8) & 0xFF) / 255.0f;
        out[2] = (GLfloat)(context->ClearColor & 0xFF) / 255.0f;
        out[3] = (GLfloat)((context->ClearColor >> 24) & 0xFF) / 255.0f;
        return 4;

    case GL_DEPTH_RANGE:
        out[0] = context->near;
        out[1] = context->far;
        return 2;

    case GL_VIEWPORT:
    {
        const GLfloat dh = (GLfloat)context->w3dContext->height;

        out[0] = context->ax - context->sx;
        out[1] = dh - context->ay - context->sy;
        out[2] = context->sx * 2.0f;
        out[3] = context->sy * 2.0f;
        return 4;
    }

    case GL_SCISSOR_BOX:
    {
        const GLfloat dh = (GLfloat)context->w3dContext->height;

        out[0] = (GLfloat)context->scissor.left;
        out[1] = dh - (GLfloat)context->scissor.top - (GLfloat)context->scissor.height;
        out[2] = (GLfloat)context->scissor.width;
        out[3] = (GLfloat)context->scissor.height;
        return 4;
    }

    case GL_DEPTH_FUNC:
        out[0] = (GLfloat)context->CurDepthFunc;
        return 1;

    case GL_DEPTH_CLEAR_VALUE:
        out[0] = (GLfloat)context->ClearDepth;
        return 1;

    case GL_TEXTURE_BINDING_2D:
        out[0] = (GLfloat)(context->ActiveTexture ? context->VirtualBinding[context->ActiveTexture] : context->CurrentBinding);
        return 1;

    case GL_CURRENT_RASTER_POSITION:
        out[0] = context->RasterPos[0];
        out[1] = context->RasterPos[1];
        out[2] = context->RasterPos[2];
        out[3] = context->RasterPos[3];
        return 4;

    case GL_CURRENT_RASTER_POSITION_VALID:
        out[0] = (GLfloat)context->RasterPosValid;
        return 1;

    case GL_ZOOM_X:
        out[0] = context->PixelZoomX;
        return 1;

    case GL_ZOOM_Y:
        out[0] = context->PixelZoomY;
        return 1;

    case GL_ACTIVE_TEXTURE_ARB:
        out[0] = (GLfloat)(GL_TEXTURE0_ARB + context->ActiveTexture);
        return 1;

    case GL_CLIENT_ACTIVE_TEXTURE_ARB:
        out[0] = (GLfloat)(GL_TEXTURE0_ARB + context->ClientActiveTexture);
        return 1;

    case GL_POLYGON_OFFSET_FACTOR:
        out[0] = context->PolygonOffsetFactor;
        return 1;

    case GL_POLYGON_OFFSET_UNITS:
        out[0] = context->PolygonOffsetUnits;
        return 1;

    case GL_CURRENT_TEXTURE_COORDS:
        if (context->ActiveTexture)
        {
            out[0] = context->CurrentTcoord[context->ActiveTexture].s;
            out[1] = context->CurrentTcoord[context->ActiveTexture].t;
            out[2] = 0.0f;
            out[3] = 1.0f;
        }
        else
        {
            out[0] = context->CurrentTexS;
            out[1] = context->CurrentTexT;
            out[2] = context->CurrentTexR;
            out[3] = 1.0f / context->CurrentTexQ; // stored as 1/q
        }

        return 4;

    case GL_CURRENT_NORMAL:
        out[0] = context->CurrentNormal.x;
        out[1] = context->CurrentNormal.y;
        out[2] = context->CurrentNormal.z;
        return 3;

    case GL_MAX_LIGHTS:
        out[0] = MGL_MAX_LIGHTS;
        return 1;

    case GL_LIGHT_MODEL_AMBIENT:
        out[0] = context->ModelAmbient.r;
        out[1] = context->ModelAmbient.g;
        out[2] = context->ModelAmbient.b;
        out[3] = context->ModelAmbient.a;
        return 4;

    case GL_LIGHT_MODEL_LOCAL_VIEWER:
        out[0] = (GLfloat)context->ModelLocalViewer;
        return 1;

    case GL_LIGHT_MODEL_TWO_SIDE:
        out[0] = (GLfloat)context->ModelTwoSide;
        return 1;

    case GL_LIGHT_MODEL_COLOR_CONTROL:
        out[0] = (GLfloat)(context->ModelSepSpec ? GL_SEPARATE_SPECULAR_COLOR : GL_SINGLE_COLOR);
        return 1;

    case GL_COLOR_MATERIAL_FACE:
        out[0] = (GLfloat)context->ColorMaterialFace;
        return 1;

    case GL_COLOR_MATERIAL_PARAMETER:
        out[0] = (GLfloat)context->ColorMaterialMode;
        return 1;

    case GL_MAX_MODELVIEW_STACK_DEPTH:
        out[0] = MODELVIEW_STACK_SIZE;
        return 1;

    case GL_MAX_PROJECTION_STACK_DEPTH:
        out[0] = PROJECTION_STACK_SIZE;
        return 1;

    case GL_PERSPECTIVE_CORRECTION_HINT:
        out[0] = (GLfloat)context->HintPersp;
        return 1;

    case GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES:
        out[0] = (GLfloat)GL_RGBA;
        return 1;

    case GL_IMPLEMENTATION_COLOR_READ_TYPE_OES:
        out[0] = (GLfloat)GL_UNSIGNED_BYTE;
        return 1;

    case GL_POINT_SMOOTH_HINT:
        out[0] = (GLfloat)context->HintPoint;
        return 1;

    case GL_LINE_SMOOTH_HINT:
        out[0] = (GLfloat)context->HintLine;
        return 1;

    case GL_POLYGON_SMOOTH_HINT:
        out[0] = (GLfloat)context->HintPoly;
        return 1;

    case GL_FOG_HINT:
        out[0] = (GLfloat)context->HintFog;
        return 1;

    case GL_POINT_SIZE:
        out[0] = context->CurrentPointSize;
        return 1;

    case GL_LINE_WIDTH:
        out[0] = context->LineWidth;
        return 1;

    case GL_MODELVIEW_MATRIX:
    {
        const GLfloat *m = context->ModelView[context->ModelViewNr].v;

        for (i = 0; i < 16; i++)
            out[i] = m[i];

        return 16;
    }

    case GL_PROJECTION_MATRIX:
    {
        const GLfloat *m = context->Projection[context->ProjectionNr].v;

        for (i = 0; i < 16; i++)
            out[i] = m[i];

        return 16;
    }

    case GL_MATRIX_MODE:
        out[0] = (GLfloat)context->CurrentMatrixMode;
        return 1;
    case GL_MODELVIEW_STACK_DEPTH:
        out[0] = (GLfloat)context->ModelViewStackPointer;
        return 1;
    case GL_PROJECTION_STACK_DEPTH:
        out[0] = (GLfloat)context->ProjectionStackPointer;
        return 1;

    case GL_SHADE_MODEL:
        out[0] = (GLfloat)context->ShadeModel;
        return 1;
    case GL_CULL_FACE_MODE:
        out[0] = (GLfloat)context->CurrentCullFace;
        return 1;
    case GL_FRONT_FACE:
        out[0] = (GLfloat)context->CurrentFrontFace;
        return 1;
    case GL_POLYGON_MODE:
        // front and back share one mode in this backend
        out[0] = (GLfloat)context->CurPolygonMode;
        out[1] = (GLfloat)context->CurPolygonMode;
        return 2;
    case GL_BLEND_SRC:
        out[0] = (GLfloat)context->CurBlendSrc;
        return 1;
    case GL_BLEND_DST:
        out[0] = (GLfloat)context->CurBlendDst;
        return 1;
    case GL_FOG_MODE:
        out[0] = (GLfloat)context->CurFogMode;
        return 1;
    case GL_FOG_DENSITY:
        out[0] = context->w3dFog.fog_density;
        return 1;
    case GL_FOG_START:
        out[0] = (GLfloat)context->FogStart;
        return 1;
    case GL_FOG_END:
        out[0] = (GLfloat)context->FogEnd;
        return 1;
    case GL_FOG_COLOR:
        out[0] = context->w3dFog.fog_color.r;
        out[1] = context->w3dFog.fog_color.g;
        out[2] = context->w3dFog.fog_color.b;
        out[3] = 0.0f; // fog alpha is unused by the fog blend, spec initial 0
        return 4;
    case GL_ALPHA_TEST_FUNC:
        out[0] = (GLfloat)context->CurAlphaFunc;
        return 1;
    case GL_ALPHA_TEST_REF:
        out[0] = context->CurAlphaRef;
        return 1;
    case GL_DRAW_BUFFER:
        out[0] = (GLfloat)context->CurDrawBuffer;
        return 1;
    case GL_READ_BUFFER:
        out[0] = (GLfloat)context->CurReadBuffer;
        return 1;
    case GL_COLOR_WRITEMASK:
        out[0] = (GLfloat)(context->CurColorMask & 1);
        out[1] = (GLfloat)((context->CurColorMask >> 1) & 1);
        out[2] = (GLfloat)((context->CurColorMask >> 2) & 1);
        out[3] = (GLfloat)((context->CurColorMask >> 3) & 1);
        return 4;
    case GL_PACK_ALIGNMENT:
        out[0] = (GLfloat)context->PackAlign;
        return 1;
    case GL_UNPACK_ALIGNMENT:
        out[0] = (GLfloat)context->UnpackAlign;
        return 1;
    case GL_PACK_ROW_LENGTH:
        out[0] = (GLfloat)context->CurPackRowLength;
        return 1;
    case GL_UNPACK_ROW_LENGTH:
        out[0] = (GLfloat)context->CurUnpackRowLength;
        return 1;
    case GL_PACK_SKIP_PIXELS:
        out[0] = (GLfloat)context->CurPackSkipPixels;
        return 1;
    case GL_UNPACK_SKIP_PIXELS:
        out[0] = (GLfloat)context->CurUnpackSkipPixels;
        return 1;
    case GL_PACK_SKIP_ROWS:
        out[0] = (GLfloat)context->CurPackSkipRows;
        return 1;
    case GL_UNPACK_SKIP_ROWS:
        out[0] = (GLfloat)context->CurUnpackSkipRows;
        return 1;
    case GL_PACK_SWAP_BYTES:
        out[0] = (GLfloat)context->CurPackSwapBytes;
        return 1;
    case GL_UNPACK_SWAP_BYTES:
        out[0] = (GLfloat)context->CurUnpackSwapBytes;
        return 1;
    case GL_PACK_LSB_FIRST:
        out[0] = (GLfloat)context->CurPackLsbFirst;
        return 1;
    case GL_UNPACK_LSB_FIRST:
        out[0] = (GLfloat)context->CurUnpackLsbFirst;
        return 1;
    case GL_TEXTURE_MIN_FILTER:
        out[0] = (GLfloat)context->MinFilter;
        return 1;
    case GL_TEXTURE_MAG_FILTER:
        out[0] = (GLfloat)context->MagFilter;
        return 1;
    case GL_TEXTURE_WRAP_S:
        out[0] = (GLfloat)context->WrapS;
        return 1;
    case GL_TEXTURE_WRAP_T:
        out[0] = (GLfloat)context->WrapT;
        return 1;
    case GL_TEXTURE_ENV_MODE:
        out[0] = (GLfloat)context->TexEnv[context->ActiveTexture];
        return 1;
    }

    i = mgl_cap_state(context, pname);

    if (i < 0)
        return -1;

    out[0] = (GLfloat)i;

    return 1;
}

void GLGetBooleanv(GLcontext context, GLenum pname, GLboolean *params)
{
    GLfloat out[16];
    const GLint n = mgl_get_params(context, pname, out);
    int i;

    if (n < 0)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    for (i = 0; i < n; i++)
        params[i] = out[i] != 0.0f ? GL_TRUE : GL_FALSE;
}

GLenum GLGetError(GLcontext context)
{
    const GLenum e = context->CurrentError;

    context->CurrentError = GL_NO_ERROR;

    return e;
}

void GLGetFloatv(GLcontext context, GLenum pname, GLfloat *params)
{
    if (mgl_get_params(context, pname, params) < 0)
        mgl_error(context, GL_INVALID_ENUM);
}

void GLGetDoublev(GLcontext context, GLenum pname, GLdouble *params)
{
    GLfloat out[16];
    const GLint n = mgl_get_params(context, pname, out);
    int i;

    if (n < 0)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    for (i = 0; i < n; i++)
        params[i] = (GLdouble)out[i];
}

void GLGetPointerv(GLcontext context, GLenum pname, GLvoid **params)
{
    switch (pname)
    {
    case GL_VERTEX_ARRAY_POINTER:
        params[0] = (GLvoid *)context->ArrayPointer.verts;
        break;
    case GL_COLOR_ARRAY_POINTER:
        params[0] = (GLvoid *)context->ArrayPointer.colors;
        break;
    case GL_TEXTURE_COORD_ARRAY_POINTER:
        params[0] = (GLvoid *)context->ArrayPointer.ctex[context->ClientActiveTexture].ptr;
        break;
    case GL_NORMAL_ARRAY_POINTER:
        params[0] = (GLvoid *)context->ArrNormal;
        break;
    case GL_EDGE_FLAG_ARRAY_POINTER:
        params[0] = (GLvoid *)context->EdgeFlagPtr;
        break;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

GLboolean GLIsTexture(GLcontext context, GLuint texture)
{
    if (texture && texture < (GLuint)context->TexBufferSize && context->GeneratedTextures)
        return context->GeneratedTextures[texture] ? GL_TRUE : GL_FALSE;

    return GL_FALSE;
}

GLboolean GLAreTexturesResident(GLcontext context, GLsizei n, const GLuint *textures, GLboolean *residences)
{
    GLboolean all = GL_TRUE;
    GLsizei i;

    if (n < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return GL_FALSE;
    }

    if (!textures || !residences)
        return GL_FALSE;

    for (i = 0; i < n; i++)
    {
        residences[i] = GLIsTexture(context, textures[i]);

        if (!residences[i])
            all = GL_FALSE;
    }

    return all;
}

void GLPrioritizeTextures(GLcontext context, GLsizei n, const GLuint *textures, const GLclampf *priorities)
{
    if (n < 0)
        mgl_error(context, GL_INVALID_VALUE);
}

void GLGetIntegerv(GLcontext context, GLenum pname, GLint *params)
{
    const W3D_Context *w3d = context->w3dContext;

    switch (pname)
    {
    case GL_MAX_TEXTURE_SIZE:
        params[0] = mglTexLimit(w3d ? w3d->maxtexwidthp : 0);
        break;

    case GL_MAX_VIEWPORT_DIMS:
        params[0] = w3d ? w3d->width : 0;
        params[1] = w3d ? w3d->height : 0;
        break;

    case GL_DEPTH_BITS:
        params[0] = w3d && w3d->zbufferalloc ? 16 : 0;
        break;

    case GL_STENCIL_BITS:
        params[0] = (GLfloat)context->StencilBits;
        break;

    case GL_STENCIL_FUNC:
        params[0] = (GLfloat)context->StencilFunc;
        break;

    case GL_STENCIL_REF:
        params[0] = (GLfloat)context->StencilRef;
        break;

    case GL_STENCIL_VALUE_MASK:
        params[0] = (GLfloat)context->StencilValueMask;
        break;

    case GL_STENCIL_WRITEMASK:
        params[0] = (GLfloat)context->StencilWriteMask;
        break;

    case GL_STENCIL_CLEAR_VALUE:
        params[0] = (GLfloat)context->StencilClear;
        break;

    case GL_STENCIL_FAIL:
        params[0] = (GLfloat)context->StencilSfail;
        break;

    case GL_STENCIL_PASS_DEPTH_FAIL:
        params[0] = (GLfloat)context->StencilZfail;
        break;

    case GL_STENCIL_PASS_DEPTH_PASS:
        params[0] = (GLfloat)context->StencilZpass;
        break;

    case GL_LOGIC_OP_MODE:
        params[0] = (GLfloat)context->LogicOp;
        break;

    case GL_MAX_ATTRIB_STACK_DEPTH:
        params[0] = (GLfloat)MGL_ATTRIB_MAX;
        break;

    case GL_ATTRIB_STACK_DEPTH:
        params[0] = (GLfloat)context->AttribTop;
        break;

    case GL_EDGE_FLAG:
        params[0] = (GLfloat)context->EdgeFlagCur;
        break;

    case GL_EDGE_FLAG_ARRAY_STRIDE:
        params[0] = (GLfloat)context->EdgeFlagStride;
        break;

    case GL_INDEX_BITS:
        params[0] = w3d && w3d->chunky ? 8 : 0;
        break;

    case GL_RED_BITS:
        params[0] = !w3d || w3d->chunky ? 0 : (w3d->depth >= 24 ? 8 : 5);
        break;

    case GL_GREEN_BITS:
        params[0] = !w3d || w3d->chunky ? 0 : (w3d->depth >= 24 ? 8 : (w3d->depth == 16 ? 6 : 5));
        break;

    case GL_BLUE_BITS:
        params[0] = !w3d || w3d->chunky ? 0 : (w3d->depth >= 24 ? 8 : 5);
        break;

    case GL_ALPHA_BITS:
        params[0] = w3d && w3d->destalpha ? 8 : 0;
        break;

    case GL_DOUBLEBUFFER:
        params[0] = context->NumBuffers > 1 || (context->w3dBitMap && !context->externalBitMap);
        break;

    case GL_STEREO:
    case GL_AUX_BUFFERS:
        params[0] = 0;
        break;

    case GL_MAX_TEXTURE_UNITS_ARB:
        params[0] = (GLint)context->TexUnits;
        break;

    default:
    {
        GLfloat out[16];
        const GLint n = mgl_get_params(context, pname, out);
        int i;

        if (n < 0)
        {
            mgl_error(context, GL_INVALID_ENUM);
            break;
        }

        for (i = 0; i < n; i++)
            params[i] = (GLint)(out[i] + 0.5f);

        break;
    }
    }
}

const GLubyte *GLGetString(GLcontext context, GLenum name)
{
    switch (name)
    {
    case GL_RENDERER:
        if (context->w3dContext)
        {
            switch (context->w3dContext->CurrentChip)
            {
            case W3D_CHIP_VIRGE:
                return (const GLubyte *)"VIRGE";
            case W3D_CHIP_PERMEDIA2:
                return (const GLubyte *)"Permedia 2";
            case W3D_CHIP_VOODOO1:
                return (const GLubyte *)"Voodoo";
            case W3D_CHIP_AVENGER_LE:
            case W3D_CHIP_AVENGER_BE:
                return (const GLubyte *)"Voodoo 3";
            case W3D_CHIP_PERMEDIA3:
                return (const GLubyte *)"Permedia 3";
            case W3D_CHIP_RADEON:
            case W3D_CHIP_RADEON2:
                return (const GLubyte *)"Radeon";
            }
        }

        return (const GLubyte *)"MiniGL/Warp3D";

    case GL_VERSION:
        return (const GLubyte *)"1.2";

    case GL_EXTENSIONS:
        if (context->w3dContext)
            return (const GLubyte *)"GL_MGL_packed_pixels GL_MGL_array_transformations GL_EXT_compiled_vertex_array GL_EXT_paletted_texture GL_EXT_multi_draw_arrays GL_ARB_multitexture GL_MGL_ARB_multitexture GL_OES_rgb8_rgba8 GL_OES_element_index_uint GL_OES_draw_elements_base_vertex GL_OES_read_format GL_OES_byte_coordinates GL_EXT_debug_marker GL_OES_vertex_array_object GL_EXT_clip_control GL_EXT_separate_specular_color GL_EXT_rescale_normal";

        return (const GLubyte *)"GL_MGL_packed_pixels GL_MGL_array_transformations GL_EXT_compiled_vertex_array GL_EXT_paletted_texture GL_EXT_multi_draw_arrays GL_OES_rgb8_rgba8 GL_OES_element_index_uint GL_OES_draw_elements_base_vertex GL_OES_read_format GL_OES_byte_coordinates GL_EXT_debug_marker GL_OES_vertex_array_object GL_EXT_clip_control";

    case GL_VENDOR:
        return (const GLubyte *)"MiniGL";

    default:
        mgl_error(context, GL_INVALID_ENUM);
        return NULL;
    }
}

void GLHint(GLcontext context, GLenum target, GLenum mode)
{
    ULONG wmode;
    ULONG wqual;
    GLenum *shadow = NULL;

    switch (target)
    {
    case GL_PERSPECTIVE_CORRECTION_HINT:
        wmode = W3D_H_PERSPECTIVE;
        shadow = &context->HintPersp;
        break;
    case GL_POINT_SMOOTH_HINT:
        wmode = W3D_H_POINTDRAW;
        shadow = &context->HintPoint;
        break;
    case GL_LINE_SMOOTH_HINT:
        wmode = W3D_H_ANTIALIASING;
        shadow = &context->HintLine;
        break;
    case GL_POLYGON_SMOOTH_HINT:
        wmode = W3D_H_ANTIALIASING;
        shadow = &context->HintPoly;
        break;
    case GL_FOG_HINT:
        wmode = W3D_H_FOGGING;
        shadow = &context->HintFog;
        break;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    switch (mode)
    {
    case GL_FASTEST:
        wqual = W3D_H_FAST;
        break;
    case GL_NICEST:
        wqual = W3D_H_NICE;
        break;
    case GL_DONT_CARE:
        wqual = W3D_H_AVERAGE;
        break;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    *shadow = mode;

    W3D_Hint(context->w3dContext, wmode, wqual);
}

GLboolean GLIsEnabled(GLcontext context, GLenum cap)
{
    const GLint s = mgl_cap_state(context, cap);

    if (s < 0)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return GL_FALSE;
    }

    return s ? GL_TRUE : GL_FALSE;
}

void GLPolygonMode(GLcontext context, GLenum face, GLenum mode)
{
    if (face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (mode != GL_POINT && mode != GL_LINE && mode != GL_FILL)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->CurPolygonMode = mode;
}

void GLPolygonOffset(GLcontext context, GLfloat factor, GLfloat units)
{
    context->PolygonOffsetFactor = factor;
    context->PolygonOffsetUnits = units;
    context->ZOffset = units * MGL_ZO_LSB;
}

static void mgl_pack_row(UBYTE *dst, const UBYTE *src, GLenum format, GLint n)
{
    GLint i;

    switch (format)
    {
    case GL_RGBA:
        memcpy(dst, src, n * 4);
        break;

    case GL_RGB:
        for (i = 0; i < n; i++, dst += 3, src += 4)
        {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
        }

        break;

    case GL_ALPHA:
        for (i = 0; i < n; i++, src += 4)
            dst[i] = src[3];

        break;

    case GL_LUMINANCE:
        for (i = 0; i < n; i++, src += 4)
            dst[i] = (UBYTE)((src[0] * 77 + src[1] * 150 + src[2] * 29 + 128) >> 8);

        break;

    case GL_LUMINANCE_ALPHA:
        for (i = 0; i < n; i++, dst += 2, src += 4)
        {
            dst[0] = (UBYTE)((src[0] * 77 + src[1] * 150 + src[2] * 29 + 128) >> 8);
            dst[1] = src[3];
        }

        break;

    case GL_RED:
        for (i = 0; i < n; i++, src += 4)
            dst[i] = src[0];

        break;

    case GL_GREEN:
        for (i = 0; i < n; i++, src += 4)
            dst[i] = src[1];

        break;

    case GL_BLUE:
        for (i = 0; i < n; i++, src += 4)
            dst[i] = src[2];

        break;
    }
}

// rows outside the window are left unwritten, same as spec-undefined pixels
void GLReadPixels(GLcontext context, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
    W3D_Context *w3d = context->w3dContext;
    GLint bpp;
    GLint cx0;
    GLint cw;
    GLint stride;
    GLint r;
    UBYTE *dst = (UBYTE *)pixels;

    if (type != GL_UNSIGNED_BYTE)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    switch (format)
    {
    case GL_RGBA:
        bpp = 4;
        break;
    case GL_RGB:
        bpp = 3;
        break;
    case GL_LUMINANCE_ALPHA:
        bpp = 2;
        break;
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_DEPTH_COMPONENT:
    case GL_STENCIL_INDEX:
        bpp = 1;
        break;
    case GL_COLOR_INDEX:
        // legal gl format, but the framebuffer is direct color
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (width < 0 || height < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (!pixels || !w3d || !w3d->drawregion || !width || !height)
        return;

    if (format == GL_DEPTH_COMPONENT && !w3d->zbufferalloc)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (format == GL_STENCIL_INDEX && !context->StencilBits)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    W3D_Flush(w3d);

    // clip the rect, dest column offset keeps the x<0 remainder
    cx0 = x > 0 ? x : 0;
    cw = x + width < (GLint)w3d->width ? x + width : (GLint)w3d->width;
    cw -= cx0;

    if (cw <= 0)
        return;

    const GLint rowlen = context->CurPackRowLength > 0 ? context->CurPackRowLength : width;

    stride = (rowlen * bpp + context->PackAlign - 1) / context->PackAlign * context->PackAlign;
    dst += context->CurPackSkipRows * stride + context->CurPackSkipPixels * bpp;

    if (format == GL_DEPTH_COMPONENT)
    {
        // W3D_ReadZSpan needs the hardware lock, keep the caller's state
        const GLboolean locked = context->w3dLocked;
        W3D_Double *zbuf = malloc(cw * sizeof(W3D_Double));

        if (!zbuf)
        {
            mgl_error(context, GL_OUT_OF_MEMORY);
            return;
        }

        if (!locked && W3D_LockHardware(w3d) != W3D_SUCCESS)
        {
            free(zbuf);
            return;
        }

        for (r = y < 0 ? -y : 0; r < height && y + r < (GLint)w3d->height; r++)
        {
            const GLint glrow = y + r;
            UBYTE *drow = dst + r * stride + (cx0 - x) * bpp;
            GLint i;

            W3D_ReadZSpan(w3d, cx0, w3d->height - 1 - glrow, cw, zbuf);

            for (i = 0; i < cw; i++)
            {
                W3D_Double z = zbuf[i];

                if (z < 0.0)
                    z = 0.0;
                else if (z > 1.0)
                    z = 1.0;

                drow[i] = (UBYTE)(z * 255.0 + 0.5);
            }
        }

        if (!locked)
            W3D_UnLockHardware(w3d);

        free(zbuf);
        return;
    }

    if (format == GL_STENCIL_INDEX)
    {
        const GLboolean locked = context->w3dLocked;
        ULONG *sbuf = malloc(cw * sizeof(ULONG));

        if (!sbuf)
        {
            mgl_error(context, GL_OUT_OF_MEMORY);
            return;
        }

        if (!locked && W3D_LockHardware(w3d) != W3D_SUCCESS)
        {
            free(sbuf);
            return;
        }

        for (r = y < 0 ? -y : 0; r < height && y + r < (GLint)w3d->height; r++)
        {
            const GLint glrow = y + r;
            UBYTE *drow = dst + r * stride + (cx0 - x) * bpp;
            GLint i;

            W3D_ReadStencilSpan(w3d, cx0, w3d->height - 1 - glrow, cw, sbuf);

            for (i = 0; i < cw; i++)
                drow[i] = (UBYTE)(sbuf[i] & 0xFF);
        }

        if (!locked)
            W3D_UnLockHardware(w3d);

        free(sbuf);
        return;
    }

    UBYTE *scr = malloc(cw * 4);
    struct RastPort rp;
    const GLboolean locked = context->w3dLocked;

    if (!scr)
    {
        mgl_error(context, GL_OUT_OF_MEMORY);
        return;
    }

    InitRastPort(&rp);

    rp.BitMap = w3d->drawregion;

    W3D_WaitIdle(w3d);

    if (locked)
        W3D_UnLockHardware(w3d);

    for (r = y < 0 ? -y : 0; r < height && y + r < (GLint)w3d->height; r++)
    {
        const GLint sy = (GLint)w3d->height - 1 - (y + r) + (GLint)w3d->yoffset;
        GLint i;

        ReadPixelArray(scr, 0, 0, cw * 4, &rp, cx0, sy, cw, 1, RECTFMT_ARGB);

        for (i = 0; i < cw; i++)
        {
            const UBYTE a = scr[i * 4];

            scr[i * 4] = scr[i * 4 + 1];
            scr[i * 4 + 1] = scr[i * 4 + 2];
            scr[i * 4 + 2] = scr[i * 4 + 3];
            scr[i * 4 + 3] = w3d->destalpha ? a : 255;
        }

        mgl_pack_row(dst + r * stride + (cx0 - x) * bpp, scr, format, cw);
    }

    if (locked)
        context->w3dLocked = W3D_LockHardware(w3d) == W3D_SUCCESS;

    free(scr);
}

void GLShadeModel(GLcontext context, GLenum mode)
{
    if (mode != GL_FLAT && mode != GL_SMOOTH)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->ShadeModel = mode;
    context->UpdateCurrentColor = GL_TRUE;

    W3D_SetState(context->w3dContext, W3D_GOURAUD, W3D_DISABLE - (mode - GL_FLAT));
}

void MGLExit(GLcontext context)
{
    context->Running = GL_FALSE;
}

void MGLIdleFunc(GLcontext context, IdleFn i)
{
    context->Idle = i;
}

void MGLKeyFunc(GLcontext context, KeyHandlerFn k)
{
    context->KeyHandler = k;
}

void MGLMouseFunc(GLcontext context, MouseHandlerFn m)
{
    context->MouseHandler = m;
}

void MGLSpecialFunc(GLcontext context, SpecialHandlerFn s)
{
    context->SpecialHandler = s;
}

static MGLspecial vid_specialkey(UWORD code)
{
    code &= 0x7F;

    switch (code)
    {
    case 0x4C:
        return MGLKEY_CUP;
    case 0x4D:
        return MGLKEY_CDOWN;
    case 0x4E:
        return MGLKEY_CRIGHT;
    case 0x4F:
        return MGLKEY_CLEFT;
    default:
        if (code >= 0x50 && code <= 0x59)
            return (MGLspecial)(MGLKEY_F1 + (LONG)(code - 0x50));

        return (MGLspecial)-1;
    }
}

void MGLMainLoop(GLcontext context)
{
    struct Window *win = context->w3dWindow;
    struct IntuiMessage *msg;
    GLbitfield buttons = 0;
    ULONG wantsig;

    if (!win || !win->UserPort)
        return;

    ModifyIDCMP(win, IDCMP_VANILLAKEY | IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY);

    wantsig = 1L << win->UserPort->mp_SigBit;
    context->Running = GL_TRUE;

    while (context->Running)
    {
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
        {
            ULONG cls = msg->Class;
            UWORD code = msg->Code;
            WORD mx = msg->MouseX;
            WORD my = msg->MouseY;

            ReplyMsg((struct Message *)msg);

            switch (cls)
            {
            case IDCMP_VANILLAKEY:
                if (context->KeyHandler)
                    context->KeyHandler((char)code);

                break;
            case IDCMP_RAWKEY:
                if (context->SpecialHandler)
                {
                    if (code & 0x80)
                    {
                        context->SpecialHandler((MGLspecial)(0x200 | (code & 0x7f)));
                    }
                    else
                    {
                        MGLspecial sp = vid_specialkey(code);

                        if ((LONG)sp >= 0)
                            context->SpecialHandler(sp);
                        else
                            context->SpecialHandler((MGLspecial)(0x100 | code));
                    }
                }

                break;
            case IDCMP_MOUSEBUTTONS:
                switch (code)
                {
                case SELECTDOWN:
                    buttons |= MGL_BUTTON_LEFT;
                    break;
                case SELECTUP:
                    buttons &= ~MGL_BUTTON_LEFT;
                    break;
                case MENUDOWN:
                    buttons |= MGL_BUTTON_RIGHT;
                    break;
                case MENUUP:
                    buttons &= ~MGL_BUTTON_RIGHT;
                    break;
                case MIDDLEDOWN:
                    buttons |= MGL_BUTTON_MID;
                    break;
                case MIDDLEUP:
                    buttons &= ~MGL_BUTTON_MID;
                    break;
                }

            case IDCMP_MOUSEMOVE:
                if (context->MouseHandler)
                    context->MouseHandler((GLint)mx, (GLint)my, buttons);

                break;
            case IDCMP_CLOSEWINDOW:
                context->Running = GL_FALSE;
                break;
            }
        }

        if (context->Idle)
            context->Idle();
        else
            Wait(wantsig);
    }
}

void MGLSetZOffset(GLcontext context, GLfloat offset)
{
    context->ZOffset = offset;
}

void GLLineWidth(GLcontext context, GLfloat width)
{
    if (width <= 0.0f)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    context->LineWidth = width;
}

static void mgl_marker(GLcontext context, GLsizei length, const GLchar *marker)
{
    GLsizei n;

    if (!marker)
        return;

    if (length < 0)
        length = (GLsizei)strlen(marker);

    n = length < (GLsizei)sizeof(context->DebugMarker) - 1 ? length : (GLsizei)sizeof(context->DebugMarker) - 1;

    memcpy(context->DebugMarker, marker, n);
    context->DebugMarker[n] = 0;
}

void GLInsertEventMarkerEXT(GLcontext context, GLsizei length, const GLchar *marker)
{
    mgl_marker(context, length, marker);
}

void GLPushGroupMarkerEXT(GLcontext context, GLsizei length, const GLchar *marker)
{
    context->DebugMarkerDepth++;

    mgl_marker(context, length, marker);
}

void GLPopGroupMarkerEXT(GLcontext context)
{
    if (context->DebugMarkerDepth)
        context->DebugMarkerDepth--;
}

void GLClipControlEXT(GLcontext context, GLenum origin, GLenum depth)
{
    if ((origin != GL_LOWER_LEFT && origin != GL_UPPER_LEFT) || (depth != GL_NEGATIVE_ONE_TO_ONE && depth != GL_ZERO_TO_ONE))
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    // z' = 2z - w is applied at outcode time so the +-w clip planes stay exact
    context->ClipUpperLeft = origin == GL_UPPER_LEFT;
    context->ClipZO = depth == GL_ZERO_TO_ONE;
}

static UBYTE *mgl_dec(UBYTE *p, ULONG v)
{
    UBYTE tmp[10];
    int n = 0;

    do
    {
        tmp[n++] = (UBYTE)('0' + v % 10);
        v /= 10;
    } while (v);

    while (n)
        *p++ = tmp[--n];

    return p;
}

// p6 dump of the draw region, rows top-down, through dos.library
void MGLWriteShotPPM(GLcontext context, char *filename)
{
    W3D_Context *w3d = context->w3dContext;
    BPTR f;
    UBYTE *scr;
    UBYTE hdr[40];
    UBYTE *p;
    struct RastPort rp;
    ULONG w;
    ULONG h;
    ULONG y;

    if (!filename || !w3d || !w3d->drawregion)
        return;

    w = w3d->width;
    h = w3d->height;

    f = Open((CONST_STRPTR)filename, MODE_NEWFILE);

    if (!f)
        return;

    scr = malloc(w * 4);

    if (!scr)
    {
        Close(f);
        return;
    }

    W3D_Flush(w3d);

    if (!context->w3dLocked && W3D_LockHardware(w3d) != W3D_SUCCESS)
    {
        free(scr);
        Close(f);
        return;
    }

    InitRastPort(&rp);

    rp.BitMap = w3d->drawregion;

    memcpy(hdr, "P6\n", 3);

    p = mgl_dec(hdr + 3, w);
    *p++ = ' ';
    p = mgl_dec(p, h);

    memcpy(p, "\n255\n", 5);

    p += 5;

    Write(f, hdr, p - hdr);

    for (y = 0; y < h; y++)
    {
        ULONG i;

        ReadPixelArray(scr, 0, 0, w * 4, &rp, 0, y, w, 1, RECTFMT_RGBA);

        // drop alpha in place, dst always trails src
        for (i = 0; i < w; i++)
        {
            scr[i * 3] = scr[i * 4];
            scr[i * 3 + 1] = scr[i * 4 + 1];
            scr[i * 3 + 2] = scr[i * 4 + 2];
        }

        Write(f, scr, w * 3);
    }

    if (!context->w3dLocked)
        W3D_UnLockHardware(w3d);

    free(scr);

    Close(f);
}

void GLRasterPos4f(GLcontext context, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    const float *m;
    float cx, cy, cz, cw;

    mglUpdateCombined(context);
    m = context->CombinedMatrix.v;

    cx = m[0] * x + m[4] * y + m[8] * z + m[12] * w;
    cy = m[1] * x + m[5] * y + m[9] * z + m[13] * w;
    cz = m[2] * x + m[6] * y + m[10] * z + m[14] * w;
    cw = m[3] * x + m[7] * y + m[11] * z + m[15] * w;

    // a frustum-clipped anchor invalidates the raster position
    if (cw <= MGL_CLIP_EPS || cx < -cw || cx > cw || cy < -cw || cy > cw || cz < -cw || cz > cw)
    {
        context->RasterPosValid = GL_FALSE;
        return;
    }

    const float iw = 1.0f / cw;

    context->RasterPos[0] = context->ax + cx * iw * context->sx;
    context->RasterPos[1] = context->ay - cy * iw * context->sy;
    context->RasterPos[2] = (GLfloat)(context->az + cz * iw * context->sz);
    context->RasterPos[3] = iw;
    context->RasterPosValid = GL_TRUE;
}

void GLPixelZoom(GLcontext context, GLfloat xfactor, GLfloat yfactor)
{
    context->PixelZoomX = xfactor;
    context->PixelZoomY = yfactor;
}

// dest-index to src-index under zoom, nearest resample, sign flips the image
static GLint px_src(GLint d, GLint span, GLint n, GLint flip)
{
    return ((flip ? span - 1 - d : d) * n) / span;
}

// shared blit core for drawpixels/copypixels, src image is bottom-up already,
// in one of the three write layouts, kind 0 rgba ubyte, 1 z double, 2 stencil
static void px_blit(GLcontext context, GLint kind, const GLvoid *src, GLint sw, GLint sh)
{
    W3D_Context *w3d = context->w3dContext;
    const GLint dw = (GLint)(sw * (context->PixelZoomX < 0.0f ? -context->PixelZoomX : context->PixelZoomX));
    const GLint dh = (GLint)(sh * (context->PixelZoomY < 0.0f ? -context->PixelZoomY : context->PixelZoomY));
    const GLint flipx = context->PixelZoomX < 0.0f;
    const GLint flipy = context->PixelZoomY < 0.0f;
    const GLint rx = (GLint)context->RasterPos[0];
    const GLint ry = (GLint)context->RasterPos[1];
    const GLboolean locked = context->w3dLocked;
    const int idw = sw == dw;
    const int idh = sh == dh;
    struct RastPort rp;
    GLvoid *row;
    GLint rowsz;
    GLint dr;

    if (!dw || !dh)
        return;

    rowsz = kind == 1 ? (GLint)sizeof(W3D_Double) : 4;
    row = malloc(dw * rowsz);

    if (!row)
    {
        mgl_error(context, GL_OUT_OF_MEMORY);
        return;
    }

    // buffered draws must land before direct writes, keep the GL order
    W3D_Flush(w3d);

    if (!locked && W3D_LockHardware(w3d) != W3D_SUCCESS)
    {
        free(row);
        return;
    }

    if (kind == 0)
    {
        InitRastPort(&rp);
        rp.BitMap = w3d->drawregion;
    }

    for (dr = 0; dr < dh; dr++)
    {
        const GLint srow = idh ? (flipy ? dh - 1 - dr : dr) : px_src(dr, dh, sh, flipy);
        const GLint screen_y = ry - dr;
        const GLint dx0 = rx < 0 ? -rx : 0;
        const GLint dx1 = rx + dw > (GLint)w3d->width ? (GLint)w3d->width - rx : dw;
        const GLint cw = dx1 - dx0;
        GLint dc;

        if (screen_y < 0 || screen_y >= (GLint)w3d->height || cw <= 0)
            continue;

        if (kind == 0)
        {
            const UBYTE *sline = (const UBYTE *)src + srow * sw * 4;
            UBYTE *dline = (UBYTE *)row;

            if (idw && !flipx)
            {
                memcpy(dline, sline + dx0 * 4, (size_t)cw * 4);
            }
            else
            {
                for (dc = dx0; dc < dx1; dc++)
                {
                    const GLint sc = idw ? sw - 1 - dc : px_src(dc, dw, sw, flipx);

                    memcpy(dline + (dc - dx0) * 4, sline + sc * 4, 4);
                }
            }

            WritePixelArray(dline, 0, 0, cw * 4, &rp, rx + dx0, screen_y, cw, 1, RECTFMT_RGBA);
        }
        else if (kind == 1)
        {
            const W3D_Double *sline = (const W3D_Double *)src + srow * sw;
            W3D_Double *dline = (W3D_Double *)row;

            if (idw && !flipx)
            {
                memcpy(dline, sline + dx0, (size_t)cw * sizeof(W3D_Double));
            }
            else
            {
                for (dc = dx0; dc < dx1; dc++)
                {
                    const GLint sc = idw ? sw - 1 - dc : px_src(dc, dw, sw, flipx);

                    dline[dc - dx0] = sline[sc];
                }
            }

            W3D_WriteZSpan(w3d, rx + dx0, screen_y, cw, dline, NULL);
        }
        else
        {
            const ULONG *sline = (const ULONG *)src + srow * sw;
            ULONG *dline = (ULONG *)row;

            if (idw && !flipx)
            {
                memcpy(dline, sline + dx0, (size_t)cw * sizeof(ULONG));
            }
            else
            {
                for (dc = dx0; dc < dx1; dc++)
                {
                    const GLint sc = idw ? sw - 1 - dc : px_src(dc, dw, sw, flipx);

                    dline[dc - dx0] = sline[sc];
                }
            }

            W3D_WriteStencilSpan(w3d, rx + dx0, screen_y, cw, dline, NULL);
        }
    }

    if (!locked)
        W3D_UnLockHardware(w3d);

    free(row);
}

// converts one packed ubyte row into the blit layout for its format
static void px_conv(GLenum format, const UBYTE *src, GLvoid *dst, GLint w)
{
    GLint i;

    switch (format)
    {
    case GL_RGBA:
        memcpy(dst, src, w * 4);
        break;

    case GL_RGB:
    {
        UBYTE *d = (UBYTE *)dst;

        for (i = 0; i < w; i++, src += 3, d += 4)
        {
            d[0] = src[0];
            d[1] = src[1];
            d[2] = src[2];
            d[3] = 255;
        }

        break;
    }

    case GL_LUMINANCE_ALPHA:
    {
        UBYTE *d = (UBYTE *)dst;

        for (i = 0; i < w; i++, src += 2, d += 4)
        {
            d[0] = src[0];
            d[1] = src[0];
            d[2] = src[0];
            d[3] = src[1];
        }

        break;
    }

    case GL_LUMINANCE:
    case GL_ALPHA:
    {
        UBYTE *d = (UBYTE *)dst;
        const GLboolean alpha = format == GL_ALPHA;

        for (i = 0; i < w; i++, src++, d += 4)
        {
            d[0] = alpha ? 0 : src[0];
            d[1] = alpha ? 0 : src[0];
            d[2] = alpha ? 0 : src[0];
            d[3] = alpha ? src[0] : 255;
        }

        break;
    }

    case GL_DEPTH_COMPONENT:
    {
        W3D_Double *d = (W3D_Double *)dst;

        for (i = 0; i < w; i++)
            d[i] = (W3D_Double)(src[i] / 255.0);

        break;
    }

    default:
    {
        ULONG *d = (ULONG *)dst;

        for (i = 0; i < w; i++)
            d[i] = src[i];

        break;
    }
    }
}

void GLDrawPixels(GLcontext context, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
    W3D_Context *w3d = context->w3dContext;
    const GLint ualign = context->UnpackAlign > 0 ? context->UnpackAlign : 1;
    GLint bpp;
    GLint kind;
    GLvoid *img;
    const UBYTE *src = (const UBYTE *)pixels;
    GLint r;
    GLint rowlen;
    GLint stride;
    GLint rowsz;

    if (type != GL_UNSIGNED_BYTE)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    switch (format)
    {
    case GL_RGBA:
        bpp = 4;
        kind = 0;
        break;
    case GL_RGB:
        bpp = 3;
        kind = 0;
        break;
    case GL_LUMINANCE_ALPHA:
        bpp = 2;
        kind = 0;
        break;
    case GL_ALPHA:
    case GL_LUMINANCE:
        bpp = 1;
        kind = 0;
        break;
    case GL_DEPTH_COMPONENT:
        bpp = 1;
        kind = 1;
        break;
    case GL_STENCIL_INDEX:
        bpp = 1;
        kind = 2;
        break;
    case GL_COLOR_INDEX:
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (width < 0 || height < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (!context->RasterPosValid || !pixels || !w3d || !w3d->drawregion || !width || !height)
        return;

    if (format == GL_DEPTH_COMPONENT && !w3d->zbufferalloc)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (format == GL_STENCIL_INDEX && !context->StencilBits)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    // unpack-state aware source walk, same convention as texupload/readpixels
    rowlen = context->CurUnpackRowLength > 0 ? context->CurUnpackRowLength : width;
    stride = (rowlen * bpp + ualign - 1) / ualign * ualign;
    src += context->CurUnpackSkipRows * stride + context->CurUnpackSkipPixels * bpp;

    rowsz = kind == 1 ? (GLint)sizeof(W3D_Double) : 4;
    img = malloc(width * height * rowsz);

    if (!img)
    {
        mgl_error(context, GL_OUT_OF_MEMORY);
        return;
    }

    for (r = 0; r < height; r++)
        px_conv(format, src + r * stride, (UBYTE *)img + r * width * rowsz, width);

    px_blit(context, kind, img, width, height);

    free(img);
}

void GLCopyPixels(GLcontext context, GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
    W3D_Context *w3d = context->w3dContext;
    GLvoid *img;
    GLint kind;
    GLint r;
    GLboolean locked;

    switch (type)
    {
    case GL_COLOR:
        kind = 0;
        break;
    case GL_DEPTH:
        kind = 1;
        break;
    case GL_STENCIL:
        kind = 2;
        break;
    default:
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (width < 0 || height < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (!context->RasterPosValid || !w3d || !w3d->drawregion || !width || !height)
        return;

    if (kind == 1 && !w3d->zbufferalloc)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (kind == 2 && !context->StencilBits)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    img = malloc(width * height * (kind == 1 ? (GLint)sizeof(W3D_Double) : 4));

    if (!img)
    {
        mgl_error(context, GL_OUT_OF_MEMORY);
        return;
    }

    W3D_Flush(w3d);

    locked = context->w3dLocked;

    if (kind == 0)
    {
        struct RastPort rp;

        InitRastPort(&rp);
        rp.BitMap = w3d->drawregion;

        if (!locked && W3D_LockHardware(w3d) != W3D_SUCCESS)
        {
            free(img);
            return;
        }

        for (r = 0; r < height; r++)
        {
            UBYTE *drow = (UBYTE *)img + r * width * 4;
            const GLint glrow = y + r;

            if (glrow < 0 || glrow >= (GLint)w3d->height)
                continue;

            ReadPixelArray(drow, 0, 0, width * 4, &rp, x, (GLint)w3d->height - 1 - glrow, width, 1, RECTFMT_RGBA);
        }
    }
    else
    {
        if (!locked && W3D_LockHardware(w3d) != W3D_SUCCESS)
        {
            free(img);
            return;
        }

        for (r = 0; r < height; r++)
        {
            const GLint glrow = y + r;

            if (glrow < 0 || glrow >= (GLint)w3d->height)
                continue;

            if (kind == 1)
                W3D_ReadZSpan(w3d, x, (ULONG)w3d->height - 1 - glrow, width, (W3D_Double *)img + r * width);
            else
                W3D_ReadStencilSpan(w3d, x, (ULONG)w3d->height - 1 - glrow, width, (ULONG *)img + r * width);
        }

        if (!locked)
            W3D_UnLockHardware(w3d);
    }

    px_blit(context, kind, img, width, height);

    free(img);
}

void GLBitmap(GLcontext context, GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap)
{
    W3D_Context *w3d = context->w3dContext;
    const GLint rowbytes = (width + 7) >> 3;
    UBYTE *rbuf;
    UBYTE px[4];
    struct RastPort rp;
    GLint i;
    GLint r;

    if (!context->RasterPosValid)
        return;

    if (width < 0 || height < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (!bitmap || !w3d || !w3d->drawregion || !width || !height)
    {
        context->RasterPos[0] += xmove;
        context->RasterPos[1] -= ymove;
        return;
    }

    px[0] = (UBYTE)(context->CurrentColor.r * 255.0f);
    px[1] = (UBYTE)(context->CurrentColor.g * 255.0f);
    px[2] = (UBYTE)(context->CurrentColor.b * 255.0f);
    px[3] = (UBYTE)(context->CurrentColor.a * 255.0f);

    // one row buffer pre-filled, every set bit writes the same color
    rbuf = malloc(width * 4);

    if (!rbuf)
    {
        mgl_error(context, GL_OUT_OF_MEMORY);
        return;
    }

    for (i = 0; i < width; i++)
        memcpy(rbuf + i * 4, px, 4);

    InitRastPort(&rp);
    rp.BitMap = w3d->drawregion;

    const GLint rx = (GLint)context->RasterPos[0] - (GLint)xorig;
    const GLint ry = (GLint)context->RasterPos[1] + (GLint)yorig;
    const GLboolean locked = context->w3dLocked;

    W3D_Flush(w3d);

    if (!locked && W3D_LockHardware(w3d) != W3D_SUCCESS)
    {
        free(rbuf);

        context->RasterPos[0] += xmove;
        context->RasterPos[1] -= ymove;
        return;
    }

    // bitmap row 0 is the bottom row, screen y climbs
    for (r = 0; r < height; r++)
    {
        const UBYTE *sline = bitmap + r * rowbytes;
        const GLint screen_y = ry - r;
        GLint run = -1;
        GLint c;

        if (screen_y < 0 || screen_y >= (GLint)w3d->height)
            continue;

        for (c = 0; c <= width; c++)
        {
            const GLint bit = c < width && (context->CurUnpackLsbFirst ? (sline[c >> 3] >> (c & 7)) : (sline[c >> 3] >> (7 - (c & 7)))) & 1;

            if (bit && run < 0)
                run = c;

            if (!bit && run >= 0)
            {
                WritePixelArray(rbuf, 0, 0, (c - run) * 4, &rp, rx + run, screen_y, c - run, 1, RECTFMT_RGBA);

                run = -1;
            }
        }
    }

    if (!locked)
        W3D_UnLockHardware(w3d);

    free(rbuf);

    context->RasterPos[0] += xmove;
    context->RasterPos[1] -= ymove;
}

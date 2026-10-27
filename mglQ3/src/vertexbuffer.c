#include "mglint.h"

// shared tail for the glVertex entries, flushes once the buffer fills
static void vb_done(GLcontext context)
{
    MGLVertex *v;
    GLuint i;

    if (context->CurrentPrimitive == GL_BASE)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    v = &context->VertexBuffer[context->VertexBufferPointer];

    v->normal = context->NormalBufferPointer;
    v->v.u = context->CurrentTexS;
    v->v.v = context->CurrentTexT;
    v->v.tex3d = context->CurrentTexR;
    v->q = context->CurrentTexQ;

    for (i = 0; i < context->TCopyMax; i++)
        v->tcoord[i] = context->CurrentTcoord[i + 1];

    // flat fallback needs every vertex's color for the provoking test
    v->v.color = context->CurrentColor;

    if (++context->VertexBufferPointer == context->VertexBufferSize)
        d_Submit(context, NULL, 0, GL_FALSE);
}

void GLBegin(GLcontext context, GLenum mode)
{
    // draw modes run GL_POINTS..GL_POLYGON contiguous
    if (context->CurrentPrimitive != GL_BASE)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (mode > GL_POLYGON)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (!context->VertexArrayPipeline)
        va_stagebind(context);

    context->CurrentPrimitive = mode;
    context->VertexBufferPointer = 0;
    context->EmitBase = 0;
    context->StripBase = 0;
    context->NormalBufferPointer = 0;
    context->WOne_Hint = GL_TRUE;
    context->LockStaged = GL_FALSE;
    context->LockXformed = GL_FALSE;
}

void GLEnd(GLcontext context)
{
    if (context->CurrentPrimitive == GL_BASE)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    d_Submit(context, NULL, 0, GL_TRUE);

    context->CurrentPrimitive = GL_BASE;

    if (!context->VertexArrayPipeline)
        va_bindptr(context);
}

void GLVertex2f(GLcontext context, GLfloat x, GLfloat y)
{
    MGLVertex *vert = &context->VertexBuffer[context->VertexBufferPointer];

    vert->bx = x;
    vert->by = y;
    vert->bz = 0.0f;
    vert->bw = 1.0f;

    vb_done(context);
}

void GLVertex2fv(GLcontext context, GLfloat *v)
{
    GLVertex2f(context, v[0], v[1]);
}

void GLVertex3fv(GLcontext context, GLfloat *v)
{
    MGLVertex *vert = &context->VertexBuffer[context->VertexBufferPointer];

    vert->bx = v[0];
    vert->by = v[1];
    vert->bz = v[2];
    vert->bw = 1.0f;

    vb_done(context);
}

void GLVertex4f(GLcontext context, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    MGLVertex *vert = &context->VertexBuffer[context->VertexBufferPointer];

    vert->bx = x;
    vert->by = y;
    vert->bz = z;
    vert->bw = w;

    // a non-unit staged w kills the one-w transform hint
    if (w != 1.0f)
        context->WOne_Hint = GL_FALSE;

    vb_done(context);
}

void GLVertex4fv(GLcontext context, GLfloat *v)
{
    MGLVertex *vert = &context->VertexBuffer[context->VertexBufferPointer];

    vert->bx = v[0];
    vert->by = v[1];
    vert->bz = v[2];
    vert->bw = v[3];

    if (v[3] != 1.0f)
        context->WOne_Hint = GL_FALSE;

    vb_done(context);
}

static void vb_color(GLcontext context, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    if (r < 0.0f)
        r = 0.0f;
    else if (r > 1.0f)
        r = 1.0f;

    if (g < 0.0f)
        g = 0.0f;
    else if (g > 1.0f)
        g = 1.0f;

    if (b < 0.0f)
        b = 0.0f;
    else if (b > 1.0f)
        b = 1.0f;

    if (a < 0.0f)
        a = 0.0f;
    else if (a > 1.0f)
        a = 1.0f;

    context->CurrentColor.r = r;
    context->CurrentColor.g = g;
    context->CurrentColor.b = b;
    context->CurrentColor.a = a;
    context->UpdateCurrentColor = GL_TRUE;
}

void GLColor3fv(GLcontext context, GLfloat *v)
{
    vb_color(context, v[0], v[1], v[2], 1.0f);
}

void GLColor3ubv(GLcontext context, GLubyte *v)
{
    vb_color(context, v[0] * (1.0f / 255.0f), v[1] * (1.0f / 255.0f), v[2] * (1.0f / 255.0f), 1.0f);
}

void GLColor4f(GLcontext context, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    vb_color(context, red, green, blue, alpha);
}

void GLColor4fv(GLcontext context, GLfloat *v)
{
    vb_color(context, v[0], v[1], v[2], v[3]);
}

void GLColor4ub(GLcontext context, GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    vb_color(context, red * (1.0f / 255.0f), green * (1.0f / 255.0f), blue * (1.0f / 255.0f), alpha * (1.0f / 255.0f));
}

void GLColor4ubv(GLcontext context, GLubyte *v)
{
    vb_color(context, v[0] * (1.0f / 255.0f), v[1] * (1.0f / 255.0f), v[2] * (1.0f / 255.0f), v[3] * (1.0f / 255.0f));
}

void GLNormal3f(GLcontext context, GLfloat x, GLfloat y, GLfloat z)
{
    MGLNormal *nb;

    // current-normal state updates even when the buffer is full
    context->CurrentNormal.x = x;
    context->CurrentNormal.y = y;
    context->CurrentNormal.z = z;

    if (context->NormalBufferPointer + 1 >= context->VertexBufferSize - MGL_NORMAL_RESERVE)
        return;

    nb = &context->NormalBuffer[++context->NormalBufferPointer];
    nb->x = x;
    nb->y = y;
    nb->z = z;
}

void GLNormal3fv(GLcontext context, GLfloat *n)
{
    GLNormal3f(context, n[0], n[1], n[2]);
}

void GLTexCoord2f(GLcontext context, GLfloat s, GLfloat t)
{
    context->CurrentTexS = s;
    context->CurrentTexT = t;
    context->CurrentTexR = 0.0f;
    context->CurrentTexQ = 1.0f;

    context->CurrentTexQValid = GL_FALSE;
}

void GLTexCoord2fv(GLcontext context, GLfloat *v)
{
    GLTexCoord2f(context, v[0], v[1]);
}

void GLTexCoord4f(GLcontext context, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    // w feeds the driver as 1/q so the per-pixel divide lands s/q
    context->CurrentTexS = s;
    context->CurrentTexT = t;
    context->CurrentTexR = r;
    context->CurrentTexQ = 1.0f / q;

    context->CurrentTexQValid = GL_TRUE;
}

void GLTexCoord4fv(GLcontext context, GLfloat *v)
{
    GLTexCoord4f(context, v[0], v[1], v[2], v[3]);
}

void GLMultiTexCoord2fARB(GLcontext context, GLenum unit, GLfloat s, GLfloat t)
{
    const int u = unit - GL_TEXTURE0_ARB;

    if (u < 0 || u >= (int)context->TexUnits)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    if (u)
    {
        context->CurrentTcoord[u].s = s;
        context->CurrentTcoord[u].t = t;
        return;
    }

    context->CurrentTexS = s;
    context->CurrentTexT = t;
    context->CurrentTexR = 0.0f;
    context->CurrentTexQ = 1.0f;
    context->CurrentTexQValid = GL_FALSE;
}

void GLMultiTexCoord2fvARB(GLcontext context, GLenum unit, GLfloat *v)
{
    GLMultiTexCoord2fARB(context, unit, v[0], v[1]);
}

void GLArrayElement(GLcontext context, GLint i)
{
    const MGLAPointer *ap = &context->ArrayPointer;
    const GLbitfield cs = context->ClientState;

    if (context->CurrentPrimitive == GL_BASE)
    {
        mgl_error(context, GL_INVALID_OPERATION);
        return;
    }

    if (i < 0 || !(cs & GLCS_VERTEX) || !ap->verts || ((cs & GLCS_TEXTURE) && !ap->ctex[0].ptr) || ((cs & GLCS_COLOR) && !ap->colors) || ((cs & GLCS_NORMAL) && !context->ArrNormal))
        return;

    va_Stage(context, (GLuint)i, context->VertexBufferPointer);

    if (++context->VertexBufferPointer == context->VertexBufferSize)
        d_Submit(context, NULL, 0, GL_FALSE);
}

void GLPointSize(GLcontext context, GLfloat size)
{
    if (size <= 0.0f)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    context->CurrentPointSize = size;
}

void GLDepthRange(GLcontext context, GLclampd n, GLclampd f)
{
    if (n < 0.0)
        n = 0.0;
    else if (n > 1.0)
        n = 1.0;

    if (f < 0.0)
        f = 0.0;
    else if (f > 1.0)
        f = 1.0;

    context->near = n;
    context->far = f;
    context->sz = (GLfloat)((f - n) * 0.5);
    context->az = (GLfloat)((n + f) * 0.5);

    context->LockStaged = GL_FALSE;
    context->LockXformed = GL_FALSE;
}

void GLViewport(GLcontext context, GLint x, GLint y, GLsizei width, GLsizei height)
{
    const GLfloat dh = (GLfloat)context->w3dContext->height;

    if (width < 0 || height < 0)
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (context->GuardBand && context->w3dScreen)
    {
        GLuint cf = MGL_CLIP_NEGW | MGL_CLIP_FRONT | MGL_CLIP_BACK;

        if (x > 0)
            cf |= MGL_CLIP_LEFT;

        if (x + width < context->w3dScreen->Width)
            cf |= MGL_CLIP_RIGHT;

        if (y > 0)
            cf |= MGL_CLIP_BOTTOM;

        if (y + height < context->w3dScreen->Height)
            cf |= MGL_CLIP_TOP;

        context->ClipFlags = cf;
    }
    else // x/y also clip on CPU, the rasterizer clip cannot be trusted everywhere
        context->ClipFlags = (MGL_CLIP_NEGW | MGL_CLIP_LEFT | MGL_CLIP_RIGHT | MGL_CLIP_TOP | MGL_CLIP_BOTTOM | MGL_CLIP_FRONT | MGL_CLIP_BACK);

    context->ax = (GLfloat)x + (GLfloat)width * 0.5f;
    context->ay = dh - (GLfloat)y - (GLfloat)height * 0.5f;
    context->sx = (GLfloat)width * 0.5f;
    context->sy = (GLfloat)height * 0.5f;

    context->LockStaged = GL_FALSE;
    context->LockXformed = GL_FALSE;
}

void GLFinish(GLcontext context)
{
    W3D_WaitIdle(context->w3dContext);
}

void GLFlush(GLcontext context)
{
    W3D_Flush(context->w3dContext);
}

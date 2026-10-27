#include "mglint.h"
#include <exec/memory.h>
#include <graphics/modeid.h>
#include <graphics/displayinfo.h>

#define MGL_POINTER_SIZE 8
#define MGL_PALETTE_BYTES 1024
#define MGL_CLOSE_DELAY 25
#define MGL_WIN_LEFT 90
#define MGL_WIN_TOP 60
#define MGL_MIN_COPY 0xC0

static int newVertexBufferSize = 256;
static int newTextureBufferSize = 2048;
static int newMtexBufferSize;
static int newNumberOfBuffers = 2;
static int newPixelDepth = 15;
static GLboolean newWindowMode;
static GLboolean clw;
static GLboolean newNoMipMapping = GL_TRUE;
static GLboolean newNoFallbackAlpha;
static GLboolean newGuardBand;

static UWORD *mgl_MousePointer;

static struct TagItem mgl_w3dctags[7] __attribute__((aligned(4)));

static struct TagItem mgl_bestmodetags[] __attribute__((aligned(4))) =
    {
        {W3D_BMI_WIDTH, 0},
        {W3D_BMI_HEIGHT, 0},
        {W3D_BMI_DEPTH, 0},
        {TAG_DONE, 0}};

static struct TagItem mgl_openscrtags[] __attribute__((aligned(4))) =
    {
        {SA_Depth, 8},
        {SA_DisplayID, 0},
        {SA_ShowTitle, FALSE},
        {SA_Draggable, FALSE},
        {TAG_DONE, 0}};

static struct TagItem mgl_openwintags[] __attribute__((aligned(4))) =
    {
        {WA_CustomScreen, 0},
        {WA_Width, 0},
        {WA_Height, 0},
        {WA_Left, 0},
        {WA_Top, 0},
        {WA_Title, 0},
        {WA_SimpleRefresh, TRUE},
        {WA_NoCareRefresh, TRUE},
        {WA_Flags, WFLG_ACTIVATE | WFLG_BORDERLESS | WFLG_BACKDROP | WFLG_RMBTRAP},
        {WA_IDCMP, IDCMP_VANILLAKEY | IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY},
        {TAG_DONE, 0}};

static struct TagItem mgl_pubwintags[] __attribute__((aligned(4))) =
    {
        {WA_PubScreen, 0},
        {WA_InnerWidth, 0},
        {WA_InnerHeight, 0},
        {WA_Left, MGL_WIN_LEFT},
        {WA_Top, MGL_WIN_TOP},
        {WA_Title, 0},
        {WA_DragBar, TRUE},
        {WA_DepthGadget, TRUE},
        {WA_CloseGadget, TRUE},
        {WA_Flags, WFLG_ACTIVATE | WFLG_RMBTRAP},
        {WA_IDCMP, IDCMP_VANILLAKEY | IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY},
        {TAG_DONE, 0}};

struct Device *TimerBase;

static GLboolean vid_BitMapTarget(GLcontext context, int w, int h, struct BitMap *friend);

static W3D_Driver *vid_DriverForMode(ULONG modeID);

void TMA_Start(LockTimeHandle *handle)
{
    struct EClockVal clock;

    if (!TimerBase)
    {
        Forbid();

        TimerBase = (struct Device *)FindName(&SysBase->DeviceList, (CONST_STRPTR)TIMERNAME);

        Permit();
    }

    handle->e_freq = ReadEClock(&clock);
    handle->s_lo = clock.ev_lo;
    handle->s_hi = clock.ev_hi;
}

void TMA_Stop(LockTimeHandle *handle)
{
    struct EClockVal clock;

    ReadEClock(&clock);

    handle->s_lo = clock.ev_lo - handle->s_lo;
    handle->s_hi = clock.ev_hi - handle->s_hi;
}

double TMA_GetTime(LockTimeHandle *handle)
{
    struct EClockVal clock;
    ULONG lo;

    ReadEClock(&clock);

    lo = clock.ev_lo - handle->s_lo;

    return (double)(lo * (1.0 / (double)handle->e_freq));
}

static GLuint mgl_texunits(void)
{
    UBYTE buf[8] = {0};

    if (DOSBase && GetVar((STRPTR) "MINIGL_TEXUNITS", (STRPTR)buf, sizeof buf, 0) > 0)
    {
        const UBYTE *e;
        GLuint n = 0;

        for (e = buf; *e >= '0' && *e <= '9'; e++)
            n = n * 10 + (*e - '0');

        if (n > 0 && n < MAX_TEXUNIT)
            return n;
    }

    return MAX_TEXUNIT;
}

static void vid_Pointer(struct Window *window)
{
    if (!mgl_MousePointer)
        mgl_MousePointer = AllocVec(MGL_POINTER_SIZE, MEMF_CLEAR | MEMF_CHIP | MEMF_PUBLIC);

    if (window)
        SetPointer(window, mgl_MousePointer, 0, 0, 0, 0);
}

static void vid_DeletePointer(struct Window *window)
{
    if (window)
        ClearPointer(window);

    if (mgl_MousePointer)
    {
        FreeVec(mgl_MousePointer);
        mgl_MousePointer = NULL;
    }
}

void MGLSetPointer(GLcontext context)
{
    vid_Pointer(context->w3dWindow);
}

void MGLClearPointer(GLcontext context)
{
    vid_DeletePointer(context->w3dWindow);
}

static void vid_FreeTextures(GLcontext context)
{
    int i;

    if (!context->w3dTexBuffer)
        return;

    for (i = 0; i < context->TexBufferSize; i++)
    {
        if (context->w3dTexBuffer[i])
            W3D_FreeTexObj(context->w3dContext, context->w3dTexBuffer[i]);

        if (context->w3dTexMemory && context->w3dTexMemory[i])
            free(context->w3dTexMemory[i]);
    }
}

static void vid_W3DTeardown(GLcontext context)
{
    if (!context->w3dContext)
        return;

    if (context->StencilBits)
        W3D_FreeStencilBuffer(context->w3dContext);

    W3D_FreeZBuffer(context->w3dContext);
    vid_FreeTextures(context);

    W3D_DestroyContext(context->w3dContext);
    context->w3dContext = NULL;
}

static GLboolean vid_W3DContext(GLcontext context, struct BitMap *bm, ULONG modeid)
{
    ULONG error = W3D_SUCCESS;
    int n = 0;

    mgl_w3dctags[n].ti_Tag = W3D_CC_BITMAP;
    mgl_w3dctags[n++].ti_Data = (ULONG)bm;
    mgl_w3dctags[n].ti_Tag = W3D_CC_YOFFSET;
    mgl_w3dctags[n++].ti_Data = 0;
    mgl_w3dctags[n].ti_Tag = W3D_CC_DRIVERTYPE;
    mgl_w3dctags[n++].ti_Data = W3D_DRIVER_BEST;
    mgl_w3dctags[n].ti_Tag = W3D_CC_GLOBALTEXENV;
    mgl_w3dctags[n++].ti_Data = TRUE;
    mgl_w3dctags[n].ti_Tag = W3D_CC_FAST;
    mgl_w3dctags[n++].ti_Data = FALSE;

    if (modeid != (ULONG)INVALID_ID)
    {
        mgl_w3dctags[n].ti_Tag = W3D_CC_MODEID;
        mgl_w3dctags[n++].ti_Data = modeid;
    }

    mgl_w3dctags[n].ti_Tag = TAG_DONE;
    mgl_w3dctags[n].ti_Data = 0;

    context->w3dContext = W3D_CreateContext(&error, mgl_w3dctags);

    if (!context->w3dContext || error != W3D_SUCCESS)
        return GL_FALSE;

    context->TexFmtDrv = vid_DriverForMode(modeid);

    return GL_TRUE;
}

static GLboolean vid_W3DDefaults(GLcontext context)
{
    W3D_Context *w3d = context->w3dContext;

    W3D_SetState(w3d, W3D_DITHERING, W3D_ENABLE);
    W3D_SetState(w3d, W3D_GOURAUD, W3D_ENABLE);
    W3D_SetState(w3d, W3D_PERSPECTIVE, W3D_ENABLE);
    W3D_SetState(w3d, W3D_AUTOTEXMANAGEMENT, W3D_ENABLE);

    if (W3D_AllocZBuffer(w3d) != W3D_SUCCESS)
        return GL_FALSE;

    context->StencilBits = (GLubyte)(W3D_Query(w3d, W3D_Q_STENCILBUFFER, 0) != W3D_NOT_SUPPORTED);

    if (context->StencilBits && W3D_AllocStencilBuffer(w3d) != W3D_SUCCESS)
        context->StencilBits = 0;

    context->LogicOpOK = (W3D_Query(w3d, W3D_Q_LOGICOP, 0) != W3D_NOT_SUPPORTED);

    W3D_SetState(w3d, W3D_ZBUFFER, W3D_DISABLE);
    W3D_SetState(w3d, W3D_ZBUFFERUPDATE, W3D_ENABLE);
    W3D_SetState(w3d, W3D_SCISSOR, W3D_DISABLE);
    W3D_SetZCompareMode(w3d, W3D_Z_LESS);

    W3D_SetFrontFace(w3d, W3D_CW);

    context->w3dLocked = GL_FALSE;

    context->CullHW = (W3D_SetState(w3d, W3D_CULLFACE, W3D_DISABLE) == W3D_SUCCESS);

    return GL_TRUE;
}

static void vid_ScissorFull(GLcontext context, W3D_Scissor *s)
{
    s->left = 0;
    s->top = 0;
    s->width = context->w3dContext->width;
    s->height = context->w3dContext->height;
}

void GLScissor(GLcontext context, GLint x, GLint y, GLsizei width, GLsizei height)
{
    GLint dw, dh;

    dw = context->w3dContext->width;
    dh = context->w3dContext->height;

    if (x < 0)
    {
        width += x;
        x = 0;
    }

    if (y < 0)
    {
        height += y;
        y = 0;
    }

    if (x + width > dw)
        width = dw - x;

    if (y + height > dh)
        height = dh - y;

    if (width < 0)
        width = 0;

    if (height < 0)
        height = 0;

    // stored GL box, y flipped into W3D top-down space
    context->scissor.left = x;
    context->scissor.top = dh - y - height;
    context->scissor.width = width;
    context->scissor.height = height;

    if (context->Scissor_State)
        W3D_SetScissor(context->w3dContext, &context->scissor);
}

static void vid_CloseDisplay(GLcontext context)
{
    int i;

    vid_W3DTeardown(context);

    Delay(MGL_CLOSE_DELAY);

    if (context->Buffers[0])
    {
        context->Buffers[0]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = NULL;

        while (!ChangeScreenBuffer(context->w3dScreen, context->Buffers[0]))
            ;
    }

    for (i = 0; i < context->NumBuffers; i++)
    {
        if (context->Buffers[i])
            FreeScreenBuffer(context->w3dScreen, context->Buffers[i]);

        context->Buffers[i] = NULL;
    }

    vid_DeletePointer(context->w3dWindow);

    if (context->w3dWindow)
    {
        CloseWindow(context->w3dWindow);
        context->w3dWindow = NULL;
    }

    if (context->w3dScreen)
    {
        CloseScreen(context->w3dScreen);
        context->w3dScreen = NULL;
    }

    if (clw)
        OpenWorkBench();
}

static void vid_CloseWindow(GLcontext context)
{
    vid_W3DTeardown(context);

    if (!context->externalWindow)
    {
        if (context->w3dWindow)
            CloseWindow(context->w3dWindow);

        if (context->pubLocked)
        {
            UnlockPubScreen(NULL, context->w3dScreen);
            context->pubLocked = GL_FALSE;
        }
    }

    context->w3dWindow = NULL;
    context->w3dScreen = NULL;

    if (context->w3dBitMap && !context->externalBitMap)
        FreeBitMap(context->w3dBitMap);

    context->w3dBitMap = NULL;

    if (context->w3dRastPort)
    {
        free(context->w3dRastPort);
        context->w3dRastPort = NULL;
    }
}

static GLboolean vid_OpenDisplay(GLcontext context, int pw, int ph, ULONG id)
{
    ULONG modeid;
    int i;
    W3D_Scissor full;

    if (id == MGL_SM_BESTMODE || id == MGL_SM_WINDOWMODE)
    {
        mgl_bestmodetags[0].ti_Data = (ULONG)pw;
        mgl_bestmodetags[1].ti_Data = (ULONG)ph;
        mgl_bestmodetags[2].ti_Data = (ULONG)newPixelDepth;

        modeid = W3D_BestModeID(mgl_bestmodetags);

        if (modeid == (ULONG)INVALID_ID)
            return GL_FALSE;
    }
    else
        modeid = id;

    if (clw)
        CloseWorkBench();

    mgl_openscrtags[1].ti_Data = modeid;

    context->w3dScreen = OpenScreenTagList(NULL, mgl_openscrtags);

    if (!context->w3dScreen)
    {
        if (clw)
            OpenWorkBench();

        return GL_FALSE;
    }

    mgl_openwintags[0].ti_Data = (ULONG)context->w3dScreen;
    mgl_openwintags[1].ti_Data = (ULONG)context->w3dScreen->Width;
    mgl_openwintags[2].ti_Data = (ULONG)context->w3dScreen->Height;

    context->w3dWindow = OpenWindowTagList(NULL, mgl_openwintags);

    if (!context->w3dWindow)
    {
        vid_CloseDisplay(context);
        return GL_FALSE;
    }

    context->NumBuffers = newNumberOfBuffers;
    context->Buffers[0] = AllocScreenBuffer(context->w3dScreen, NULL, SB_SCREEN_BITMAP);

    if (!context->Buffers[0])
    {
        vid_CloseDisplay(context);
        return GL_FALSE;
    }

    for (i = 1; i < context->NumBuffers; i++)
    {
        context->Buffers[i] = AllocScreenBuffer(context->w3dScreen, NULL, 0);

        if (!context->Buffers[i])
        {
            vid_CloseDisplay(context);
            return GL_FALSE;
        }
    }

    context->BufNr = (context->NumBuffers > 1) ? 1 : 0;
    context->DoSync = GL_TRUE;

    if (!vid_W3DContext(context, context->Buffers[context->BufNr]->sb_BitMap, modeid))
    {
        vid_CloseDisplay(context);
        return GL_FALSE;
    }

    if (!vid_W3DDefaults(context))
    {
        vid_CloseDisplay(context);
        return GL_FALSE;
    }

    SetRGB32(&context->w3dScreen->ViewPort, 0, 0x7fffffff, 0x7fffffff, 0x7fffffff);

    for (i = 0; i < context->NumBuffers; i++)
    {
        context->Buffers[i]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = NULL;

        while (!ChangeScreenBuffer(context->w3dScreen, context->Buffers[i]))
            ;

        EraseRect(context->w3dWindow->RPort, context->w3dWindow->BorderLeft, context->w3dWindow->BorderTop, context->w3dWindow->Width - context->w3dWindow->BorderLeft - context->w3dWindow->BorderRight - 1, context->w3dWindow->Height - context->w3dWindow->BorderTop - context->w3dWindow->BorderBottom - 1);
    }

    context->Buffers[0]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = NULL;

    while (!ChangeScreenBuffer(context->w3dScreen, context->Buffers[0]))
        ;

    GLScissor(context, 0, 0, context->w3dContext->width, context->w3dContext->height);

    vid_ScissorFull(context, &full);

    W3D_SetDrawRegion(context->w3dContext, context->NumBuffers > 1 ? context->Buffers[context->BufNr]->sb_BitMap : context->w3dScreen->RastPort.BitMap, 0, &full);
    W3D_SetScissor(context->w3dContext, &full);

    vid_Pointer(context->w3dWindow);

    return GL_TRUE;
}

static GLboolean vid_OpenWindow(GLcontext context, int w, int h)
{
    context->w3dScreen = LockPubScreen(NULL);

    if (!context->w3dScreen)
        return GL_FALSE;

    context->pubLocked = GL_TRUE;

    struct BitMap *bm = context->w3dScreen->RastPort.BitMap;

    if (GetBitMapAttr(bm, BMA_DEPTH) <= 8 && bm->Planes[1])
    {
        vid_CloseWindow(context);

        return GL_FALSE;
    }

    mgl_pubwintags[0].ti_Data = (ULONG)context->w3dScreen;
    mgl_pubwintags[1].ti_Data = (ULONG)w;
    mgl_pubwintags[2].ti_Data = (ULONG)h;

    context->w3dWindow = OpenWindowTagList(NULL, mgl_pubwintags);

    UnlockPubScreen(NULL, context->w3dScreen);

    context->pubLocked = GL_FALSE;

    if (!context->w3dWindow)
    {
        vid_CloseWindow(context);
        return GL_FALSE;
    }

    context->BufNr = 0;
    context->NumBuffers = 0;
    context->DoSync = GL_TRUE;

    if (!vid_BitMapTarget(context, w, h, context->w3dWindow->RPort->BitMap))
        return GL_FALSE;

    return GL_TRUE;
}

static GLboolean vid_BitMapTarget(GLcontext context, int w, int h, struct BitMap *friend)
{
    struct BitMap *screenbm;
    LONG depth;
    W3D_Scissor full;

    if (friend && !GetCyberMapAttr(friend, CYBRMATTR_ISCYBERGFX))
    {
        vid_CloseWindow(context);
        return GL_FALSE;
    }

    screenbm = context->w3dScreen ? context->w3dScreen->RastPort.BitMap : friend;
    depth = screenbm ? (LONG)GetBitMapAttr(screenbm, BMA_DEPTH) : newPixelDepth;

    context->w3dBitMap = AllocBitMap(w, h, (ULONG)depth, BMF_MINPLANES | BMF_DISPLAYABLE, friend);

    if (!context->w3dBitMap)
        context->w3dBitMap = AllocBitMap(w, h, (ULONG)depth, BMF_MINPLANES, friend);

    if (!context->w3dBitMap)
    {
        vid_CloseWindow(context);
        return GL_FALSE;
    }

    context->w3dRastPort = malloc(sizeof(struct RastPort));

    if (!context->w3dRastPort)
    {
        vid_CloseWindow(context);
        return GL_FALSE;
    }

    InitRastPort(context->w3dRastPort);

    context->w3dRastPort->BitMap = context->w3dBitMap;

    if (!vid_W3DContext(context, context->w3dBitMap, INVALID_ID))
    {
        vid_CloseWindow(context);
        return GL_FALSE;
    }

    if (!vid_W3DDefaults(context))
    {
        vid_CloseWindow(context);
        return GL_FALSE;
    }

    GLScissor(context, 0, 0, w, h);

    vid_ScissorFull(context, &full);

    W3D_SetDrawRegion(context->w3dContext, context->w3dBitMap, 0, &full);
    W3D_SetScissor(context->w3dContext, &full);

    return GL_TRUE;
}

// window owned by the caller, never closed or unlocked by us
static GLboolean vid_OpenExistingWindow(GLcontext context, struct Window *window, int *outw, int *outh)
{
    int w;
    int h;

    if (!window || !window->RPort || !window->RPort->BitMap)
        return GL_FALSE;

    w = window->Width - window->BorderLeft - window->BorderRight;
    h = window->Height - window->BorderTop - window->BorderBottom;

    if (w <= 0 || h <= 0)
        return GL_FALSE;

    context->externalWindow = GL_TRUE;
    context->w3dWindow = window;
    context->w3dScreen = window->WScreen;
    context->BufNr = 0;
    context->NumBuffers = 0;
    context->DoSync = GL_TRUE;

    if (GetBitMapAttr(window->RPort->BitMap, BMA_DEPTH) <= 8 && window->RPort->BitMap->Planes[1])
        return GL_FALSE;

    if (!vid_BitMapTarget(context, w, h, window->RPort->BitMap))
        return GL_FALSE;

    if (outw)
        *outw = w;

    if (outh)
        *outh = h;

    return GL_TRUE;
}

GLboolean MGLInitContext(GLcontext context)
{
    int i;
    W3D_Context *w3d;

    w3d = context->w3dContext;

    context->CurrentPrimitive = GL_BASE;
    context->CurrentError = GL_NO_ERROR;

    context->WBuffer = NULL;

    // clip-produced verts live past VertexBufferSize, index lists are two of cap
    context->VertexBuffer = malloc(sizeof(MGLVertex) * (newVertexBufferSize + MGL_CLIP_SLACK));
    context->ClipIdx = malloc(sizeof(UWORD) * 2 * (newVertexBufferSize + MGL_CLIP_SLACK));
    context->IdentIdx = malloc(sizeof(UWORD) * (newVertexBufferSize + MGL_CLIP_SLACK));
    context->EdgeIdx = malloc(sizeof(UWORD) * 2 * (newVertexBufferSize + MGL_CLIP_SLACK));
    context->IdxBuf = malloc(sizeof(UWORD) * MGL_IDXBUF_MULT * (newVertexBufferSize + MGL_CLIP_SLACK));
    context->NormalBuffer = malloc(sizeof(MGLNormal) * newVertexBufferSize * 2);
    context->EyeScratch = malloc(sizeof(MGLEye) * (newVertexBufferSize + MGL_CLIP_SLACK));
    context->MeScratch = malloc(sizeof(MGLMaterial) * 2 * (newVertexBufferSize + MGL_CLIP_SLACK));
    context->LitBack = malloc(sizeof(W3D_Color) * (newVertexBufferSize + MGL_CLIP_SLACK));
    context->LitSpec = malloc(sizeof(W3D_Color) * (newVertexBufferSize + MGL_CLIP_SLACK));
    context->LitSpecB = malloc(sizeof(W3D_Color) * (newVertexBufferSize + MGL_CLIP_SLACK));
    context->LitSave = malloc(sizeof(W3D_Color) * (newVertexBufferSize + MGL_CLIP_SLACK));
    context->w3dTexBuffer = malloc(sizeof(W3D_Texture *) * newTextureBufferSize);
    context->GeneratedTextures = malloc(sizeof(GLubyte) * newTextureBufferSize);
    context->w3dTexMemory = malloc(sizeof(GLubyte *) * newTextureBufferSize);
    context->PaletteData = malloc(MGL_PALETTE_BYTES);

    if (!context->VertexBuffer || !context->ClipIdx || !context->IdentIdx || !context->EdgeIdx || !context->IdxBuf || !context->NormalBuffer || !context->EyeScratch || !context->MeScratch || !context->LitBack || !context->LitSpec || !context->LitSpecB || !context->LitSave || !context->w3dTexBuffer || !context->GeneratedTextures || !context->w3dTexMemory || !context->PaletteData)
        return GL_FALSE;

    for (i = 0; i < newVertexBufferSize + MGL_CLIP_SLACK; i++)
        context->IdentIdx[i] = (UWORD)i;

    context->VertexBufferSize = (GLuint)newVertexBufferSize;
    context->VertexBufferPointer = 0;
    context->EmitBase = 0;
    context->StripBase = 0;
    context->SmartLock = GL_FALSE;
    context->NormalBufferPointer = 0;

    context->NormalBuffer[0].x = 0.0f;
    context->NormalBuffer[0].y = 0.0f;
    context->NormalBuffer[0].z = 1.0f;
    context->NormalBuffer[0].w = 0.0f;
    context->CurrentNormal.x = 0.0f;
    context->CurrentNormal.y = 0.0f;
    context->CurrentNormal.z = 1.0f;

    context->TexBufferSize = newTextureBufferSize;
    context->CurrentBinding = 0;
    context->NextTexture = 1;
    context->TexMemCur = 0;
    context->TexMemPeak = 0;
    context->ArrayTexBound = GL_FALSE;
    context->ActiveTexture = 0;
    context->ClientActiveTexture = 0;
    context->TCopyMax = 1;
    context->TexUnits = mgl_texunits();

    for (i = 0; i < newTextureBufferSize; i++)
    {
        context->w3dTexBuffer[i] = NULL;
        context->w3dTexMemory[i] = NULL;
        context->GeneratedTextures[i] = 0;
    }

    context->CurrentTexS = 0.0f;
    context->CurrentTexT = 0.0f;
    context->CurrentTexQ = 1.0f;
    context->CurrentTexQValid = GL_FALSE;

    context->PackAlign = 4;
    context->UnpackAlign = 4;

    context->AlphaTest_State = GL_FALSE;
    context->Blend_State = GL_FALSE;
    context->TextureGenS_State = GL_FALSE;
    context->TextureGenT_State = GL_FALSE;
    context->Fog_State = GL_FALSE;
    context->Scissor_State = GL_FALSE;
    context->CullFace_State = GL_FALSE;
    context->DepthTest_State = GL_FALSE;
    context->PointSmooth_State = GL_FALSE;
    context->Dither_State = GL_TRUE;

    context->FogDirty = GL_FALSE;
    context->FogStart = 0.0;
    context->FogEnd = 1.0;
    context->FogP43 = 0.0f;
    context->FogP44 = 0.0f;

    for (i = 0; i < MAX_TEXUNIT; i++)
    {
        context->Texture2D_State[i] = GL_FALSE;
        context->TexEnv[i] = GL_MODULATE;
        context->VirtualBinding[i] = 0;
        context->CurrentTcoord[i].s = 0.0f;
        context->CurrentTcoord[i].t = 0.0f;
        context->TexEnvColor[i].r = 0.0f;
        context->TexEnvColor[i].g = 0.0f;
        context->TexEnvColor[i].b = 0.0f;
        context->TexEnvColor[i].a = 0.0f;
    }

    context->CurTexEnv = 0;

    memset(&context->CurTexEnvColor, 0, sizeof(context->CurTexEnvColor));

    context->MinFilter = GL_NEAREST;
    context->MagFilter = GL_NEAREST;
    context->WrapS = GL_REPEAT;
    context->WrapT = GL_REPEAT;
    context->TexGenModeS = GL_EYE_LINEAR;
    context->TexGenModeT = GL_EYE_LINEAR;

    light_SetDefaults(context);

    memset(context->ObjectPlaneS, 0, sizeof(context->ObjectPlaneS));
    memset(context->ObjectPlaneT, 0, sizeof(context->ObjectPlaneT));
    memset(context->EyePlaneS, 0, sizeof(context->EyePlaneS));
    memset(context->EyePlaneT, 0, sizeof(context->EyePlaneT));

    context->ObjectPlaneS[0] = 1.0f;
    context->ObjectPlaneT[1] = 1.0f;

    context->w3dFog.fog_start = 1.0f;
    context->w3dFog.fog_end = 0.1f;
    context->w3dFog.fog_density = 1.0f;
    context->w3dFog.fog_color.r = 0.0f;
    context->w3dFog.fog_color.g = 0.0f;
    context->w3dFog.fog_color.b = 0.0f;
    context->w3dFogMode = W3D_FOG_EXP;
    context->CurFogMode = GL_EXP;
    context->FogRange = 1.0f;
    context->FogMult = 1.0f;

    context->CurrentCullFace = GL_BACK;
    context->CurrentFrontFace = GL_CCW;

    context->CurrentColor.r = 1.0f;
    context->CurrentColor.g = 1.0f;
    context->CurrentColor.b = 1.0f;
    context->CurrentColor.a = 1.0f;
    context->UpdateCurrentColor = GL_TRUE;

    context->ShadeModel = GL_SMOOTH;
    context->DepthMask = GL_TRUE;
    context->ClearDepth = 1.0;
    context->ClearColor = 0;

#ifdef AUTOMATIC_LOCKING_ENABLE
    context->LockMode = MGL_LOCK_MANUAL;
#endif

    context->NoMipMapping = newNoMipMapping;
    context->NoFallbackAlpha = newNoFallbackAlpha;

    context->Idle = NULL;
    context->MouseHandler = NULL;
    context->SpecialHandler = NULL;
    context->KeyHandler = NULL;

    context->SrcAlpha = 0;
    context->DstAlpha = 0;
    context->AlphaFellBack = GL_FALSE;
    context->WOne_Hint = GL_TRUE;
    context->HintPersp = GL_DONT_CARE;
    context->HintPoint = GL_DONT_CARE;
    context->HintLine = GL_DONT_CARE;
    context->HintPoly = GL_DONT_CARE;
    context->HintFog = GL_DONT_CARE;
    context->FixpointTrans_Hint = GL_FALSE;
    context->ZOffset = 0.0f;
    context->PolygonOffsetFactor = 0.0f;
    context->PolygonOffsetUnits = 0.0f;
    context->PolygonOffsetFill_State = GL_FALSE;
    context->DepthBufferUnit = 0.0f;

    context->PaletteSize = 0;
    context->PaletteFormat = 0;

    context->CurPolygonMode = GL_FILL;
    context->CurBlendSrc = GL_ONE;
    context->CurBlendDst = GL_ZERO;
    context->CurUnpackRowLength = 0;
    context->CurUnpackSkipPixels = 0;
    context->CurUnpackSkipRows = 0;
    context->CurWriteMask = GL_TRUE;
    context->CurDepthTest = GL_FALSE;
    context->CurDepthFunc = GL_LESS;

    context->CurDrawBuffer = GL_BACK;
    context->CurReadBuffer = GL_BACK;
    context->CurAlphaFunc = GL_ALWAYS;
    context->CurAlphaRef = 0.0f;
    context->StencilFunc = GL_ALWAYS;
    context->StencilValueMask = ~0u;
    context->StencilWriteMask = ~0u;
    context->StencilSfail = GL_KEEP;
    context->StencilZfail = GL_KEEP;
    context->StencilZpass = GL_KEEP;
    context->LogicOp = GL_COPY;
    context->EdgeFlagCur = GL_TRUE;

    context->RasterPos[0] = 0.0f;
    context->RasterPos[1] = 0.0f;
    context->RasterPos[2] = 0.0f;
    context->RasterPos[3] = 1.0f;
    context->RasterPosValid = GL_TRUE;
    context->PixelZoomX = 1.0f;
    context->PixelZoomY = 1.0f;
    context->CurColorMask = 0xF;
    context->CurPackRowLength = 0;
    context->CurPackSkipPixels = 0;
    context->CurPackSkipRows = 0;

    context->ClientState = 0;
    context->TexCSMask = 0;
    context->VertexArrayPipeline = GL_TRUE;

    // client arrays start unbound, the internal stream layout gets re-derived from vb
    context->ArrayPointer.colors = NULL;
    context->ArrayPointer.colorstride = 0;
    context->ArrayPointer.colormode = W3D_COLOR_FLOAT | W3D_CMODE_RGBA;

    memset(context->ArrayPointer.ctex, 0, sizeof(context->ArrayPointer.ctex));

    context->ArrayPointer.w_buffer = NULL;
    context->ArrayPointer.w_off = -4;
    context->ArrayPointer.verts = NULL;
    context->ArrayPointer.vertexstride = 0;
    context->ArrayPointer.vertexmode = W3D_VERTEX_F_F_D;
    context->ArrayPointer.lockfirst = 0;
    context->ArrayPointer.locksize = 0;
    context->ArrayPointer.transformed = 0;
    context->ArrayPointer.state = 0;

    context->CurrentDraw = NULL;

    W3D_VertexPointer(w3d, context->VertexBuffer, sizeof(MGLVertex), W3D_VERTEX_F_F_D, 0);
    W3D_TexCoordPointer(w3d, &context->VertexBuffer[0].v.u, sizeof(MGLVertex), 0, 4, -4, W3D_TEXCOORD_NORMALIZED);
    W3D_ColorPointer(w3d, &context->VertexBuffer[0].v.color, sizeof(MGLVertex), W3D_COLOR_FLOAT, W3D_CMODE_RGBA, 0);

    W3D_BindTexture(w3d, 0, NULL);

    context->LineWidth = 1.0f;
    context->MinTriArea = 0.5f;
    context->CurrentPointSize = 1.0f;

    context->ClipFlags = (MGL_CLIP_NEGW | MGL_CLIP_LEFT | MGL_CLIP_RIGHT | MGL_CLIP_TOP | MGL_CLIP_BOTTOM | MGL_CLIP_FRONT | MGL_CLIP_BACK);
    context->GuardBand = newGuardBand;

    GLMatrixInit(context);

    return GL_TRUE;
}

static GLcontext vid_FinishCreate(GLcontext context, int offx, int offy, int w, int h)
{
    if (!MGLInitContext(context))
    {
        MGLDeleteContext(context);
        return NULL;
    }

    GLDepthRange(context, 0.0, 1.0);
    GLViewport(context, offx, offy, w, h);
    GLClearColor(context, 0.0f, 0.0f, 0.0f, 0.0f);

    context->w3dChipID = context->w3dContext->CurrentChip;

    return context;
}

void *MGLCreateContext(int offx, int offy, int w, int h)
{
    GLcontext context;

    if (!mglEnsureVidLibs())
        return NULL;

    context = malloc(sizeof(struct GLcontext_t));

    if (!context)
        return NULL;

    memset(context, 0, sizeof(struct GLcontext_t));

    if ((newWindowMode == GL_FALSE && !vid_OpenDisplay(context, w, h, MGL_SM_BESTMODE)) || (newWindowMode != GL_FALSE && !vid_OpenWindow(context, w, h)))
    {
        free(context);
        return NULL;
    }

    return vid_FinishCreate(context, offx, offy, context->w3dContext->width, context->w3dContext->height);
}

void *MGLCreateContextFromID(GLint id, GLint *width, GLint *height)
{
    GLcontext context;
    int w;
    int h;

    if (!mglEnsureVidLibs())
        return NULL;

    context = malloc(sizeof(struct GLcontext_t));

    if (!context)
        return NULL;

    memset(context, 0, sizeof(struct GLcontext_t));

    if (id == MGL_SM_WINDOWMODE)
        newWindowMode = GL_TRUE;

    if (newWindowMode != GL_FALSE || !vid_OpenDisplay(context, -1, -1, (ULONG)id))
    {
        free(context);
        return NULL;
    }

    w = context->w3dScreen->Width;
    h = context->w3dScreen->Height;

    if (width)
        *width = w;

    if (height)
        *height = h;

    return vid_FinishCreate(context, 0, 0, w, h);
}

void *MGLCreateContextFromWindow(struct Window *window)
{
    GLcontext context;
    int w;
    int h;

    if (!window || !mglEnsureVidLibs())
        return NULL;

    context = malloc(sizeof(struct GLcontext_t));

    if (!context)
        return NULL;

    memset(context, 0, sizeof(struct GLcontext_t));

    if (!vid_OpenExistingWindow(context, window, &w, &h))
    {
        free(context);
        return NULL;
    }

    return vid_FinishCreate(context, 0, 0, w, h);
}

// borrowed CGX bitmap, presentation and lifetime belong to the caller
void *MGLCreateContextFromBitMap(struct BitMap *bitmap)
{
    GLcontext context;
    ULONG w, h, bpp;

    if (!bitmap || !mglEnsureVidLibs())
        return NULL;

    if (!GetCyberMapAttr(bitmap, CYBRMATTR_ISCYBERGFX))
        return NULL;

    w = GetCyberMapAttr(bitmap, CYBRMATTR_WIDTH);
    h = GetCyberMapAttr(bitmap, CYBRMATTR_HEIGHT);
    bpp = GetCyberMapAttr(bitmap, CYBRMATTR_BPPIX);

    if (!w || !h || bpp < 2)
        return NULL;

    context = malloc(sizeof(struct GLcontext_t));

    if (!context)
        return NULL;

    memset(context, 0, sizeof(struct GLcontext_t));

    context->externalWindow = GL_TRUE;
    context->externalBitMap = GL_TRUE;
    context->w3dBitMap = bitmap;
    context->NumBuffers = 0;
    context->DoSync = GL_FALSE;

    if (!vid_W3DContext(context, bitmap, INVALID_ID))
    {
        MGLDeleteContext(context);
        return NULL;
    }

    if (!vid_W3DDefaults(context))
    {
        MGLDeleteContext(context);
        return NULL;
    }

    GLScissor(context, 0, 0, w, h);

    W3D_Scissor full;

    vid_ScissorFull(context, &full);

    W3D_SetDrawRegion(context->w3dContext, bitmap, 0, &full);
    W3D_SetScissor(context->w3dContext, &full);

    return vid_FinishCreate(context, 0, 0, w, h);
}

void MGLDeleteContext(GLcontext context)
{
    if (context->w3dBitMap)
        vid_CloseWindow(context);
    else
        vid_CloseDisplay(context);

    if (context->NormalBuffer)
        free(context->NormalBuffer);

    if (context->EyeScratch)
        free(context->EyeScratch);

    if (context->MeScratch)
        free(context->MeScratch);

    if (context->LitBack)
        free(context->LitBack);

    if (context->LitSpec)
        free(context->LitSpec);

    if (context->LitSpecB)
        free(context->LitSpecB);

    if (context->LitSave)
        free(context->LitSave);

    if (context->WBuffer)
        free(context->WBuffer);

    if (context->ClipIdx)
        free(context->ClipIdx);

    if (context->IdentIdx)
        free(context->IdentIdx);

    if (context->EdgeIdx)
        free(context->EdgeIdx);

    if (context->IdxBuf)
        free(context->IdxBuf);

    if (context->VertexBuffer)
        free(context->VertexBuffer);

    if (context->w3dTexBuffer)
        free(context->w3dTexBuffer);

    if (context->GeneratedTextures)
        free(context->GeneratedTextures);

    if (context->w3dTexMemory)
        free(context->w3dTexMemory);

    if (context->PaletteData)
        free(context->PaletteData);

    if (mini_CurrentContext == context)
        mini_CurrentContext = (GLcontext)0;

    free(context);
}

void *MGLGetWindowHandle(GLcontext context)
{
    return context->w3dWindow;
}

// intuition-side cleanup only
void MGLEmergencyCloseScreen(GLcontext context)
{
    int i;

    if (context->externalWindow)
        return;

    if (context->w3dWindow)
    {
        CloseWindow(context->w3dWindow);
        context->w3dWindow = NULL;
    }

    if (context->w3dBitMap)
    {
        if (context->pubLocked)
        {
            UnlockPubScreen(NULL, context->w3dScreen);

            context->pubLocked = GL_FALSE;
            context->w3dScreen = NULL;
        }

        return;
    }

    if (context->w3dScreen)
    {
        if (context->Buffers[0])
        {
            context->Buffers[0]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = NULL;

            ChangeScreenBuffer(context->w3dScreen, context->Buffers[0]);
        }

        for (i = 0; i < context->NumBuffers; i++)
        {
            if (context->Buffers[i])
            {
                FreeScreenBuffer(context->w3dScreen, context->Buffers[i]);
                context->Buffers[i] = NULL;
            }
        }

        CloseScreen(context->w3dScreen);
        context->w3dScreen = NULL;
    }
}

void mglChooseGuardBand(GLboolean flag)
{
    newGuardBand = flag;
}

void mglChooseVertexBufferSize(int size)
{
    // UWORD index space plus the clip slack caps the usable size
    if (size < 8)
        size = 8;

    if (size > 0x10000 - MGL_CLIP_SLACK)
        size = 0x10000 - MGL_CLIP_SLACK;

    newVertexBufferSize = size;
}

void mglChooseMtexBufferSize(int size)
{
    if (size <= 0)
        return;

    newMtexBufferSize = size;
}

void mglChooseTextureBufferSize(int size)
{
    if (size > 0)
        newTextureBufferSize = size;
}

void mglChooseNumberOfBuffers(int number)
{
    if (number < 1)
        number = 1;

    if (number > 3)
        number = 3;

    newNumberOfBuffers = number;
}

void mglChoosePixelDepth(int depth)
{
    newPixelDepth = depth;
}

void mglProposeCloseDesktop(GLboolean closeme)
{
    clw = closeme;
}

void mglProhibitMipMapping(GLboolean flag)
{
    newNoMipMapping = flag;
}

void mglProhibitAlphaFallback(GLboolean flag)
{
    newNoFallbackAlpha = flag;
}

void mglChooseWindowMode(GLboolean flag)
{
    newWindowMode = flag;
}

void GLClearColor(GLcontext context, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    red = red < 0.0f ? 0.0f : red > 1.0f ? 1.0f
                                         : red;
    green = green < 0.0f ? 0.0f : green > 1.0f ? 1.0f
                                               : green;
    blue = blue < 0.0f ? 0.0f : blue > 1.0f ? 1.0f
                                            : blue;
    alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f
                                               : alpha;

    context->ClearColor = ((ULONG)(GLubyte)(alpha * 255.0f) << 24) | ((ULONG)(GLubyte)(red * 255.0f) << 16) | ((ULONG)(GLubyte)(green * 255.0f) << 8) | (ULONG)(GLubyte)(blue * 255.0f);
}

void GLDepthMask(GLcontext context, GLboolean flag)
{
    W3D_SetState(context->w3dContext, W3D_ZBUFFERUPDATE, (flag && context->DepthTest_State) ? W3D_ENABLE : W3D_DISABLE);

    context->DepthMask = flag;
    context->CurWriteMask = flag;
}

void GLDepthFunc(GLcontext context, GLenum func)
{
    const ULONG wfunc = mgl_depthw(func);

    if (!wfunc)
    {
        mgl_error(context, GL_INVALID_ENUM);
        return;
    }

    context->CurDepthFunc = func;

    W3D_SetZCompareMode(context->w3dContext, wfunc);
}

void GLClearDepth(GLcontext context, GLclampd depth)
{
    context->ClearDepth = (W3D_Double)(depth < 0.0 ? 0.0 : depth > 1.0 ? 1.0
                                                                       : depth);
}

void GLClear(GLcontext context, GLbitfield mask)
{
    GLboolean locked;

    /* stencil bit accepted, clear waits for its phase */
    if (mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))
    {
        mgl_error(context, GL_INVALID_VALUE);
        return;
    }

    if (!mask)
        return;

    locked = context->w3dLocked;

    if (!locked && W3D_LockHardware(context->w3dContext) != W3D_SUCCESS)
        return;

    if (mask & GL_COLOR_BUFFER_BIT)
        W3D_ClearDrawRegion(context->w3dContext, context->ClearColor);

    if (mask & GL_DEPTH_BUFFER_BIT)
        W3D_ClearZBuffer(context->w3dContext, &context->ClearDepth);

    if ((mask & GL_STENCIL_BUFFER_BIT) && context->StencilBits)
    {
        const ULONG sv = (ULONG)context->StencilClear;

        W3D_ClearStencilBuffer(context->w3dContext, (ULONG *)&sv);
    }

    if (!locked)
        W3D_UnLockHardware(context->w3dContext);
}

void MGLLockMode(GLcontext context, GLenum lockMode)
{
#ifdef AUTOMATIC_LOCKING_ENABLE
    context->LockMode = lockMode;
#endif
}

GLboolean MGLLockBack(GLcontext context, MGLLockInfo *info)
{
    ULONG error = W3D_SUCCESS;

    if (!context->w3dLocked)
        error = W3D_LockHardware(context->w3dContext);

    if (error != W3D_SUCCESS)
        return GL_FALSE;

    context->w3dLocked = GL_TRUE;

#ifdef AUTOMATIC_LOCKING_ENABLE
    TMA_Start(&context->LockTime);
#endif

    if (info)
    {
        info->width = context->w3dContext->width;
        info->height = context->w3dContext->height;
        info->depth = context->w3dContext->depth;
        info->pixel_format = context->w3dContext->format;
        info->base_address = context->w3dContext->drawmem;
        info->pitch = context->w3dContext->bprow;
    }

    return GL_TRUE;
}

GLboolean MGLLockDisplay(GLcontext context)
{
    if (context->w3dLocked)
        return GL_TRUE;

#ifdef AUTOMATIC_LOCKING_ENABLE
    switch (context->LockMode)
    {
    case MGL_LOCK_AUTOMATIC:
    case MGL_LOCK_SMART:
        return GL_TRUE;

    default:
        break;
    }
#endif

    if (W3D_LockHardware(context->w3dContext) != W3D_SUCCESS)
        return GL_FALSE;

    context->w3dLocked = GL_TRUE;

    return GL_TRUE;
}

void MGLUnlockDisplay(GLcontext context)
{
    if (!context->w3dLocked)
        return;

#ifdef AUTOMATIC_LOCKING_ENABLE
    if (context->LockMode == MGL_LOCK_AUTOMATIC)
        // stays locked, w3dLocked keeps reporting the real state
        return;
#endif

    W3D_UnLockHardware(context->w3dContext);

    context->w3dLocked = GL_FALSE;
    context->SmartLock = GL_FALSE;
}

void MGLEnableSync(GLcontext context, GLboolean enable)
{
    context->DoSync = enable;
}

void MGLSwitchDisplay(GLcontext context)
{
    int nowbuf;
    W3D_Scissor full;
    W3D_Scissor *eff;

    W3D_Flush(context->w3dContext);
    W3D_FlushFrame(context->w3dContext);

    if (context->externalBitMap)
    {
        GLFinish(context);

        MGLUnlockDisplay(context);
        return;
    }

    nowbuf = context->BufNr;

    MGLUnlockDisplay(context);

    if (!context->w3dBitMap)
    {
        // fullscreen, rotate draw buffers
        context->BufNr++;

        if (context->BufNr >= context->NumBuffers)
            context->BufNr = 0;

        context->Buffers[nowbuf]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = NULL;

        while (!ChangeScreenBuffer(context->w3dScreen, context->Buffers[nowbuf]))
            ;

        if (context->Scissor_State)
        {
            eff = &context->scissor;
        }
        else
        {
            vid_ScissorFull(context, &full);

            eff = &full;
        }

        W3D_SetDrawRegion(context->w3dContext, context->Buffers[context->BufNr]->sb_BitMap, 0, eff);

        if (context->DoSync)
            WaitBOVP(&context->w3dScreen->ViewPort);
    }
    else
    {
        // windowed, present the offscreen bitmap
        W3D_WaitIdle(context->w3dContext);

        BltBitMapRastPort(context->w3dBitMap, 0, 0, context->w3dWindow->RPort, context->w3dWindow->BorderLeft, context->w3dWindow->BorderTop, context->w3dWindow->Width - context->w3dWindow->BorderLeft - context->w3dWindow->BorderRight, context->w3dWindow->Height - context->w3dWindow->BorderTop - context->w3dWindow->BorderBottom, MGL_MIN_COPY);
    }
}

void MGLSetState(GLcontext context, GLenum cap, GLboolean flag)
{
    W3D_Scissor full;
    ULONG ed;
    int active;

    active = context->ActiveTexture;
    ed = flag ? W3D_ENABLE : W3D_DISABLE;

    switch (cap)
    {
    case GL_POLYGON_OFFSET_FILL:
        context->PolygonOffsetFill_State = flag;
        context->LockStaged = GL_FALSE;
        context->LockXformed = GL_FALSE;
        break;

    case GL_ALPHA_TEST:
        context->AlphaTest_State = flag;

        W3D_SetState(context->w3dContext, W3D_ALPHATEST, ed);
        break;

    case GL_STENCIL_TEST:
        context->Stencil_State = flag;

        if (context->StencilBits)
            W3D_SetState(context->w3dContext, W3D_STENCILBUFFER, ed);

        break;

    case GL_COLOR_LOGIC_OP:
        context->LogicOp_State = flag;

        if (context->LogicOpOK)
            W3D_SetState(context->w3dContext, W3D_LOGICOP, ed);

        break;

    case GL_BLEND:
        context->Blend_State = flag;

        W3D_SetState(context->w3dContext, W3D_BLENDING, ed);
        break;

    case GL_TEXTURE_2D:
        context->Texture2D_State[active] = flag;

        if (active == 0)
            W3D_SetState(context->w3dContext, W3D_TEXMAPPING, (flag && context->w3dTexBuffer[context->CurrentBinding]) ? W3D_ENABLE : W3D_DISABLE);
        else if (flag && active > context->TCopyMax)
            context->TCopyMax = (GLubyte)active;

        break;

    case GL_SCISSOR_TEST:
        context->Scissor_State = flag;

        if (!flag)
            vid_ScissorFull(context, &full);

        W3D_SetScissor(context->w3dContext, flag ? &context->scissor : &full);
        W3D_SetState(context->w3dContext, W3D_SCISSOR, ed);

        break;

    case GL_CULL_FACE:
        context->CullFace_State = flag;

        context->CullHW = (W3D_SetState(context->w3dContext, W3D_CULLFACE, ed) == W3D_SUCCESS) && flag && context->CurrentCullFace != GL_FRONT_AND_BACK;

        cl_FrontOrder(context);
        break;

    case GL_DEPTH_WRITEMASK:
        context->CurWriteMask = flag;
        context->DepthMask = flag;

        W3D_SetState(context->w3dContext, W3D_ZBUFFERUPDATE, (flag && context->DepthTest_State) ? W3D_ENABLE : W3D_DISABLE);
        break;

    case GL_DEPTH_TEST:
        context->CurDepthTest = flag;
        context->DepthTest_State = flag;

        W3D_SetState(context->w3dContext, W3D_ZBUFFER, ed);

        if (context->DepthMask)
            W3D_SetState(context->w3dContext, W3D_ZBUFFERUPDATE, ed);

        break;

    case GL_FOG:
        context->FogDirty = GL_TRUE;
        context->Fog_State = flag;

        W3D_SetState(context->w3dContext, W3D_FOGGING, ed);
        break;

    case GL_TEXTURE_GEN_S:
        context->TextureGenS_State = flag;
        break;

    case GL_TEXTURE_GEN_T:
        context->TextureGenT_State = flag;
        break;

    case GL_LIGHTING:
        context->Lighting_State = flag;
        break;

    case GL_COLOR_MATERIAL:
        context->ColorMaterial_State = flag;
        break;

    case GL_NORMALIZE:
        context->Normalize_State = flag;
        break;

    case GL_RESCALE_NORMAL:
        context->Rescale_State = flag;
        break;

    case GL_DITHER:
        context->Dither_State = flag;

        W3D_SetState(context->w3dContext, W3D_DITHERING, ed);
        break;

    case GL_POINT_SMOOTH:
        context->PointSmooth_State = flag;

        W3D_SetState(context->w3dContext, W3D_ANTI_POINT, ed);
        break;

    case MGL_PERSPECTIVE_MAPPING:
        W3D_SetState(context->w3dContext, W3D_PERSPECTIVE, ed);
        break;

    case MGL_ARRAY_TRANSFORMATIONS:
        if (context->VertexArrayPipeline != flag)
        {
            context->VertexArrayPipeline = flag;

            va_bindptr(context);
        }

        break;

    default:
        if (cap >= GL_LIGHT0 && cap <= GL_LIGHT7)
        {
            if (flag)
                context->LightMask |= (GLubyte)(1u << (cap - GL_LIGHT0));
            else
                context->LightMask &= (GLubyte) ~(1u << (cap - GL_LIGHT0));

            break;
        }

        mgl_error(context, GL_INVALID_ENUM);
        break;
    }
}

void MGLMinTriArea(GLcontext context, GLfloat area)
{
    context->MinTriArea = area;
}

GLint mglGetSupportedScreenModes(MGLScreenModeCallback CallbackFn)
{
    W3D_ScreenMode *modes;
    W3D_ScreenMode *cursor;
    MGLScreenMode smode;

    if (!mglEnsureVidLibs())
        return MGL_SM_BESTMODE;

    modes = W3D_GetScreenmodeList();

    if (!modes)
        return MGL_SM_BESTMODE;

    cursor = modes;

    while (cursor)
    {
        smode.id = (GLint)cursor->ModeID;
        smode.width = (GLint)cursor->Width;
        smode.height = (GLint)cursor->Height;
        smode.bit_depth = (GLint)cursor->Depth;
        smode.mode_name[0] = 0;

        strncpy(smode.mode_name, cursor->DisplayName, MGL_MAX_MODE - 1);

        smode.mode_name[MGL_MAX_MODE - 1] = 0;

        if (CallbackFn(&smode) == GL_TRUE)
        {
            W3D_FreeScreenmodeList(modes);
            return smode.id;
        }

        cursor = cursor->Next;
    }

    W3D_FreeScreenmodeList(modes);

    return MGL_SM_BESTMODE;
}

LONG mglQuery(GLcontext context, ULONG query, ULONG destfmt)
{
    if (!context)
        return W3D_NOT_SUPPORTED;

    switch (query)
    {
    case MGL_Q_VAO_MAX:
        return MGL_VAO_MAX;

    case MGL_Q_VAO_FREE:
    {
        GLuint i;
        LONG free = 0;

        for (i = 1; i <= MGL_VAO_MAX; i++)
            free += !context->VAOs[i].name;

        return free;
    }

    case MGL_Q_VB_SIZE:
        return (LONG)context->VertexBufferSize;

    case MGL_Q_MARKER_DEPTH:
        return (LONG)context->DebugMarkerDepth;

    case MGL_Q_LOGICOP:
        return (LONG)context->LogicOpOK;
    }

    if (!context->w3dContext || !Warp3DBase)
        return W3D_NOT_SUPPORTED;

    return W3D_Query(context->w3dContext, query, destfmt);
}

void *mglGetScreenmodeList(void)
{
    if (!mglEnsureVidLibs())
        return (void *)0;

    return (void *)W3D_GetScreenmodeList();
}

void mglFreeScreenmodeList(void *list)
{
    if (list && mglEnsureVidLibs())
        W3D_FreeScreenmodeList((W3D_ScreenMode *)list);
}

static W3D_Driver *vid_DriverForMode(ULONG modeID)
{
    W3D_Driver *drv = NULL;
    ULONG i;

    if (modeID != (ULONG)INVALID_ID)
        drv = W3D_TestMode(modeID);

    if (!drv)
    {
        W3D_Driver **list = W3D_GetDrivers();

        if (list)
        {
            for (i = 0; list[i]; i++)
            {
                if (!list[i]->swdriver)
                {
                    drv = list[i];
                    break;
                }
            }

            if (!drv)
                drv = list[0];
        }
    }

    return drv;
}

LONG mglGetModeDriverInfo(ULONG modeID, MGLModeDriverInfo *info)
{
    W3D_Driver *drv;

    if (!info || !mglEnsureVidLibs())
        return 0;

    drv = vid_DriverForMode(modeID);

    if (!drv)
        return 0;

    info->chipID = drv->ChipID;
    info->formats = drv->formats;
    info->swdriver = drv->swdriver;

    strncpy(info->name, drv->name ? drv->name : "?", sizeof(info->name) - 1);

    info->name[sizeof(info->name) - 1] = 0;

    return 1;
}

LONG mglQueryMode(ULONG modeID, ULONG query, ULONG destfmt)
{
    W3D_Driver *drv;

    if (!mglEnsureVidLibs() || !(drv = vid_DriverForMode(modeID)))
        return W3D_NOT_SUPPORTED;

    return W3D_QueryDriver(drv, query, destfmt);
}

ULONG mglModeTexFmt(ULONG modeID, ULONG texfmt, ULONG destfmt)
{
    W3D_Driver *drv;

    if (!mglEnsureVidLibs() || !(drv = vid_DriverForMode(modeID)))
        return W3D_TEXFMT_UNSUPPORTED;

    return W3D_GetDriverTexFmtInfo(drv, texfmt, destfmt);
}

#define MINIGL_LIBRARY_BUILD 1
#include <exec/types.h>
#include <libraries/minigl_dispatch.h>
#include "context_access.h"

/* Present in backend archives but not declared by every historical header. */
extern void MGLLockMode(GLcontext context, GLenum lockMode);
extern void mglChooseGuardBand(GLboolean flag);
extern void mglChooseMtexBufferSize(int size);
extern void mglChooseNumberOfBuffers(int number);
extern void mglChooseTextureBufferSize(int size);
extern void mglChooseVertexBufferSize(int size);
extern void mglChooseWindowMode(GLboolean flag);
extern GLint mglGetSupportedScreenModes(MGLScreenModeCallback CallbackFn);
extern void mglProhibitAlphaFallback(GLboolean flag);
extern void mglProhibitMipMapping(GLboolean flag);
extern void mglProposeCloseDesktop(GLboolean closeme);
extern LONG mglQuery(GLcontext context, ULONG query, ULONG destfmt);
extern void *mglGetScreenmodeList(void);
extern void mglFreeScreenmodeList(void *list);

/* Functions added by PiStorm3D but not present in Classic MiniGL. */
static void *ClassicStub_MGLGetInputWindowHandle(GLcontext context) { return (void *)0; }

/* The Classic Q3 backend declares MGLResizeContext, but its implementation in
 * context.c is disabled with #if 0 and therefore exports no such symbol. */
static GLboolean ClassicStub_MGLResizeContext(GLcontext context, GLsizei width, GLsizei height)
{
    (void)context;
    (void)width;
    (void)height;
    return GL_FALSE; /* V29 reports that no resize took place. */
}

/* Classic MiniGL has no GLLineWidth to dispatch to -- the entry point is new
 * in MiniGLV3D (2026-09-12, audit fix 4). Stubbed so the slot exists and the
 * two tables stay the same shape; classic lines stay one pixel wide. */
static void ClassicStub_GLLineWidth(GLcontext context, GLfloat width)
{
    (void)context;
    (void)width;
}

/* Nor the eleven entry points of GL 1.1 audit item 10 (2026-09-12). Classic's
 * own header, mgl_classic/gl.h, has the same shape MiniGLV3D's had: live
 * macros over commented-out prototypes, GLGetPointerv's alone left live.
 * The Classic library is not in this tree, so these follow its header and
 * stub out rather than name symbols its link may not have.
 *
 * The three that hand information back answer deterministically rather than
 * leaving the caller's memory as it was: no pointer, one zero. */
static void ClassicStub_GLGetDoublev(GLcontext context, GLenum pname, GLdouble *params)
{
    (void)context;
    (void)pname;
    if (params)
        params[0] = 0.0;
}

static void ClassicStub_GLGetPointerv(GLcontext context, GLenum pname, GLvoid **params)
{
    (void)context;
    (void)pname;
    if (params)
        *params = (GLvoid *)0;
}

static void ClassicStub_GLIndexi(GLcontext context, GLint c)
{
    (void)context;
    (void)c;
}

static void ClassicStub_GLIndexiv(GLcontext context, const GLint *c)
{
    (void)context;
    (void)c;
}

static void ClassicStub_GLIndexPointer(GLcontext context, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    (void)context;
    (void)type;
    (void)stride;
    (void)pointer;
}

/* Appended 2026-09-08. Classic MiniGL has GLInterleavedArrays but none of the
 * other three (no ARB multitexture client state, no glIsTexture, and
 * mglChooseZBufferDepth is a MiniGLV3D-only API), so they are stubbed. */

static GLboolean ClassicStub_GLIsTexture(GLcontext context, GLuint texture)
{
    (void)context;
    (void)texture;
    return GL_FALSE;
}

static void ClassicStub_mglChooseZBufferDepth(int bits) { (void)bits; }

/* Appended 2026-09-08. Classic MiniGL has neither glBlendEquation nor
 * glBlendFuncSeparate (verified against mgl_classic/gl.h), so both stub out.
 * Silently ignoring them degrades to plain ADD blending with the colour
 * factors on both channels -- which is exactly what Classic already does. */
static void ClassicStub_GLBlendEquation(GLcontext context, GLenum mode)
{
    (void)context;
    (void)mode;
}

static void ClassicStub_GLBlendFuncSeparate(GLcontext context, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    (void)srcAlpha;
    (void)dstAlpha;
    /* Best effort: apply the RGB pair through the API Classic does have,
     * rather than dropping the call entirely. */
    GLBlendFunc(context, srcRGB, dstRGB);
}

/* New unsupported core GL entry points report failure without changing state.
 * Do not use GLFlagError: Classic's release config compiles that macro out. */
static void ClassicUnsupported(GLcontext context)
{
    if (context && context->CurrentError == GL_NO_ERROR)
        context->CurrentError = GL_INVALID_OPERATION;
}

/* Stub bodies for the entries appended for 28.0 that MiniGL still lacks:
 * display lists, GLU quadrics and GLUT. Lighting is real since mglQ3 lit. */

static GLuint ClassicStub_GLGenLists(GLcontext context, GLsizei range)
{
    (void)context;
    (void)range;
    return 0;
}

static void ClassicStub_GLDeleteLists(GLcontext context, GLuint list, GLsizei range)
{
    (void)context;
    (void)list;
    (void)range;
}

static GLboolean ClassicStub_GLIsList(GLcontext context, GLuint list)
{
    (void)context;
    (void)list;
    return GL_FALSE;
}

static void ClassicStub_GLNewList(GLcontext context, GLuint list, GLenum mode)
{
    (void)context;
    (void)list;
    (void)mode;
    ClassicUnsupported(context);
}

static void ClassicStub_GLEndList(GLcontext context)
{
    (void)context;
    ClassicUnsupported(context);
}

static void ClassicStub_GLCallList(GLcontext context, GLuint list)
{
    (void)context;
    (void)list;
    ClassicUnsupported(context);
}

/* GLU error text does not require a renderer. */
static const GLubyte *Classic_GLUErrorString(GLenum errCode)
{
    switch (errCode)
    {
    case GL_NO_ERROR:
        return (const GLubyte *)"no error";
    case GL_INVALID_ENUM:
    case GLU_INVALID_ENUM:
        return (const GLubyte *)"invalid enum";
    case GL_INVALID_VALUE:
    case GLU_INVALID_VALUE:
        return (const GLubyte *)"invalid value";
    case GL_INVALID_OPERATION:
    case GLU_INVALID_OPERATION:
        return (const GLubyte *)"invalid operation";
    case GL_OUT_OF_MEMORY:
    case GLU_OUT_OF_MEMORY:
        return (const GLubyte *)"out of memory";
    case GL_STACK_OVERFLOW:
        return (const GLubyte *)"stack overflow";
    case GL_STACK_UNDERFLOW:
        return (const GLubyte *)"stack underflow";
    case GLU_INCOMPATIBLE_GL_VERSION:
        return (const GLubyte *)"incompatible GL version";
    default:
        return (const GLubyte *)0;
    }
}

static GLUquadricObj *ClassicStub_GLUNewQuadric(void) { return NULL; }

static void ClassicStub_GLUDeleteQuadric(GLUquadricObj *q) { (void)q; }

static void ClassicStub_GLUQuadricNormals(GLUquadricObj *q, GLenum normals)
{
    (void)q;
    (void)normals;
}

static void ClassicStub_GLUQuadricTexture(GLUquadricObj *q, GLboolean textureCoords)
{
    (void)q;
    (void)textureCoords;
}

static void ClassicStub_GLUQuadricDrawStyle(GLUquadricObj *q, GLenum drawStyle)
{
    (void)q;
    (void)drawStyle;
}

static void ClassicStub_GLUQuadricOrientation(GLUquadricObj *q, GLenum orientation)
{
    (void)q;
    (void)orientation;
}

static void ClassicStub_GLUCylinder(GLUquadricObj *q, GLdouble base, GLdouble top, GLdouble height, GLint slices, GLint stacks)
{
    (void)q;
    (void)base;
    (void)top;
    (void)height;
    (void)slices;
    (void)stacks;
}

static void ClassicStub_GLUSphere(GLUquadricObj *q, GLdouble radius, GLint slices, GLint stacks)
{
    (void)q;
    (void)radius;
    (void)slices;
    (void)stacks;
}

static void ClassicStub_GLUDisk(GLUquadricObj *q, GLdouble inner, GLdouble outer, GLint slices, GLint loops)
{
    (void)q;
    (void)inner;
    (void)outer;
    (void)slices;
    (void)loops;
}

static void ClassicStub_GLUTInit(int *argcp, char **argv)
{
    (void)argcp;
    (void)argv;
}

static void ClassicStub_GLUTInitDisplayMode(unsigned int mode) { (void)mode; }

static void ClassicStub_GLUTInitWindowSize(int width, int height)
{
    (void)width;
    (void)height;
}

static void ClassicStub_GLUTInitWindowPosition(int x, int y)
{
    (void)x;
    (void)y;
}

static int ClassicStub_GLUTCreateWindow(const char *title)
{
    (void)title;
    return 0;
}

static void ClassicStub_GLUTMainLoop(void) {}

static void ClassicStub_GLUTDisplayFunc(void (*func)(void)) { (void)func; }

static void ClassicStub_GLUTIdleFunc(void (*func)(void)) { (void)func; }

static void ClassicStub_GLUTKeyboardFunc(void (*func)(unsigned char key, int x, int y)) { (void)func; }

static void ClassicStub_GLUTReshapeFunc(void (*func)(int width, int height)) { (void)func; }

static void ClassicStub_GLUTSwapBuffers(void) {}

static void ClassicStub_GLUTPostRedisplay(void) {}

static int ClassicStub_GLUTGet(GLenum state)
{
    (void)state;
    return 0;
}

static void ClassicStub_GLUTGameModeString(const char *string) { (void)string; }

static int ClassicStub_GLUTEnterGameMode(void) { return 0; }

static void ClassicStub_GLUTLeaveGameMode(void) {}

static int ClassicStub_GLUTGameModeGet(GLenum query)
{
    (void)query;
    return 0;
}

static void ClassicStub_GLUTSolidCube(GLdouble size) { (void)size; }

static void ClassicStub_GLUTSolidSphere(GLdouble radius, GLint slices, GLint stacks)
{
    (void)radius;
    (void)slices;
    (void)stacks;
}

static void ClassicStub_GLUTSolidCone(GLdouble base, GLdouble height, GLint slices, GLint stacks)
{
    (void)base;
    (void)height;
    (void)slices;
    (void)stacks;
}

static void ClassicStub_GLUTSolidTorus(GLdouble innerRadius, GLdouble outerRadius, GLint sides, GLint rings)
{
    (void)innerRadius;
    (void)outerRadius;
    (void)sides;
    (void)rings;
}

static void ClassicStub_GLUTSolidDodecahedron(void) {}

static void ClassicStub_GLUQuadricCallback(GLUquadricObj *q, GLenum which, MGLUfuncptr fn)
{
    (void)q;
    (void)which;
    (void)fn;
}

const MGLDispatchTable MiniGLDispatchTable = {
    MINIGL_DISPATCH_ABI_VERSION,
    (ULONG)sizeof(MGLDispatchTable),
    MINIGL_BACKEND_FLAG_CLASSIC,
    0,
    &mini_CurrentContext,
    GLActiveTextureARB,
    GLAlphaFunc,
    GLArrayElement,
    GLBegin,
    GLBindTexture,
    GLBlendFunc,
    GLClear,
    GLClearColor,
    GLClearDepth,
    GLColor3fv,
    GLColor3ubv,
    GLColor4f,
    GLColor4fv,
    GLColor4ub,
    GLColor4ubv,
    GLColorMask,
    GLColorPointer,
    GLColorTable,
    GLCullFace,
    GLDeleteTextures,
    GLDepthFunc,
    GLDepthMask,
    GLDepthRange,
    GLDisableClientState,
    GLDrawArrays,
    GLDrawBuffer,
    GLDrawElements,
    GLEnableClientState,
    GLEnd,
    GLFinish,
    GLFlush,
    GLFogf,
    GLFogfv,
    GLFrontFace,
    GLFrustum,
    GLGenTextures,
    GLGetBooleanv,
    GLGetError,
    GLGetFloatv,
    GLGetIntegerv,
    GLGetString,
    GLHint,
    GLIsEnabled,
    GLLoadIdentity,
    GLLoadMatrixd,
    GLLoadMatrixf,
    GLLockArrays,
    GLMatrixMode,
    GLMultMatrixd,
    GLMultMatrixf,
    GLMultiTexCoord2fARB,
    GLMultiTexCoord2fvARB,
    GLNormal3f,
    GLOrtho,
    GLPixelStorei,
    GLPointSize,
    GLPolygonMode,
    GLPopMatrix,
    GLPushMatrix,
    GLReadPixels,
    GLRotated,
    GLRotatef,
    GLRotatefEXT,
    GLRotatefEXTs,
    GLScaled,
    GLScalef,
    GLScissor,
    GLShadeModel,
    GLTexCoord2f,
    GLTexCoord2fv,
    GLTexCoord4f,
    GLTexCoord4fv,
    GLTexCoordPointer,
    GLTexEnvi,
    GLTexGeni,
    GLTexImage2D,
    GLTexParameteri,
    GLTexSubImage2D,
    GLTranslated,
    GLTranslatef,
    GLULookAt,
    GLUPerspective,
    GLUnlockArrays,
    GLVertex2fv,
    GLVertex3fv,
    GLVertex4f,
    GLVertex4fv,
    GLVertexPointer,
    GLViewport,
    MGLClearPointer,
    MGLCreateContext,
    MGLCreateContextFromID,
    MGLDeleteContext,
    MGLDrawMultitexBuffer,
    MGLEnableSync,
    MGLExit,
    ClassicStub_MGLGetInputWindowHandle,
    MGLGetWindowHandle,
    MGLIdleFunc,
    MGLKeyFunc,
    MGLLockBack,
    MGLLockDisplay,
    MGLLockMode,
    MGLMainLoop,
    MGLMinTriArea,
    MGLMouseFunc,
    MGLPrintMatrix,
    MGLPrintMatrixStack,
    ClassicStub_MGLResizeContext,
    MGLSetPointer,
    MGLSetState,
    MGLSetZOffset,
    MGLSpecialFunc,
    MGLSwitchDisplay,
    MGLTexMemStat,
    MGLUnlockDisplay,
    MGLWriteShotPPM,
    mglChooseGuardBand,
    mglChooseMtexBufferSize,
    mglChooseNumberOfBuffers,
    mglChoosePixelDepth,
    mglChooseTextureBufferSize,
    mglChooseVertexBufferSize,
    mglChooseWindowMode,
    mglGetSupportedScreenModes,
    mglProhibitAlphaFallback,
    mglProhibitMipMapping,
    mglProposeCloseDesktop,
    GLPolygonOffset,
    /* --- Published V23 order (2026-09-11), same as the struct (positional init). --- */
    GLClientActiveTextureARB,
    GLInterleavedArrays,
    GLMultiDrawArrays,
    ClassicStub_GLBlendEquation,
    ClassicStub_GLBlendFuncSeparate,
    GLIsTexture,
    ClassicStub_mglChooseZBufferDepth,
    /* --- APPENDED 2026-09-12 (audit fix 4), matching the struct. --- */
    GLLineWidth,
    /* --- APPENDED 2026-09-12 (audit fix 7). --- */
    GLTexGenfv,
    /* --- APPENDED 2026-09-12 (audit fix 9), 139 and 140. --- */
    GLCopyTexImage2D,
    GLCopyTexSubImage2D,
    /* --- APPENDED 2026-09-12 (audit item 10), 141 to 151. --- */
    GLAreTexturesResident,
    GLEdgeFlag,
    GLEdgeFlagPointer,
    GLEdgeFlagv,
    GLGetDoublev,
    GLGetPointerv,
    ClassicStub_GLIndexi,
    ClassicStub_GLIndexiv,
    ClassicStub_GLIndexPointer,
    GLPrioritizeTextures,
    GLReadBuffer,
    MGLCreateContextFromWindow,
    MGLCreateContextFromBitMap,
    /* core GL -- fixed-function lighting */
    GLLightf,
    GLLightfv,
    GLMaterialf,
    GLMaterialfv,
    GLLightModelf,
    GLLightModelfv,
    GLGetLightfv,
    GLGetMaterialfv,

    /* core GL -- display lists */
    ClassicStub_GLGenLists,
    ClassicStub_GLDeleteLists,
    ClassicStub_GLIsList,
    ClassicStub_GLNewList,
    ClassicStub_GLEndList,
    ClassicStub_GLCallList,

    /* GLU */
    GLUBuild2DMipmaps,
    Classic_GLUErrorString,
    ClassicStub_GLUNewQuadric,
    ClassicStub_GLUDeleteQuadric,
    ClassicStub_GLUQuadricNormals,
    ClassicStub_GLUQuadricTexture,
    ClassicStub_GLUQuadricDrawStyle,
    ClassicStub_GLUQuadricOrientation,
    ClassicStub_GLUCylinder,
    ClassicStub_GLUSphere,
    ClassicStub_GLUDisk,

    /* GLUT */
    ClassicStub_GLUTInit,
    ClassicStub_GLUTInitDisplayMode,
    ClassicStub_GLUTInitWindowSize,
    ClassicStub_GLUTInitWindowPosition,
    ClassicStub_GLUTCreateWindow,
    ClassicStub_GLUTMainLoop,
    ClassicStub_GLUTDisplayFunc,
    ClassicStub_GLUTIdleFunc,
    ClassicStub_GLUTKeyboardFunc,
    ClassicStub_GLUTReshapeFunc,
    ClassicStub_GLUTSwapBuffers,
    ClassicStub_GLUTPostRedisplay,
    ClassicStub_GLUTGet,
    ClassicStub_GLUTGameModeString,
    ClassicStub_GLUTEnterGameMode,
    ClassicStub_GLUTLeaveGameMode,
    ClassicStub_GLUTGameModeGet,
    ClassicStub_GLUTSolidCube,
    ClassicStub_GLUTSolidSphere,
    ClassicStub_GLUTSolidCone,
    ClassicStub_GLUTSolidTorus,
    ClassicStub_GLUTSolidDodecahedron,
    GLNormalPointer,
    ClassicStub_GLUQuadricCallback,
    GLColorMaterial,
    GLTexEnvfv,
    GLNormal3fv, /* 29.1: append-only slot 205. */
    mglQuery,
    mglGetScreenmodeList,
    mglFreeScreenmodeList,
    mglGetModeDriverInfo,
    mglQueryMode,
    mglModeTexFmt,
    GLGetTexParameteriv,
    GLGetTexEnviv,
    GLGetTexEnvfv,
    GLGetTexGeniv,
    GLGetTexGenfv,
    GLGetTexLevelParameteriv,
    GLGetTexImage,
    GLDrawElementsBaseVertex,
    GLInsertEventMarkerEXT,
    GLPushGroupMarkerEXT,
    GLPopGroupMarkerEXT,
    GLGenVertexArrays,
    GLIsVertexArray,
    GLBindVertexArray,
    GLDeleteVertexArrays,
    GLClipControlEXT,
    GLStencilFunc,
    GLStencilMask,
    GLStencilOp,
    GLClearStencil,
    MGLSetTexChromaBounds,
    GLLogicOp,
    GLRectf,
    GLTexImage1D,
    GLPushAttrib,
    GLPopAttrib,
    GLRasterPos4f,
    GLPixelZoom,
    GLDrawPixels,
    GLCopyPixels,
    GLBitmap,
    GLUScaleImage,
    GLUGetString,
    GLUOrtho2D,
    GLUProject,
    GLUUnProject,
    GLUPickMatrix,
    GLMultiDrawElements,
};

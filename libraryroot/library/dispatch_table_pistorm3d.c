#define MINIGL_LIBRARY_BUILD 1
#include <exec/types.h>
#include <libraries/minigl_dispatch.h>
#include "context_access.h"

/* Present in backend archives but not declared by every historical header. */
extern void MGLLockMode(GLcontext context, GLenum lockMode);
extern void mglChooseNumberOfBuffers(int number);
extern void mglChooseVertexBufferSize(int size);
extern void mglChooseWindowMode(GLboolean flag);
extern void mglProposeCloseDesktop(GLboolean closeme);

/* glPolygonOffset dispatches to the backend's real implementation (2026-09-11).
 * This slot used to call a library-side no-op even though MiniGLV3D has
 * implemented GLPolygonOffset since 2026-08-30; the static games got the real
 * thing while every shared-library client silently got nothing. */
extern void GLPolygonOffset(GLcontext context, GLfloat factor, GLfloat units);

/* no W3D context on this backend, 5 = W3D_NOT_SUPPORTED */
#define P3D_QUERY_NOT_SUPPORTED 5L

static LONG P3DStub_mglQuery(GLcontext context, ULONG query, ULONG destfmt)
{
    return P3D_QUERY_NOT_SUPPORTED;
}

static void *P3DStub_mglGetScreenmodeList(void) { return (void *)0; }

static void P3DStub_mglFreeScreenmodeList(void *list) {}

/* 2 = W3D_TEXFMT_UNSUPPORTED */
#define P3D_TEXFMT_UNSUPPORTED 2L

static LONG P3DStub_mglGetModeDriverInfo(ULONG modeID, MGLModeDriverInfo *info) { return 0; }

static LONG P3DStub_mglQueryMode(ULONG modeID, ULONG query, ULONG destfmt)
{
    return P3D_QUERY_NOT_SUPPORTED;
}

static ULONG P3DStub_mglModeTexFmt(ULONG modeID, ULONG texfmt, ULONG destfmt)
{
    return P3D_TEXFMT_UNSUPPORTED;
}

static void P3DStub_GLGetTexParameteriv(GLcontext context, GLenum target, GLenum pname, GLint *params) {}

static void P3DStub_GLGetTexEnviv(GLcontext context, GLenum target, GLenum pname, GLint *params) {}

static void P3DStub_GLGetTexEnvfv(GLcontext context, GLenum target, GLenum pname, GLfloat *params) {}

static void P3DStub_GLGetTexGeniv(GLcontext context, GLenum coord, GLenum pname, GLint *params) {}

static void P3DStub_GLGetTexGenfv(GLcontext context, GLenum coord, GLenum pname, GLfloat *params) {}

static void P3DStub_GLDrawElementsBaseVertex(GLcontext context, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex) {}

static void P3DStub_GLInsertEventMarkerEXT(GLcontext context, GLsizei length, const GLchar *marker) {}

static void P3DStub_GLPushGroupMarkerEXT(GLcontext context, GLsizei length, const GLchar *marker) {}

static void P3DStub_GLPopGroupMarkerEXT(GLcontext context) {}

static void P3DStub_GLGenVertexArrays(GLcontext context, GLsizei n, GLuint *ids) {}

static GLboolean P3DStub_GLIsVertexArray(GLcontext context, GLuint id) { return GL_FALSE; }

static void P3DStub_GLBindVertexArray(GLcontext context, GLuint id) {}

static void P3DStub_GLDeleteVertexArrays(GLcontext context, GLsizei n, const GLuint *ids) {}

static void P3DStub_GLClipControlEXT(GLcontext context, GLenum origin, GLenum depth) {}

static void P3DStub_GLStencilFunc(GLcontext context, GLenum func, GLint ref, GLuint mask) {}

static void P3DStub_GLStencilMask(GLcontext context, GLuint mask) {}

static void P3DStub_GLStencilOp(GLcontext context, GLenum fail, GLenum zfail, GLenum zpass) {}

static void P3DStub_GLClearStencil(GLcontext context, GLint s) {}

static ULONG P3DStub_MGLSetTexChromaBounds(GLcontext context, GLuint texture, ULONG rgba_lower, ULONG rgba_upper, ULONG mode) { return 5; }

static void P3DStub_GLLogicOp(GLcontext context, GLenum opcode) {}

static void P3DStub_GLRectf(GLcontext context, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {}

static void P3DStub_GLTexImage1D(GLcontext context, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {}

static void P3DStub_GLPushAttrib(GLcontext context, GLbitfield mask) {}

static void P3DStub_GLPopAttrib(GLcontext context) {}

static void P3DStub_GLRasterPos4f(GLcontext context, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {}

static void P3DStub_GLPixelZoom(GLcontext context, GLfloat xfactor, GLfloat yfactor) {}

static void P3DStub_GLDrawPixels(GLcontext context, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels) {}

static void P3DStub_GLCopyPixels(GLcontext context, GLint x, GLint y, GLsizei width, GLsizei height, GLenum type) {}

static void P3DStub_GLBitmap(GLcontext context, GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap) {}

static GLint P3DStub_GLUScaleImage(GLcontext context, GLenum format, GLint wIn, GLint hIn, GLenum typeIn, const GLvoid *dataIn, GLint wOut, GLint hOut, GLenum typeOut, GLvoid *dataOut) { return 0; }

static const GLubyte *P3DStub_GLUGetString(GLenum name) { return NULL; }

static void P3DStub_GLUOrtho2D(GLcontext context, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top) {}

static GLint P3DStub_GLUProject(GLcontext context, GLdouble objX, GLdouble objY, GLdouble objZ, const GLdouble *model, const GLdouble *proj, const GLint *viewport, GLdouble *winX, GLdouble *winY, GLdouble *winZ) { return 0; }

static GLint P3DStub_GLUUnProject(GLcontext context, GLdouble winX, GLdouble winY, GLdouble winZ, const GLdouble *model, const GLdouble *proj, const GLint *viewport, GLdouble *objX, GLdouble *objY, GLdouble *objZ) { return 0; }

static void P3DStub_GLUPickMatrix(GLcontext context, GLdouble x, GLdouble y, GLdouble width, GLdouble height, const GLint *viewport) {}

static void P3DStub_GLMultiDrawElements(GLcontext context, GLenum mode, const GLsizei *count, GLenum type, const GLvoid *const *indices, GLsizei primcount) {}

static void P3DStub_GLGetTexLevelParameteriv(GLcontext context, GLenum target, GLint level, GLenum pname, GLint *params) {}

static void P3DStub_GLGetTexImage(GLcontext context, GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels) {}

const MGLDispatchTable MiniGLDispatchTable = {
    MINIGL_DISPATCH_ABI_VERSION,
    (ULONG)sizeof(MGLDispatchTable),
    MINIGL_BACKEND_FLAG_PISTORM3D,
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
    MGLGetInputWindowHandle,
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
    MGLResizeContext,
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
    /* --- Published V23 order (2026-09-11), same as the struct: this
     * initialiser is positional, so keep it in step with the header. --- */
    GLClientActiveTextureARB,
    GLInterleavedArrays,
    GLMultiDrawArrays,
    GLBlendEquation,
    GLBlendFuncSeparate,
    GLIsTexture,
    mglChooseZBufferDepth,
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
    GLIndexi,
    GLIndexiv,
    GLIndexPointer,
    GLPrioritizeTextures,
    GLReadBuffer,

    /* APPENDED 2026-09-20 -- render into a window the host already opened.
     * All three tables grow together: a short positional initialiser
     * zero-fills the trailing slot while sizeof still reports the full struct,
     * so the ABI check passes and the call NULL-derefs. */
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
    GLGenLists,
    GLDeleteLists,
    GLIsList,
    GLNewList,
    GLEndList,
    GLCallList,

    /* GLU */
    GLUBuild2DMipmaps,
    GLUErrorString,
    GLUNewQuadric,
    GLUDeleteQuadric,
    GLUQuadricNormals,
    GLUQuadricTexture,
    GLUQuadricDrawStyle,
    GLUQuadricOrientation,
    GLUCylinder,
    GLUSphere,
    GLUDisk,

    /* GLUT */
    GLUTInit,
    GLUTInitDisplayMode,
    GLUTInitWindowSize,
    GLUTInitWindowPosition,
    GLUTCreateWindow,
    GLUTMainLoop,
    GLUTDisplayFunc,
    GLUTIdleFunc,
    GLUTKeyboardFunc,
    GLUTReshapeFunc,
    GLUTSwapBuffers,
    GLUTPostRedisplay,
    GLUTGet,
    GLUTGameModeString,
    GLUTEnterGameMode,
    GLUTLeaveGameMode,
    GLUTGameModeGet,
    GLUTSolidCube,
    GLUTSolidSphere,
    GLUTSolidCone,
    GLUTSolidTorus,
    GLUTSolidDodecahedron,
    GLNormalPointer,
    GLUQuadricCallback,
    GLColorMaterial,
    GLTexEnvfv,
    GLNormal3fv, /* 29.1: append-only slot 205. */
    P3DStub_mglQuery,
    P3DStub_mglGetScreenmodeList,
    P3DStub_mglFreeScreenmodeList,
    P3DStub_mglGetModeDriverInfo,
    P3DStub_mglQueryMode,
    P3DStub_mglModeTexFmt,
    P3DStub_GLGetTexParameteriv,
    P3DStub_GLGetTexEnviv,
    P3DStub_GLGetTexEnvfv,
    P3DStub_GLGetTexGeniv,
    P3DStub_GLGetTexGenfv,
    P3DStub_GLGetTexLevelParameteriv,
    P3DStub_GLGetTexImage,
    P3DStub_GLDrawElementsBaseVertex,
    P3DStub_GLInsertEventMarkerEXT,
    P3DStub_GLPushGroupMarkerEXT,
    P3DStub_GLPopGroupMarkerEXT,
    P3DStub_GLGenVertexArrays,
    P3DStub_GLIsVertexArray,
    P3DStub_GLBindVertexArray,
    P3DStub_GLDeleteVertexArrays,
    P3DStub_GLClipControlEXT,
    P3DStub_GLStencilFunc,
    P3DStub_GLStencilMask,
    P3DStub_GLStencilOp,
    P3DStub_GLClearStencil,
    P3DStub_MGLSetTexChromaBounds,
    P3DStub_GLLogicOp,
    P3DStub_GLRectf,
    P3DStub_GLTexImage1D,
    P3DStub_GLPushAttrib,
    P3DStub_GLPopAttrib,
    P3DStub_GLRasterPos4f,
    P3DStub_GLPixelZoom,
    P3DStub_GLDrawPixels,
    P3DStub_GLCopyPixels,
    P3DStub_GLBitmap,
    P3DStub_GLUScaleImage,
    P3DStub_GLUGetString,
    P3DStub_GLUOrtho2D,
    P3DStub_GLUProject,
    P3DStub_GLUUnProject,
    P3DStub_GLUPickMatrix,
    P3DStub_GLMultiDrawElements,
};

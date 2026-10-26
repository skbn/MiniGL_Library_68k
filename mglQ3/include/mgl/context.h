/*
 * $Id: context.h,v 1.1.1.1 2000/04/07 19:44:51 tfrieden Exp $
 *
 * $Date: 2000/04/07 19:44:51 $
 * $Revision: 1.1.1.1 $
 *
 * (C) 1999 by Hyperion
 * All rights reserved
 *
 * This file is part of the MiniGL library project
 * See the file Licence.txt for more details
 *
 */

#ifndef __CONTEXT_H
#define __CONTEXT_H

#include "mgl/matrix.h"
#include "mgl/config.h"
#include "mgl/vertexbuffer.h"

#pragma pack(push, 2)

#include <intuition/intuition.h>

#ifdef __PPC__

#include <devices/timer.h>

typedef struct LockTimeHandle_s
{
    struct timeval StartTime;
} LockTimeHandle;

#else

typedef struct LockTimeHandle_s
{
    ULONG s_hi, s_lo;
    ULONG e_freq;
} LockTimeHandle;

#endif

#pragma pack(pop)

typedef struct GLcontext_t *GLcontext;

struct GLcontext_t;

typedef void (*DrawFn)(struct GLcontext_t *);

/* modeID driver info copied out to the caller, slots 209-211 */
typedef struct
{
    ULONG chipID;
    ULONG formats;
    ULONG swdriver;
    char name[64];
} MGLModeDriverInfo;

LONG mglGetModeDriverInfo(ULONG modeID, MGLModeDriverInfo *info);
LONG mglQueryMode(ULONG modeID, ULONG query, ULONG destfmt);
ULONG mglModeTexFmt(ULONG modeID, ULONG texfmt, ULONG destfmt);

typedef enum
{
    MGLKEY_F1,
    MGLKEY_F2,
    MGLKEY_F3,
    MGLKEY_F4,
    MGLKEY_F5,
    MGLKEY_F6,
    MGLKEY_F7,
    MGLKEY_F8,
    MGLKEY_F9,
    MGLKEY_F10,
    MGLKEY_CUP,
    MGLKEY_CDOWN,
    MGLKEY_CLEFT,
    MGLKEY_CRIGHT
} MGLspecial;

typedef enum
{
    GLCS_TEXTURE = 0x01,
    GLCS_COLOR = 0x02,
    GLCS_VERTEX = 0x04,
    GLCS_NORMAL = 0x08,
    GLCS_EDGEFLAG = 0x10,
    GLCS_MASK = 0x1F,
} ClientStates;

typedef void (*KeyHandlerFn)(char key);
typedef void (*SpecialHandlerFn)(MGLspecial special_key);
typedef void (*MouseHandlerFn)(GLint x, GLint y, GLbitfield buttons);
typedef void (*IdleFn)(void);

/*
ArrayPointer and stride storage:

We define all pointers as UBYTE to avoid problems with badly aligned arrays.
*/

#define AP_FIXPOINT 1 << 0
#define AP_COMPILED 1 << 1
#define AP_CLIP_BYPASS 1 << 2
#define AP_CHECK_OUTCODES 1 << 3
#define AP_VOLATILE 1 << 4 // current array range is "recycled" after drawing operation so it is okay to modify data in the current arrays.

typedef struct MGLClientTex_s
{
    GLubyte *ptr; /* application texcoord array */
    GLint stride;
    GLubyte size;
    GLenum type;
} MGLClientTex;

typedef struct MGLAPointer_s
{
    MGLClientTex ctex[MAX_TEXUNIT]; /* per-unit texcoord arrays, index = unit */

    GLubyte *w_buffer; // w coord pointer
    GLint w_off;       // w-offset rel. to texcoords

    GLubyte *colors; // application array
    GLint colorstride;
    ULONG colormode; // w3d bitfield

    GLubyte *verts; // application array
    GLint vertexstride;
    ULONG vertexmode; // fff / ddd / ffd

    // GL_EXT_compiled_vertex_arrays:
    GLuint lockfirst; // start of locked range
    GLsizei locksize;
    GLuint transformed; // vertexbuffer offset

    GLbitfield state; // pipeline state (AP_##)

} MGLAPointer;

#define MGL_VAO_MAX 1024

typedef struct MGLVAO_s
{
    GLuint name;
    MGLAPointer ptr;
    GLbitfield clientstate;
    ULONG texcs;
    GLubyte arrvertsize;
    GLubyte arrcolorsize;
    GLenum arrverttype;
    GLenum arrcolortype;
    GLubyte *arrnormal;
    GLint arrnormalstride;
    GLenum arrnormaltype;
    GLubyte tcopymax;
    GLboolean arrayslocked;
    GLuint lockfirst;
    GLuint lockcount;
    const GLvoid *arredgeflag;
    GLint arredgestride;
} MGLVAO;

typedef struct GLarray_t
{
    /*
    ** Vertex array
    */

    GLint size;      /* Number of elements per entry (mostly 3 or 4) */
    GLenum type;     /* Data type of entries */
    GLsizei stride;  /* How to reach the next array element */
    GLvoid *pointer; /* Pointer to the actual data */
} GLarray;

#define MGL_LF_DIR 1   /* w==0 directional, pos.xyz stays normalized */
#define MGL_LF_ATTEN 2 /* kl or kq nonzero, per-vertex distance work */
#define MGL_LF_SPOT 4  /* cutoff under 180, cone test applies */

typedef struct MGLLight_t
{
    W3D_Color ambient;
    W3D_Color diffuse;
    W3D_Color specular;
    GLfloat pos[4]; /* eye-space position w=1 or direction w=0 */
    GLfloat dir[3]; /* eye-space spot direction, normalized */
    GLfloat spotExp;
    GLfloat spotCos; /* cos(cutoff), -1.0 when cutoff is 180 */
    GLfloat kc, kl, kq;
    GLfloat att0; /* 1/kc shortcut when no kl/kq */
    UBYTE flags;
} MGLLight;

#define MGL_MAX_LIGHTS 8

typedef struct MGLMaterial_t
{
    W3D_Color ambient;
    W3D_Color diffuse;
    W3D_Color specular;
    W3D_Color emission;
    GLfloat shininess;
} MGLMaterial;

typedef struct MGLEye_t
{
    GLfloat ex;
    GLfloat ey;
    GLfloat ez;
    GLfloat nx;
    GLfloat ny;
    GLfloat nz;
} MGLEye;

#define MGL_ATTRIB_MAX 32

typedef struct
{
    GLbitfield mask;
    GLboolean en_alpha;
    GLboolean en_blend;
    GLboolean en_cull;
    GLboolean en_depth;
    GLboolean en_dither;
    GLboolean en_fog;
    GLboolean en_scissor;
    GLboolean en_stencil;
    GLboolean en_logic;
    GLboolean en_points;
    GLboolean en_texgen_s;
    GLboolean en_texgen_t;
    GLboolean en_zoff;
    GLboolean en_normalize;
    GLboolean en_wmask;
    GLenum alphaFunc;
    GLfloat alphaRef;
    GLint blendSrc;
    GLint blendDst;
    GLubyte colorMask;
    GLenum logicOp;
    GLenum depthFunc;
    GLboolean depthMask;
    W3D_Double depthClear;
    GLenum stFunc;
    GLint stRef;
    GLuint stValueMask;
    GLuint stWriteMask;
    GLenum stSfail;
    GLenum stZfail;
    GLenum stZpass;
    GLint stClear;
    W3D_Scissor scissor;
    GLenum fogMode;
    GLdouble fogStart;
    GLdouble fogEnd;
    GLint polyMode;
    GLenum cullFace;
    GLfloat pointSize;
    GLfloat lineWidth;
    GLuint clearColor;
    GLfloat curColor[4];
    GLenum matrixMode;
    GLfloat ax;
    GLfloat ay;
    GLfloat sx;
    GLfloat sy;
    ULONG clipFlags;
    GLenum texEnv[MAX_TEXUNIT];
    GLint curBinding;
    GLint virtBinding[MAX_TEXUNIT];
    GLuint activeTex;
    GLboolean tex2d[MAX_TEXUNIT];
    GLboolean en_lighting;
    GLboolean en_colormat;
    GLboolean en_rescale;
    GLubyte lightmask;
    MGLLight light[MGL_MAX_LIGHTS];
    MGLMaterial material[2];
    W3D_Color modelAmb;
    GLboolean modelLV;
    GLboolean modelTwo;
    GLboolean modelSep;
    GLenum cmFace;
    GLenum cmMode;
} MGLAttrib;

#if 0
typedef void (*DrawElementsFn)(GLcontext context, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
typedef void (*DrawArraysFn)(GLcontext context, GLenum mode, GLint first, GLsizei count);
#endif

struct GLcontext_t
{
    /*
    ** The primitive with which glBegin was called,
    ** or GL_BASE if outside glBegin/glEnd
    */

    GLenum CurrentPrimitive;

    /*
    ** Current error
    */
    GLenum CurrentError;

    /*
    ** The ModelView/Projection matrix stack.
    ** Note that the topmost (= current) matrix is not the
    ** top of the stack, but rather one of the ModelView[]/Projection[] below.
    ** This makes copying the matrices unnecessary...
    */
    int ModelViewStackPointer;
    Matrix ModelViewStack[MODELVIEW_STACK_SIZE];

    int ProjectionStackPointer;
    Matrix ProjectionStack[PROJECTION_STACK_SIZE];

    /*
    ** The current ModelView/Projeciton matrix.
    ** The matrix multiplication routine will switch between those
    ** two to avoid copying stuff.
    */
    GLuint ModelViewNr;
    Matrix ModelView[2];

#define CurrentMV (&(context->ModelView[context->ModelViewNr]))
#define SwitchMV context->ModelViewNr = !(context->ModelViewNr)

    GLuint ProjectionNr;
    Matrix Projection[2];

#define CurrentP (&(context->Projection[context->ProjectionNr]))
#define SwitchP context->ProjectionNr = !(context->ProjectionNr)

    // The current matrix mode (GL_MODELVIEW or GL_PROJECTION)
    GLuint CurrentMatrixMode;

    /*
    ** flexible buffer reserved for vertexarrays
    */

    GLfloat *WBuffer;

    /*
    ** Vertex buffers
    ** A call to glVertex*() will fill one entry of the vertex buffer
    ** with the current data. glEnd() will go over this data and
    ** draw the primitives based on this.
    */

    MGLVertex *VertexBuffer;
    GLuint VertexBufferPointer; // Next free entry
    GLuint VertexBufferSize;    // Size of the buffer

    /*
    ** Surgeon: Normals are only used in auto-texcoord generation and are not part of the W3D_Vertex struct used for drawing.
    ** Besides, Normals are more often specified for a surface rather than for single verts.
    ** The solution is a normal-buffer with position in the buffer recorded in glVertex*() functions.
    ** With propewr implementation I believe this should generate a lot of cache hits in v_GenTexCoords :)
    **
    ** Note: CurrentNormal tracks glNormal for GL_CURRENT_NORMAL gets
    **
    **
    ** A similar approach for colors is worth considering
    ** (see vertexbuffer.h for a suggestion)
    ** ubyte colors could be kept in that format until further down the pipeline.
    ** If colors were stored in a buffer, the check for shademodel currently within glVertex could be delayed until after on/off-screen culling.
    ** With smoothshading enabled, colorcopy and optional conversion from ubyte to float should probably be delayed until after the backface tests.
    **
    */

    MGLNormal *NormalBuffer;
    GLuint NormalBufferPointer;

    /*
    ** Current colors
    */
    GLuint ClearColor;
    W3D_Double ClearDepth;
    W3D_Color CurrentColor; // was MGLColor

    // Surgeon: minimize W3D_SetCurrentColor calls
    GLboolean UpdateCurrentColor;

    MGLNormal CurrentNormal; /* glNormal current state for GL_CURRENT_NORMAL */

    GLfloat CurrentTexS, CurrentTexT, CurrentTexQ;
    GLboolean CurrentTexQValid;
    /*
    ** The flag indicates wether the combined matrix is valid or not.
    ** If it indicates GL_TRUE, the CombinedMatrix field contains the
    ** product of the ModelView and Projection matrix.
    */
    GLboolean CombinedValid;
    Matrix CombinedMatrix;

    /*
    ** Scale factors for the transformation of normalized coordinates
    ** to window coordinates. The *x and *y values are set by glViewPort.
    ** *z is set by glDepthRange, which also sets near and far.
    */

//	GLdouble    sx,sy,sz;
//	GLdouble    ax,ay,az;
#if 0 // surgeon: double precision not needed
	GLdouble    sx,ax;
	GLdouble    sy,ay;
	GLdouble    sz,az;
#else
    GLfloat sx, ax;
    GLfloat sy, ay;
    GLfloat sz, az;
#endif
    GLdouble near, far;

    GLuint ClipFlags;    // surgeon: viewport flags used with guardband clipping
    GLboolean GuardBand; // surgeon

    // CullFace mode
    GLenum CurrentCullFace;
    GLenum CurrentFrontFace;

    // Sign extracted from above
    //  0 means no culling
    //  1 means back + ccw or front + cw
    // -1 means back + cw  or front + ccw

    GLint CurrentCullSign; /* unused, kept for layout */

    // Pixel states
    GLint PackAlign;
    GLint UnpackAlign;

    /*
    ** GL Rendering States
    */
    GLboolean AlphaTest_State;
    GLboolean Blend_State;
    GLboolean Texture2D_State[MAX_TEXUNIT];
    GLboolean TextureGenS_State;
    GLboolean TextureGenT_State;
    GLboolean Fog_State;
    GLboolean Scissor_State;
    GLboolean CullFace_State;
    GLboolean DepthTest_State;
    GLboolean PointSmooth_State;
    GLboolean Dither_State;
    GLboolean ZOffset_State; /* unused, kept for layout */
    GLboolean Stencil_State;

    GLubyte StencilBits;
    GLenum StencilFunc;
    GLint StencilRef;
    GLuint StencilValueMask;
    GLuint StencilWriteMask;
    GLenum StencilSfail;
    GLenum StencilZfail;
    GLenum StencilZpass;
    GLint StencilClear;

    /*
    ** 'Internal' states
    */

    GLboolean FogDirty;

    GLdouble FogStart;
    GLdouble FogEnd;
    GLenum CurFogMode;

    /*
    ** Drawing and clipping functions for the current primitive
    */

    DrawFn CurrentDraw;

    /*
    ** Warp3D specific stuff
    */

    W3D_Context *w3dContext;
    struct Window *w3dWindow;
    struct Screen *w3dScreen;
    GLboolean externalWindow;

    GLboolean ArrayTexBound;

    GLint CurrentBinding;

    // Multitexture
    GLint VirtualBinding[MAX_TEXUNIT]; /* bound texture name per unit, index = unit ([0] unused) */
    GLuint TexUnits;                   /* runtime unit cap <= MAX_TEXUNIT */
    GLuint ActiveTexture;              /* server unit for texenv/binding */
    GLuint ClientActiveTexture;        /* client unit for texcoord arrays */

    W3D_Texture **w3dTexBuffer;
    GLubyte **w3dTexMemory;
    GLint TexBufferSize;
    struct ScreenBuffer *Buffers[3];
    struct BitMap *w3dBitMap;     // If in windowed mode
    struct RastPort *w3dRastPort; // for windowed ClipBlit mode
    int BufNr;
    int NumBuffers;
    W3D_Scissor scissor;
    GLboolean w3dLocked;

#ifdef AUTOMATIC_LOCKING_ENABLE
    GLenum LockMode;
    LockTimeHandle LockTime;
#endif
    GLboolean DoSync;
    ULONG w3dChipID;
    ULONG w3dFormat;
    ULONG w3dAlphaFormat;
    GLint w3dBytesPerTexel;

    GLenum TexEnv[MAX_TEXUNIT];
    GLenum CurTexEnv;
    W3D_Color CurTexEnvColor; /* live global env color (BLEND), dirty-check snapshot */
    GLenum MinFilter;
    GLenum MagFilter;
    GLenum WrapS;
    GLenum WrapT;

    W3D_Fog w3dFog;
    ULONG w3dFogMode;
    GLfloat FogRange;
    GLfloat FogMult;
    GLenum ShadeModel;
    GLboolean DepthMask;

    GLboolean NoMipMapping;
    GLboolean NoFallbackAlpha;

    KeyHandlerFn KeyHandler;
    MouseHandlerFn MouseHandler;
    SpecialHandlerFn SpecialHandler;
    IdleFn Idle;
    GLboolean Running;

    GLenum SrcAlpha;
    GLenum DstAlpha;
    GLboolean AlphaFellBack;

    GLfloat InvRot[9];

    GLboolean InvRotValid;
    GLboolean WOne_Hint;
    GLboolean FixpointTrans_Hint; // Surgeon

    GLfloat ZOffset;

    GLfloat PolygonOffsetFactor;
    GLfloat PolygonOffsetUnits;
    GLboolean PolygonOffsetFill_State;
    GLfloat DepthBufferUnit;

    void *PaletteData;
    GLenum PaletteFormat;
    GLint PaletteSize;

    /* Begin Joe Sera Sept. 23 2000 */
    /*
    ** GL Current Modes and States for glGetIntegerv
    */
    GLint CurPolygonMode; /* GL_POLYGON_MODE */

    GLint CurBlendSrc;         /* GL_BLEND_SRC */
    GLint CurBlendDst;         /* GL_BLEND_DST */
    GLint CurUnpackRowLength;  /* GL_UNPACK_ROW_LENGTH */
    GLint CurUnpackSkipPixels; /* GL_UNPACK_SKIP_PIXELS */
    GLint CurUnpackSkipRows;   /* GL_UNPACK_SKIP_ROWS */

    /* End Joe Sera Sept. 23 2000 */

    /* Begin Joe Sera Oct. 21, 2000  */
    /*
    ** GL Current Modes and States for glGetBooleanv
    */

    GLboolean CurWriteMask; /* GL_DEPTH_WRITE_MASK */
    GLboolean CurDepthTest; /* GL_DEPTH_TEST */

    /* End Joe Sera Oct. 21 2000 */

    /*
    ** Client state
    */

    GLbitfield ClientState; /* Current client state mask */
    ULONG TexCSMask;        /* per-unit texcoord-array enables, bit k = unit k */

#if 0

	GLarray         ColorArray;         /* Current arrays */
	GLarray         TexCoordArray;
	GLarray         VertexArray;
#endif

    MGLAPointer ArrayPointer;
    GLboolean VertexArrayPipeline;
    GLboolean ArrayClientBound;

#if 0
	DrawElementsFn  DrawElementsHook;   /* Function pointers. These will depend on the state */
	DrawArraysFn    DrawArraysHook;
#endif

    GLfloat MinTriArea; /* Minimal area of triangles to be drawn (smaller ones will be rejected)*/

    GLfloat CurrentPointSize; /* diameter */

    GLfloat LineWidth; // Cowcat

    GLubyte *GeneratedTextures; /* Array to keep track of generated textures */

    GLboolean externalBitMap; /* v27: caller owns render target; never free/present it. */

    /*
     * appended pipeline state (no client inline pokes past this point)
     */

    UWORD *ClipIdx;      /* clip in/out lists and emit index batch */
    UWORD *IdentIdx;     /* identity sequence for unindexed emitters */
    UWORD *IdxBuf;       /* remapped element lists for DrawElements slices */
    GLuint EmitBase;     /* first staged vert still needing transform */
    GLuint StripBase;    /* strip tri index across mid-begin flushes */
    GLboolean CullHW;    /* driver took W3D_CULLFACE */
    GLboolean SmartLock; /* lock held by the smart lock policy */
    GLboolean ElemQueue; /* unused, kept for layout */

    GLboolean ArraysLocked; /* glLockArraysEXT range is active */
    GLboolean LockStaged;   /* locked range is staged in VertexBuffer */
    GLuint LockFirst;       /* locked client array span */
    GLuint LockCount;

    GLubyte ArrVertSize; /* client array element sizes for staging */
    GLubyte ArrColorSize;
    GLenum ArrVertType;
    GLenum ArrColorType;

    GLfloat CurrentTexR;                        /* tex3d current state */
    struct TCoord_t CurrentTcoord[MAX_TEXUNIT]; /* per-unit current texcoord, index = unit ([0] unused) */
    GLubyte TCopyMax;                           /* tcoord slots staged per vertex = highest unit seen enabled */

    GLubyte *ArrNormal; /* normal array pointer */
    GLint ArrNormalStride;
    GLenum ArrNormalType;

    GLuint EmitSkip; /* leading seed indices in a chunked indexed submit */

    GLenum CurDrawBuffer;    /* GL_DRAW_BUFFER */
    GLenum CurReadBuffer;    /* GL_READ_BUFFER */
    GLenum CurAlphaFunc;     /* GL_ALPHA_TEST_FUNC */
    GLfloat CurAlphaRef;     /* GL_ALPHA_TEST_REF */
    GLubyte CurColorMask;    /* rgba write enables */
    GLint CurPackRowLength;  /* GL_PACK_ROW_LENGTH */
    GLint CurPackSkipPixels; /* GL_PACK_SKIP_PIXELS */
    GLint CurPackSkipRows;   /* GL_PACK_SKIP_ROWS */

    GLint CurUnpackSwapBytes; /* GL_UNPACK_SWAP_BYTES */
    GLint CurPackSwapBytes;   /* GL_PACK_SWAP_BYTES */
    ULONG mtexSrc;            /* pass-2 blend src, W3D space */
    ULONG mtexDst;
    ULONG mtexEnv;       /* pass-2 texenv, W3D space */
    GLboolean mtexArmed; /* next submit replays the batch for unit-1 */
    GLenum CurDepthFunc; /* last glDepthFunc pushed */
    GLint TexMemCur;     /* live texture payload bytes */
    GLint TexMemPeak;

    GLuint NextTexture;                 /* texture-name scan hint */
    W3D_Color TexEnvColor[MAX_TEXUNIT]; /* per-unit env color fed to W3D_BLEND */
    GLenum TexGenModeS;                 /* GL_TEXTURE_GEN_MODE per coord */
    GLenum TexGenModeT;
    GLfloat ObjectPlaneS[4];
    GLfloat ObjectPlaneT[4];
    GLfloat EyePlaneS[4]; /* eye planes arrive pre-transformed by inv-modelview */
    GLfloat EyePlaneT[4];
    GLint CurPackLsbFirst;
    GLint CurUnpackLsbFirst;
    GLenum HintPersp; /* glHint shadows W3D has no readback */
    GLenum HintPoint;
    GLenum HintLine;
    GLenum HintPoly;
    GLenum HintFog;
    ULONG TexFmtNeg[12]; /* ideal W3D texfmt -> driver-supported equivalent */

    GLboolean Lighting_State; /* GL_LIGHTING */
    GLubyte LightMask;        /* bit i set = GL_LIGHTi enabled */

    MGLLight Light[MGL_MAX_LIGHTS];
    MGLMaterial Material[2]; /* 0 = front, 1 = back */

    W3D_Color ModelAmbient; /* GL_LIGHT_MODEL_AMBIENT */
    GLboolean ModelLocalViewer;
    GLboolean ModelTwoSide;
    GLboolean ModelSepSpec; /* GL_SEPARATE_SPECULAR_COLOR */

    GLboolean Normalize_State; /* GL_NORMALIZE */
    GLboolean Rescale_State;   /* GL_RESCALE_NORMAL */

    GLboolean ColorMaterial_State; /* GL_COLOR_MATERIAL */
    GLenum ColorMaterialFace;
    GLenum ColorMaterialMode;

    MGLEye *EyeScratch;     /* per-vertex eye pos + eye normal, VertexBufferSize entries */
    MGLMaterial *MeScratch; /* per-vertex material for color-material, 2 x cap */
    W3D_Color *LitBack;     /* back-face lit colors for two-side */
    W3D_Color *LitSpec;     /* front specular colors for separate-specular */
    W3D_Color *LitSpecB;    /* back specular colors when two-side joins */
    W3D_Color *LitSave;     /* pass-2/3 swap-restore for v.color */

    char DebugMarker[256];
    GLuint DebugMarkerDepth;

    MGLVAO VAOs[MGL_VAO_MAX + 1];
    GLuint VAOBound;

    GLboolean ClipUpperLeft;
    GLboolean ClipZO;

    GLenum LogicOp;
    GLboolean LogicOp_State;
    GLboolean LogicOpOK;

    GLboolean EdgeFlagCur;
    const GLvoid *EdgeFlagPtr;
    GLsizei EdgeFlagStride;
    UWORD *EdgeIdx;

    GLint AttribTop;
    MGLAttrib AttribStack[MGL_ATTRIB_MAX];

    GLfloat RasterPos[4];
    GLboolean RasterPosValid;
    GLfloat PixelZoomX;
    GLfloat PixelZoomY;

    GLfloat FogP43; /* projection terms the pushed fog bounds were built from */
    GLfloat FogP44;

    GLboolean pubLocked;   /* LockPubScreen still held (windowed create only) */
    W3D_Driver *TexFmtDrv; /* mode driver for texfmt cross-checks, NULL if unknown */
};

/*
** The CMATRIX macro give the address of the currently
** active matrix, depending on the matrix mode.
** The OMATRIX macro gives the address of the secondary matrix
** The SMATRIX macro switches the active and backup matrix
*/
#define CMATRIX(context) context->CurrentMatrixMode == GL_MODELVIEW ? (&(context->ModelView[context->ModelViewNr])) : (&(context->Projection[context->ProjectionNr]))

#define OMATRIX(context) context->CurrentMatrixMode == GL_MODELVIEW ? (&(context->ModelView[!(context->ModelViewNr)])) : (&(context->Projection[!(context->ProjectionNr)]))

#define SMATRIX(context)                                \
    if (context->CurrentMatrixMode == GL_MODELVIEW)     \
        context->ModelViewNr = !(context->ModelViewNr); \
    else                                                \
        context->ProjectionNr = !(context->ProjectionNr)

#endif

#include "mglint.h"
#include <stdio.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *UtilityBase;
struct Library *Warp3DBase;
struct Library *CyberGfxBase;

GLcontext mini_CurrentContext = (GLcontext)0;

static GLboolean mgl_owns_IntuitionBase;
static GLboolean mgl_owns_GfxBase;
static GLboolean mgl_owns_UtilityBase;
static GLboolean mgl_owns_Warp3DBase;
static GLboolean mgl_owns_CyberGfxBase;

#define MGL_W3D_MINVER 4

static void vid_open(struct Library **base, GLboolean *owns, const char *name, ULONG ver)
{
    if (!*base)
    {
        *base = OpenLibrary((STRPTR)name, ver);
        *owns = (*base != NULL);
    }
}

static void vid_close(struct Library **base, GLboolean *owns)
{
    if (*owns && *base)
    {
        CloseLibrary(*base);
        *base = NULL;
    }

    *owns = GL_FALSE;
}

GLboolean MGLInit(void)
{
    vid_open((struct Library **)&IntuitionBase, &mgl_owns_IntuitionBase, "intuition.library", 0);
    vid_open((struct Library **)&GfxBase, &mgl_owns_GfxBase, "graphics.library", 0);
    vid_open(&UtilityBase, &mgl_owns_UtilityBase, "utility.library", 0);
    vid_open(&CyberGfxBase, &mgl_owns_CyberGfxBase, "cybergraphics.library", 0);
    vid_open(&Warp3DBase, &mgl_owns_Warp3DBase, "Warp3D.library", MGL_W3D_MINVER);

    if (!IntuitionBase)
        printf("- intuition.library\n");

    if (!GfxBase)
        printf("- graphics.library\n");

    if (!UtilityBase)
        printf("- utility.library\n");

    if (!CyberGfxBase)
        printf("- cybergraphics.library\n");

    if (!Warp3DBase)
        printf("- Warp3D.library\n");

    if (!IntuitionBase || !GfxBase || !UtilityBase || !CyberGfxBase || !Warp3DBase)
    {
        MGLTerm();
        return GL_FALSE;
    }

    return GL_TRUE;
}

void MGLTerm(void)
{
    vid_close(&Warp3DBase, &mgl_owns_Warp3DBase);
    vid_close(&CyberGfxBase, &mgl_owns_CyberGfxBase);
    vid_close(&UtilityBase, &mgl_owns_UtilityBase);
    vid_close((struct Library **)&GfxBase, &mgl_owns_GfxBase);
    vid_close((struct Library **)&IntuitionBase, &mgl_owns_IntuitionBase);
}

GLboolean mglEnsureVidLibs(void)
{
    vid_open(&Warp3DBase, &mgl_owns_Warp3DBase, "Warp3D.library", MGL_W3D_MINVER);
    vid_open(&CyberGfxBase, &mgl_owns_CyberGfxBase, "cybergraphics.library", 0);

    if (!Warp3DBase || !CyberGfxBase)
        return GL_FALSE;

    return GL_TRUE;
}

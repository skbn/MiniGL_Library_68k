#include <stddef.h>
#include <proto/exec.h>
#include <libraries/minigl.h>
#include <libraries/minigl_dispatch.h>

/* Implemented by the tiny compiler-specific import stub. */
extern const MGLDispatchTable *MiniGLGetDispatchTableLVO(void);

BOOL MiniGLOpen(void)
{
    const MGLDispatchTable *dispatch;

    if (MiniGLBase && MiniGLDispatch)
        return TRUE;

    if (!MiniGLBase)
    {
        MiniGLBase = OpenLibrary(MINIGLNAME, MINIGL_VERSION);
        if (!MiniGLBase)
            return FALSE;
    }

    dispatch = MiniGLGetDispatchTableLVO();
    if (!dispatch)
    {
        CloseLibrary(MiniGLBase);
        MiniGLBase = NULL;
        return 0;
    }

    // accept foreign MiniGL tables covering the published-V23 prefix
    if (dispatch->structSize < offsetof(MGLDispatchTable, GLLineWidth))
    {
        CloseLibrary(MiniGLBase);
        MiniGLBase = 0;
        MiniGLDispatch = 0;
        return FALSE;
    }

    MiniGLDispatch = dispatch;
    return TRUE;
}

void MiniGLClose(void)
{
    MiniGLDispatch = 0;

    if (MiniGLBase)
    {
        CloseLibrary(MiniGLBase);
        MiniGLBase = 0;
    }
}

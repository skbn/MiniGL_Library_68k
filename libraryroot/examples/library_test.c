#include <stdio.h>
#include <proto/minigl.h>
#include "mgllog.h"

int main(void)
{
    void *ctx;
    const GLubyte *renderer;

    mgllog_open("library-test");

    MGLLOG("Opening minigl.library...");

    if (!MiniGLOpen())
    {
        MGLLOG("Could not open minigl.library V29 / ABI 5");
        return 20;
    }

    MGLLOG("dbg: choosevbs");

    mglChooseVertexBufferSize(1000);

    MGLLOG("dbg: createcontext");

    ctx = mglCreateContext(0, 0, 640, 480);

    if (!ctx)
    {
        MGLLOG("Could not create MiniGL context");
        MiniGLClose();
        return 20;
    }

    MGLLOG("dbg: ctx ok");

    renderer = glGetString(GL_RENDERER);
    printf("Renderer: %s\n", renderer ? (const char *)renderer : "<NULL>");

    MGLLOG("dbg: viewport+clear");

    glViewport(0, 0, 640, 480);
    glClearColor(0.1f, 0.2f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    MGLLOG("dbg: switch");

    mglSwitchDisplay();

    mglDeleteContext();
    MiniGLClose();

    MGLLOG("Done");

    return 0;
}

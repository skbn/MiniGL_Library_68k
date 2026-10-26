#ifndef MGLLOG_H
#define MGLLOG_H

#include <stdio.h>
#include <stdarg.h>

static FILE *mgl_logf;

static void mgllog_open(const char *name)
{
    char path[96];

    sprintf(path, "PROGDIR:%s.log", name);
    mgl_logf = fopen(path, "w");
}

static void mgllog(const char *s)
{
    puts(s);

    if (mgl_logf)
    {
        fprintf(mgl_logf, "%s\n", s);
        fflush(mgl_logf);
    }

    fflush(stdout);
}

static void mgllogf(const char *fmt, ...)
{
    char buf[160];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

    mgllog(buf);
}

#define MGLLOG(s) mgllog(s)
#define MGLLOGF(...) mgllogf(__VA_ARGS__)

#endif

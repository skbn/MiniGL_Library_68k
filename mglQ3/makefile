CC := m68k-amigaos-gcc
AR := m68k-amigaos-ar
RANLIB := m68k-amigaos-ranlib

CPU ?= 68020-60
FPU ?= 68881

ifeq ($(FPU),68881)
FPU_FLAGS := -m68881
else
$(error FPU must be 68881)
endif

OBJDIR ?= obj_gcc_$(CPU)-$(FPU)

LIB_SUFFIX := $(if $(filter 68881,$(FPU)),,-$(FPU))
LIBNAME ?= libmgl-q3$(LIB_SUFFIX).a

COMMON_CFLAGS = -std=c99 -m$(CPU) $(FPU_FLAGS) -noixemul \
	-O3 -fno-fast-math -fomit-frame-pointer -fno-strict-aliasing \
	-fno-strict-overflow -fsigned-char -fno-baserel -fno-builtin -D__M68K__

CFLAGS = $(COMMON_CFLAGS) -Iinclude -Wall -Wextra

LIB_SOURCES = src/context.c src/vertexbuffer.c src/draw.c \
	src/texture.c src/fog.c src/clip.c src/init.c src/glu.c \
	src/vertexelements.c src/vertexarray.c src/light.c \
	src/others.c src/matrix.c
	
LIB_OBJS = $(patsubst src/%.c,$(OBJDIR)/%.o,$(LIB_SOURCES))

.PHONY: all clean
all: $(LIBNAME)

$(OBJDIR):
	mkdir -p $@

$(OBJDIR)/%.o: src/%.c makefile | $(OBJDIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(LIBNAME): $(LIB_OBJS)
	mkdir -p $(dir $@)
	$(AR) rcs $@ $(LIB_OBJS)
	$(RANLIB) $@

clean:
	rm -rf obj_gcc_*
	rm -f $(LIBNAME)

-include $(LIB_OBJS:.o=.d)

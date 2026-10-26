#include <stddef.h>
#include <stdio.h>
#define MINIGL_LIBRARY_BUILD 1
#include <libraries/minigl_dispatch.h>
/* V23 published suffix is frozen; every later block is appended at the end. */
#define CHECK(name, expr) typedef char name[(expr) ? 1 : -1]
CHECK(abi5, MINIGL_DISPATCH_ABI_VERSION == 5);
CHECK(v23_prefix, offsetof(MGLDispatchTable, GLClientActiveTextureARB) == 536);
CHECK(v23_interleaved, offsetof(MGLDispatchTable, GLInterleavedArrays) == 540);
CHECK(v23_multidraw, offsetof(MGLDispatchTable, GLMultiDrawArrays) == 544);
CHECK(v23_suffix, offsetof(MGLDispatchTable, mglChooseZBufferDepth) == 560);
CHECK(v25_append, offsetof(MGLDispatchTable, GLLineWidth) == 564);
CHECK(v25_tail, offsetof(MGLDispatchTable, GLReadBuffer) == 620);
CHECK(v27_window, offsetof(MGLDispatchTable, MGLCreateContextFromWindow) == 624);
CHECK(v27_bitmap, offsetof(MGLDispatchTable, MGLCreateContextFromBitMap) == 628);
CHECK(v28_glu, offsetof(MGLDispatchTable, GLLightf) == 632);
CHECK(v28_glut, offsetof(MGLDispatchTable, GLUTInit) == 732);
CHECK(v28_glut_tail, offsetof(MGLDispatchTable, GLUTSolidDodecahedron) == 816);
CHECK(v29_tail, offsetof(MGLDispatchTable, GLNormal3fv) == 836);
CHECK(v29_2_query, offsetof(MGLDispatchTable, mglQuery) == 840);
CHECK(v29_2_smlist, offsetof(MGLDispatchTable, mglGetScreenmodeList) == 844);
CHECK(v29_2_smfree, offsetof(MGLDispatchTable, mglFreeScreenmodeList) == 848);
CHECK(v29_3_drvinfo, offsetof(MGLDispatchTable, mglGetModeDriverInfo) == 852);
CHECK(v29_3_qmode, offsetof(MGLDispatchTable, mglQueryMode) == 856);
CHECK(v29_3_texfmt, offsetof(MGLDispatchTable, mglModeTexFmt) == 860);
CHECK(v29_3_size, offsetof(MGLDispatchTable, GLGetTexParameteriv) == 864);
CHECK(v29_4_tail, offsetof(MGLDispatchTable, GLGetTexLevelParameteriv) == 884);
CHECK(v29_5_tail, offsetof(MGLDispatchTable, GLGetTexImage) == 888);
CHECK(v29_6_bvtx, offsetof(MGLDispatchTable, GLDrawElementsBaseVertex) == 892);
CHECK(v29_7_tail, offsetof(MGLDispatchTable, GLPopGroupMarkerEXT) == 904);
CHECK(v29_8_tail, offsetof(MGLDispatchTable, GLDeleteVertexArrays) == 920);
CHECK(v29_9_tail, offsetof(MGLDispatchTable, GLClipControlEXT) == 924);
CHECK(v29_10_tail, offsetof(MGLDispatchTable, MGLSetTexChromaBounds) == 944);
CHECK(v29_11_glu, offsetof(MGLDispatchTable, GLUScaleImage) == 988);
CHECK(v29_12_gstr, offsetof(MGLDispatchTable, GLUGetString) == 992);
CHECK(v29_12_pick, offsetof(MGLDispatchTable, GLUPickMatrix) == 1008);
CHECK(v29_12_size, sizeof(MGLDispatchTable) == 1012);
CHECK(abi_version, MINIGL_DISPATCH_ABI_VERSION == 5);
CHECK(classic_array_enum, MGL_ARRAY_TRANSFORMATIONS == 0x701D);
CHECK(classic_lock_enum, MGL_LOCK_AUTOMATIC == 0x7010);

int main(void)
{
    puts("PASS: ABI 5 table = 1012 bytes, V23 offsets preserved, Classic enums.");
    return 0;
}

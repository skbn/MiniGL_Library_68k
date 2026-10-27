#make.sh

cd "$(dirname "$0")"

export PATH=/opt/amiga/bin:$PATH
export CROSS=/opt/amiga/bin/m68k-amigaos-

Q3T=all

for a in "$@"; do [ "$a" = clean ] && Q3T=clean; done

(cd libraryroot && make -f ../Makefile_classic_verified.gcc clean) || exit

make -C mglQ3 -f makefile clean || exit

rm -f mglQ3/lib/libmgl-q3.a libminigl.a libraryroot/lib/libminigl_dispatch.a

[ "$Q3T" = clean ] && exit

(cd libraryroot && make -f ../Makefile_classic_verified.gcc -j12 "$@") || exit

make -C mglQ3 -f makefile CPU=68020-60 -j12 all || exit

MS=-nofastmath

[ "$FAST_MATH" = 1 ] && MS=-fastmath

B=libraryroot/bin/v29-path-68020-60-68881-nix20$MS

cp $B/stock/minigl.library minigl.library

(cd libraryroot && make -f Makefile_dispatch_classic.gcc CLASSIC_SRC=../mglQ3 CPU=68020-60 ../libminigl.a) || exit

exit 0

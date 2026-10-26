#make.sh

cd "$(dirname "$0")"

export PATH=/opt/amiga/bin:$PATH
export CROSS=/opt/amiga/bin/m68k-amigaos-

Q3T=all

for a in "$@"; do [ "$a" = clean ] && Q3T=clean; done

(cd libraryroot && make -f ../Makefile_classic_verified.gcc clean) || exit

make -C mglQ3 -f makefile clean || exit

[ "$Q3T" = clean ] && exit

(cd libraryroot && make -f ../Makefile_classic_verified.gcc -j12 "$@") || exit

make -C mglQ3 -f makefile CPU=68020-60 -j12 all || exit

MS=-nofastmath

[ "$FAST_MATH" = 1 ] && MS=-fastmath

B=libraryroot/bin/v29-path-68020-60-68881-nix20$MS

cp $B/stock/minigl.library minigl.library

exit 0

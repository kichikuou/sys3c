#!/bin/bash
# To update testdata/regression_test.dri, remove it and run this script
set -e

if [ $# -eq 0 ]; then
	echo 'Usage: regression_test.sh <binary_dir>'
	exit 1
fi

bindir="$1"

${bindir}/sys3c -p testdata/source/sys3c.cfg -o testdata/actual
if [ -f testdata/regression_test.dri ]; then
    ${bindir}/dri compare testdata/regression_test.dri testdata/actualSA.ALD
else
    cp testdata/actualSA.ALD testdata/regression_test.dri
fi
rm -rf testdata/decompiled
${bindir}/sys3dc -o testdata/decompiled testdata/actualSA.ALD
diff -uN --strip-trailing-cr testdata/source testdata/decompiled


tmpfile=$(mktemp)

diff -u --strip-trailing-cr - <(${bindir}/vsp -i testdata/*.vsp) <<EOF
testdata/16colors.vsp: 256x256, offset: (40, 20), palette bank: 7
EOF
${bindir}/vsp testdata/16colors.vsp -o $tmpfile && cmp testdata/16colors.png $tmpfile
${bindir}/vsp -e testdata/16colors.png -o $tmpfile && cmp testdata/16colors.vsp $tmpfile

diff -u --strip-trailing-cr - <(TZ=UTC ${bindir}/pms -i --system2 testdata/*.pms) <<EOF
testdata/256colors.pms: PMS 1, 256x256 8bpp, palette mask: 0xfffe, offset: (50, 30)
testdata/256colors_sys2.pms: PMS 0, 256x256 8bpp, offset: (50, 30)
testdata/highcolor.pms: PMS 1, 256x256 16bpp, offset: (50, 30)
testdata/highcolor_alpha.pms: PMS 2, 256x256 16bpp, 8bit alpha, 2021-03-14 04:00:26
EOF
${bindir}/pms testdata/256colors.pms -o $tmpfile && cmp testdata/256colors.png $tmpfile
${bindir}/pms -e testdata/256colors.png -o $tmpfile && cmp testdata/256colors.pms $tmpfile
${bindir}/pms --system2 testdata/256colors_sys2.pms -o $tmpfile && cmp testdata/256colors_nomask.png $tmpfile
${bindir}/pms -e --system2 testdata/256colors_nomask.png -o $tmpfile && cmp testdata/256colors_sys2.pms $tmpfile
${bindir}/pms testdata/highcolor.pms -o $tmpfile && cmp testdata/highcolor.png $tmpfile
${bindir}/pms -e testdata/highcolor.png -o $tmpfile && cmp testdata/highcolor.pms $tmpfile
${bindir}/pms testdata/highcolor_alpha.pms -o $tmpfile && cmp testdata/highcolor_alpha.png $tmpfile
${bindir}/pms -e testdata/highcolor_alpha.png -o $tmpfile && cmp testdata/highcolor_alpha.pms $tmpfile

rm $tmpfile

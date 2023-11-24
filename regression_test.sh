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

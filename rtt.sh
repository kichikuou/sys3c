#!/bin/bash
#
# rtt.sh: Round trip testing
#
# This decompiles given DAT file, compiles the generated source code,
# and verifies it matches the original DAT.
#

usage() {
	echo 'Usage: rtt.sh [options] <drifile(s)>'
	echo 'Options:'
	echo '    -C <directory>  Find executables in <directory> (default: ./build)'
	echo '    -h              Display this message and exit'
	echo '    -o <directory>  Generate outputs into <directory>'
	exit 1
}

if [ $# -eq 0 ]; then
	usage
fi

bin=./build
out=
cleanup=

while getopts C:o:h OPT
do
	case $OPT in
		C) bin="$OPTARG" ;;
		o) out="$OPTARG" ;;
		h) usage ;;
	esac
done
shift $((OPTIND - 1))

if [ -z "$out" ]; then
	out=$(mktemp -d)
	cleanup="rm -rf $out"
fi

Exit() {
	$cleanup
	exit $1
}

$bin/sys3dc -a -o "$out" "$@" || Exit 1
$bin/sys3c -p "$out"/sys3c.cfg || Exit 1

for file in "$@"; do
	[[ "$file" == -* ]] && continue
	base=$(basename "$file")
	if [[ "$file" == *.DAT ]]; then
		$bin/dri compare "$file" "$out"/"$base" || Exit 1
	else
		cmp "$file" "$out"/"$base" || Exit 1
	fi
done

Exit 0

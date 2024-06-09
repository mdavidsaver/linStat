#!/bin/sh
set -e

# eg. run "./cloc.sh --git HEAD"

TDIR="$(mktemp -d)"
trap 'rm -rf "$TDIR"' QUIT EXIT TERM

# write out built-in definitions
cloc --write-lang-def "$TDIR/def"

# remove conflicting definition for .cmd
sed -i -e '/\s*extension\s*cmd\s*/d' "$TDIR/def"

# add some EPICS specific input files
cat << EOF >> "$TDIR/def"
EPICS PDB
    extension dbd
    extension db
    extension template
    filter remove_matches ^\s*#
    filter remove_inline #.*$
    3rd_gen_scale 1.00
EPICS MSI
    extension substitutions
    filter remove_matches ^\s*#
    filter remove_inline #.*$
    3rd_gen_scale 1.00
EPICS IOCsh
    extension cmd
    filter remove_matches ^\s*#
    filter remove_inline #.*$
    3rd_gen_scale 1.00
EOF

cloc --force-lang-def="$TDIR/def" "$@"

#!/bin/sh

set -e

INPUT=$1
OUTPUT=$2
IMAGEMAGICK=$3

FILENAME=$(basename -- "$INPUT")
EXTENSION="${FILENAME##*.}"
FILENAME="${FILENAME%.*}"

WIDTHS=(16 32 44 64 128 150 256)

for WIDTH in "${WIDTHS[@]}"
do
    rsvg-convert --output="${FILENAME}_${WIDTH}.png" -w $WIDTH "$INPUT"
done

PNGS=()
for WIDTH in "${WIDTHS[@]}"; do
    PNGS+=("${FILENAME}_${WIDTH}.png")
done

PNGS_ARG="${PNGS[@]}"

if [[ "$IMAGEMAGICK" == *convert ]]; then
    # We don't call use subcommand on a system with the legacy "convert" executable
    convert $PNGS_ARG "$OUTPUT"
else
    "$IMAGEMAGICK" convert $PNGS_ARG "$OUTPUT"
fi



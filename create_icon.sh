#!/bin/sh

set -e

INPUT=$1
OUTPUT=$2
IMAGEMAGICK=$3

FILENAME=$(basename -- "$INPUT")
EXTENSION="${FILENAME##*.}"
FILENAME="${FILENAME%.*}"

for WIDTH in 16 32 44 64 128 150 256
do
    rsvg-convert --output="${FILENAME}_${WIDTH}.png" -w $WIDTH "$INPUT"
done

if [[ "$IMAGEMAGICK" == *convert ]]; then
    # We don't call use subcommand on a system with the legacy "convert" executable
    convert *.png "$OUTPUT"
else
    "$IMAGEMAGICK" convert *.png "$OUTPUT"
fi



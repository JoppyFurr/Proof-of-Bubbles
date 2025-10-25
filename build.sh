#!/bin/sh
echo ""
echo "Bubbles for Master System Build Script"
echo "--------------------------------------"

sdcc="${HOME}/Code/sdcc-4.3.0/bin/sdcc"
devkitSMS="${HOME}/Code/devkitSMS"
SMSlib="${devkitSMS}/SMSlib"
ihx2sms="${devkitSMS}/ihx2sms/Linux/ihx2sms"
sneptile="./tools/Sneptile-0.10.0/Sneptile"

build_sneptile ()
{
    # Early return if we've already got an up-to-date build
    if [ -e $sneptile -a "./tools/Sneptile-0.10.0/source/main.c" -ot $sneptile ]
    then
        return
    fi

    echo "Building Sneptile..."
    (
        cd "tools/Sneptile-0.10.0"
        ./build.sh
    )
}

build_bubbles_for_master_system ()
{
    echo "Building Bubbles for Master System..."

    # Background palette [0] is blue (background colour)
    # Background palette [1] is black
    GAME_BACKGROUND_PALETTE="0x30 0x00"
    GAME_SPRITE_PALETTE="0x30 0x00"

    echo "  Generating tile data..."
    mkdir -p game_tile_data
    (
        ${sneptile} --sprites --output-dir game_tile_data \
            --sprite-palette ${GAME_SPRITE_PALETTE} \
            --background-palette ${GAME_BACKGROUND_PALETTE} \
            --panels 1x2,8 tiles/cursor.png \
            --panels 2x2,16 tiles/bubbles.png
    )

    mkdir -p build/code
    echo "  Compiling..."
    for file in main
    do
        # Don't recompile files that are already up to date
        if [ -e "./build/code/${file}.rel" -a "./source/${file}.c" -ot "./build/code/${file}.rel" ]
        then
            continue
        fi

        echo "   -> ${file}.c"
        ${sdcc} -c -mz80 --peep-file ${devkitSMS}/SMSlib/src/peep-rules.txt -I ${SMSlib}/src \
            -o "build/code/${file}.rel" "source/${file}.c" || exit 1
    done

    echo ""
    echo "  Linking..."
    ${sdcc} -o build/Bubbles.ihx -mz80 --no-std-crt0 --data-loc 0xC000 \
        ${devkitSMS}/crt0/crt0_sms.rel build/code/*.rel ${SMSlib}/SMSlib.lib || exit 1

    echo ""
    echo "  Generating ROM..."
    ${ihx2sms} build/Bubbles.ihx Bubbles.sms || exit 1

    echo ""
    echo "  Done"
}

# Normal build:
#   Recompile all code, but don't re-generate unchanged tile data.
#
# Clean build:
#   Regenerate everything.
#
# Fast build:
#   Only rebuild source files that have changed.
#   Changes to tiles or header files will not trigger source rebuilds.
if [ "${1}" = "clean" ]
then
    rm -rf game_tile_data
    rm -rf build
elif [ "${1}" != "fast" ]
then
    rm -rf build
fi

build_sneptile
build_bubbles_for_master_system

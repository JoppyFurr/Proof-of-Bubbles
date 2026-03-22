#!/bin/sh
echo ""
echo "Bubbles for Master System Build Script"
echo "--------------------------------------"

sdcc="${HOME}/Code/sdcc-4.3.0/bin/sdcc"
devkitSMS="${HOME}/Code/devkitSMS"
SMSlib="${devkitSMS}/SMSlib"
PSGlib="${devkitSMS}/PSGlib"
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

    echo "  Generating tile data..."
    mkdir -p title_tile_data
    (
        # Background palette [0] is blue (background colour)
        # Background palette [1] is white (font)
        # Background palette [2] is light yellow (font)
        # Background palette [3] is light orange (font)
        ${sneptile} --output-dir title_tile_data \
            --background-palette 0x30 0x3f 0x2f 0x1b \
            --sprite-palette 0x00 \
            --background tiles/title_screen.png
    )
    # Background palette [0] is blue (background colour)
    # Background palette [1] is dark green (grass)
    # Background palette [2] is light green (grass)
    # Sprite palette [0] is lighter blue (game-board background colour)
    # Sprite palette [1] is dark green (grass)
    # Sprite palette [2] is light green (grass)
    # Sprite palette [3] is Yellow (for uncrossable line)
    # Sprite palette [4] is Gold (for uncrossable line)
    # Note: With sprites being split across two banks, the sprite-palette is
    #       listed in full to keep it consistent across the two calls to Sneptile.
    GAME_BACKGROUND_PALETTE="0x30 0x08 0x0c"
    GAME_SPRITE_PALETTE="0x34 0x08 0x0c 0x0f 0x0b 0x2a 0x00 0x15 0x3f 0x3c 0x03 0x32 0x38 0x02 0x21 0x06"
    mkdir -p game_tile_data
    (
        ${sneptile} --sprites --output-dir game_tile_data \
            --sprite-palette ${GAME_SPRITE_PALETTE} \
            --background-palette ${GAME_BACKGROUND_PALETTE} \
            --background tiles/border.png \
            --panels 2x2,100 tiles/bubbles_grey.png \
            --background tiles/grass.png \
            tiles/next.png \
            tiles/pip.png \
            --background --panels 1x2,39 tiles/text.png
    )
    mkdir -p bubble_tile_data
    (
        ${sneptile} --sprites --output-dir bubble_tile_data \
            --sprite-palette ${GAME_SPRITE_PALETTE} \
            --panels 2x2,100 tiles/bubbles.png
    )

    mkdir -p build/code
    echo "  Compiling..."
    for file in main title text rng save
    do
        # Don't recompile files that are already up to date
        if [ -e "./build/code/${file}.rel" -a "./source/${file}.c" -ot "./build/code/${file}.rel" ]
        then
            continue
        fi

        echo "   -> ${file}.c"
        ${sdcc} -c -mz80 --peep-file ${devkitSMS}/SMSlib/src/peep-rules.txt -I ${SMSlib}/src -I ${PSGlib}/src \
            -o "build/code/${file}.rel" "source/${file}.c" || exit 1
    done

    # Asset banks
    for bank in 2 3 4
    do
        ${sdcc} -c -mz80 --constseg BANK_${bank} source/bank_${bank}.c -o build/bank_${bank}.rel
    done

    echo ""
    echo "  Linking..."
    ${sdcc} -o build/Bubbles.ihx -mz80 --no-std-crt0 --data-loc 0xC000 \
        -Wl-b_BANK_2=0x8000 -Wl-b_BANK_3=0x8000 -Wl-b_BANK_4=0x8000 \
        ${devkitSMS}/crt0/crt0_sms.rel \
        build/code/*.rel \
        ${SMSlib}/SMSlib.lib \
        ${PSGlib}/PSGlib.lib \
        build/bank_2.rel build/bank_3.rel build/bank_4.rel || exit 1

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

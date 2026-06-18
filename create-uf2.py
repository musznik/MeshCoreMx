#!/usr/bin/python3

# Adds PlatformIO post-processing to convert hex files to uf2 files

import sys

if __name__ == "__main__":
    print("This script is a PlatformIO extra script and should be run by PlatformIO, not directly.")
    print("Use: platformio run -e <env> or platformio run -e <env> -t create_uf2")
    sys.exit(1)

Import("env")

uf2_cmd = " ".join(
    [
        '"$PYTHONEXE"',
        '"$PROJECT_DIR/bin/uf2conv/uf2conv.py"',
        '-f', '0xADA52840',
        '-c', '"$BUILD_DIR/${PROGNAME}.hex"',
        '-o', '"$BUILD_DIR/${PROGNAME}.uf2"',
    ]
)

env.AddPostAction(
    "$BUILD_DIR/${PROGNAME}.hex",
    env.VerboseAction(uf2_cmd, "Building $BUILD_DIR/${PROGNAME}.uf2")
)

#!/bin/sh
find ./ports/ \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" \) -print0 | xargs -0 clang-format-16 -i -style=file
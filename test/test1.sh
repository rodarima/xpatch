#!/bin/sh
# Copyright (c) 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cp "$inputs/a1k.rnd" a.bin

cat > 1.xpatch <<EOF
Tests a 32 bits aligned replacement of a single word storing
the output in a different target file.

--- a.bin	foo
+++ b.bin	bar
@@ u8,u32 -0x100,1 +0x100,1 @@
- 0xb0601b74
+ 0xb0603412
EOF
cat 1.xpatch

sha1sum -c <<EOF
186dec1f0aa0d33d43c44fbb74afdf77d7b4e3d9  a.bin
EOF

xpatch < 1.xpatch
sha1sum -c <<EOF
186dec1f0aa0d33d43c44fbb74afdf77d7b4e3d9  a.bin
6788a9c3a63d5b8ad93795cf515371b48ae19449  b.bin
EOF

# Compute xdiff and reaply it again

xdiff a.bin b.bin | tee > 2.xpatch
cat 2.xpatch
rm b.bin
xpatch < 2.xpatch
sha1sum -c <<EOF
186dec1f0aa0d33d43c44fbb74afdf77d7b4e3d9  a.bin
6788a9c3a63d5b8ad93795cf515371b48ae19449  b.bin
EOF

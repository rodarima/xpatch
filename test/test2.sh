#!/bin/sh
# Copyright (c) 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cp "$inputs/b1k.rnd" a.bin

cat > 1.xpatch <<EOF
Tests a 32 bits aligned replacement of a single word storing
the output in a different target file. Mismatch input.

--- a.bin	foo
+++ b.bin	bar
@@ u8,u32 -0x100,1 +0x100,1 @@
- 0xb0601b74
+ 0xb0603412
EOF
cat 1.xpatch

sha1sum -c <<EOF
df1cec9a1ffabd26757a4db34649dcff375fd411  a.bin
EOF

# Must fail
xpatch < 1.xpatch || exit 0

# Didn't fail
exit 1

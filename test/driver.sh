#!/bin/sh
# Copyright (c) 2025 Rodrigo Arias Mallo <rodarima@gmail.com
# SPDX-License-Identifier: GPL-3.0-or-later

set -ex

dir=$(readlink -f ".")
testname="$dir/$1"
workdir="${testname}.dir"

export PATH="${XPATCH_BIN_DIR}:$PATH"
if [ -n "$XPATCH_PATH" ]; then
	export PATH="${XPATCH_PATH}:$PATH"
fi

export inputs=$(readlink -f "$XPATCH_INPUTS_DIR")

rm -rf "${workdir}"
mkdir -p "${workdir}"
cd "${workdir}"

. "$XPATCH_TEST_PATH"

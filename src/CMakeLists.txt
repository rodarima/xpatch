# Copyright (c) 2024-2025 Rodrigo Arias Mallo <rodarima@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(xpatch xpatch.c common.c)
add_executable(xdiff xdiff.c common.c)

install(TARGETS xdiff xpatch)
# TODO: Manual pages
#install(FILES xdiff.1 xpatch.1 DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")

/*
 * Altera Nios II helper routines header.
 *
 * Copyright (c) 2012 Chris Wulff <crwulff@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/lgpl-2.1.html>
 */

#include "def-helper.h"

/* Define this to enable tracing calls/returns */
/* #define CALL_TRACING */

#ifdef CALL_TRACING
DEF_HELPER_2(call_status, void, i32, i32)
DEF_HELPER_1(eret_status, void, i32)
DEF_HELPER_1(ret_status, void, i32)
#endif

DEF_HELPER_1(raise_exception, void, i32)

#if !defined(CONFIG_USER_ONLY)
DEF_HELPER_1(mmu_read, i32, i32)
DEF_HELPER_2(mmu_write, void, i32, i32)
#endif

DEF_HELPER_2(divs, i32, i32, i32)
DEF_HELPER_2(divu, i32, i32, i32)

DEF_HELPER_4(memalign, void, i32, i32, i32, i32)

#include "def-helper.h"


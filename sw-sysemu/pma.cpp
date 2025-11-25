/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

#include "emu_defines.h"

#if EMU_ERBIUM
#include "pma_er.cpp"
#elif EMU_ETSOC1
#include "pma_et.cpp"
#else
#error "Architecture not supported"
#endif


// Copyright 2024 mass (@mass)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* key matrix size */
#define MATRIX_ROWS 4 // 行数
#define MATRIX_COLS 4 // 列数
#define MATRIX_ROW_PINS { GP11, GP12, GP13, GP14}
#define MATRIX_COL_PINS { GP0, GP2, GP3, GP8}

/* SPI & PMW3360 settings. */
#define SPI_DRIVER SPID0
#define SPI_SCK_PIN GP6
#define SPI_MISO_PIN GP4
#define SPI_MOSI_PIN GP7
#define PMW33XX_CS_PINS { GP5, GP1 }




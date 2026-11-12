// piano: the keys and touch are the piano's own (live DT, below); the panel
// values are still the OnePlus 13T ones, only so PaganiPanel compiles, and
// pagani-panel stays off on piano.
// SPDX-License-Identifier: BSD-2-Clause-Patent
// PKX110 live.dtb and AA594_P_7_A0020 panel configuration; installed as kits/oneplus13t.
#pragma once
#define PAGANI_GCC_BASE 0x100000ULL
// piano touch (live DT spi@a88000/novatek@0, pianoPkg TouchProbe): NT36532 THP on QUP1 SE2, SPI pins 40-43 (qup1_se2,
// 6 mA, no pull), frame-ready IRQ GPIO 162, 19.2 MHz, apps SMMU stream 0xa3 like every QUP1 engine. The touch rails
// come up with the panel, and the chip has no reset line in the DT. The engine clock is voted like the 13T's SE4,
// with gcc_qupv3_wrap1_s2_clk (vote bit 24, halt 0x1827c) in place of s4.
#define PAGANI_TOUCH_NVT 1
#define PAGANI_TOUCH_SPI_FUNC 1
#define PAGANI_TOUCH_CLK_DIV 6
#define PAGANI_TOUCH_GCC_VOTE 0x013c0000u
#define PAGANI_TOUCH_GCC_BRANCHES {0x2315c,0x23148,0x23140,0x23144,0x1827c}
#define PAGANI_TOUCH_SE 0xa88000ULL
#define PAGANI_TOUCH_WRAPPER 0xac0000ULL
#define PAGANI_TOUCH_SMMU 0x15000000ULL
#define PAGANI_TOUCH_SMMU_SIZE 0x100000
#define PAGANI_TOUCH_SMMU_SID 0xa3
#define PAGANI_TLMM_GPIO(n) (0xf100000ULL+0x1000ULL*(n))
#define PAGANI_TOUCH_RESET 161
#define PAGANI_TOUCH_ATTN 162
#define PAGANI_TOUCH_LCD_ID 100   // panel vendor strap: 0 CSOT, 1 BOE
#define PAGANI_TOUCH_PINS {40,41,42,43,PAGANI_TOUCH_RESET,PAGANI_TOUCH_ATTN}
#define PAGANI_TOUCH_DRIVES {2,2,2,2,0,0}
#define PAGANI_TOUCH_RESET_MS 10
#define PAGANI_TOUCH_STARTUP_MS 100
#define PAGANI_TOUCH_HZ 19200000
#define PAGANI_TOUCH_CS_DELAY 50
#define PAGANI_TOUCH_WORD_DELAY 0
// Android live DT and cmd-db: apps DRV-2, ldob4 (0x40800), ldob16 (0x41900).
#define PAGANI_TOUCH_RSC 0x16520000ULL
#define PAGANI_TOUCH_RSC_SIZE 0x10000
#define PAGANI_CMD_DB 0x81c60000ULL
#define PAGANI_CMD_DB_SIZE 0x20000
#define PAGANI_TOUCH_VDD "ldob4"
#define PAGANI_TOUCH_VCC "ldob16"
#define PAGANI_TOUCH_VDD_VRM 0x40800
#define PAGANI_TOUCH_VCC_VRM 0x41900
#define PAGANI_TOUCH_VDD_MV 1800
#define PAGANI_TOUCH_VCC_MV 3100
#define PAGANI_TOUCH_POWER_MS 200
#define PAGANI_TOUCH_EDGE_PIXELS 48
#define PAGANI_SPMI_CORE 0xc400000ULL
#define PAGANI_SPMI_CNFG 0xc42d000ULL
#define PAGANI_SPMI_OBS 0xc440000ULL
#define PAGANI_SPMI_EE 0
// piano keys (live DT): pmk8550 PON (SID 0, 0x1300) KPDPWR bit 7 is Power and
// RESIN bit 6 is Volume Down; Volume Up is pm8550 (SID 1) GPIO 6, 0x8d00,
// active low (gpio-keys vol_up, debounce 32 ms). No Plus key.
#define PAGANI_PON_PPID 0x013
#define PAGANI_GPIO_PPID 0x18d
#define PAGANI_PON_POWER 0x80
#define PAGANI_PON_VOLUME_UP 0x40
#define PAGANI_PON_RESIN_VOLUME_DOWN 1
#define PAGANI_GPIO_VOLUME_DOWN 1
#define PAGANI_PLUS_KEY_DRIVE 2
#define PAGANI_DSI_BASE 0xae94000ULL
#define PAGANI_BRIGHTNESS_MAX 0xdbb
#define PAGANI_BRIGHTNESS_DEFAULT 0x599

/*
 * ╔══════════════════════════════════════════════════════════════╗
 * ║              PINAGEM CONGELADA v2.0                          ║
 * ║  Freenove ESP32-S3-WROOM CAM N16R8  ·  ESP-IDF v5.x         ║
 * ╚══════════════════════════════════════════════════════════════╝
 *
 * REGRAS OBRIGATÓRIAS:
 *   R1. Chamar i2c_master_init() ANTES de esp_camera_init()
 *   R2. Não acessar barramento I2C durante captura de frame (camera)
 *   R3. GPIO43 = UART0_TX, GPIO44 = UART0_RX — console serial (monitor).
 *       NÃO usar para outros periféricos.
 *   R4. GPIO45 é strapping — se usado no futuro, adicionar pull-down 10 kΩ
 *   R5. GPIO2 está conectado ao LED onboard (LED ON). A presença do LED
 *       pode degradar a sensibilidade do touch T2 — testar SNR na HW real.
 *   R6. GPIO19/20 são USB D-/D+. Livres enquanto USB CDC estiver desabilitado.
 *       ⚠ NÃO habilitar CONFIG_ESP_CONSOLE_USB_CDC — conflito com WS2812 e servo.
 *   R7. GPIO46 é strapping (UART ROM log). Usado como RX após boot — sem problemas
 *       pois é entrada passiva; não colocar pull-up externo forte neste pino.
 *   R8. GPIO48 (WS2812 onboard) deixado quieto — DOUT não acessível na placa,
 *       usar fita externa no GPIO19.
 *
 * CONFLITOS RESOLVIDOS:
 *   [C1] HAL_I2S_SCK migrado para GPIO41 (livre após ST7789_DC → GPIO45).
 *   [C2] Servos migrados de GPIO43/44 para GPIO20/46, liberando o console UART0.
 */

#pragma once

/* ──────────────────────────────────────────────────────────────
 * SPI — ST7789 via LovyanGFX (SPI2_HOST)
 * CS hardwired to GND — barramento dedicado, always selected
 * ────────────────────────────────────────────────────────────── */
#define HAL_SPI_MOSI        21
#define HAL_SPI_SCK         47
#define HAL_SPI_MISO        (-1) /* write-only — sem leitura necessária */
#define HAL_ST7789_DC       45
#define HAL_ST7789_CS       (-1) /* CS → GND físico; LovyanGFX: pin_cs = -1 */
#define HAL_ST7789_RST      (-1) /* sem reset hardware; SWRESET (0x01) + 120 ms */

/* ──────────────────────────────────────────────────────────────
 * SD card — SDMMC 1-bit onboard (pinos fixos de placa)
 * ────────────────────────────────────────────────────────────── */
#define HAL_SD_CMD          38
#define HAL_SD_CLK          39
#define HAL_SD_DATA0        40

/* ──────────────────────────────────────────────────────────────
 * UART0 — console serial (monitor)  [C2]
 * Pinos fixos do ESP32-S3; não redirecionar para outros periféricos.
 * ────────────────────────────────────────────────────────────── */
#define HAL_UART0_TX        43   /* UART0_TX — console */
#define HAL_UART0_RX        44   /* UART0_RX — console */

/* ──────────────────────────────────────────────────────────────
 * UART1 — servos SCS0009 × 2 via FE-TTLinker (full-duplex)
 *
 * Migrado de GPIO43/44 → GPIO20/46  [C2]
 * ⚠ GPIO20 = USB D+ quando USB CDC ativo — ver Regra R6
 * ⚠ GPIO46 = strapping de log ROM     — ver Regra R7
 * ────────────────────────────────────────────────────────────── */
#define HAL_UART1_TX        20
#define HAL_UART1_RX        46
#define HAL_UART1_PORT      UART_NUM_1
#define HAL_UART1_BAUD      1000000 /* SCS0009 padrão: 1 Mbps */

/* ──────────────────────────────────────────────────────────────
 * I2S full-duplex — INMP441 (mic) + MAX98357A (amp)
 * BCLK e WS compartilhados: I2S0 full-duplex, mesma taxa de amostragem
 * ────────────────────────────────────────────────────────────── */
#define HAL_I2S_SCK         41   /* BCLK compartilhado mic + amp — GPIO41 livre após DC→GPIO45 */
#define HAL_I2S_WS          42   /* LRCK/WS compartilhado mic + amp */
#define HAL_I2S_MIC_SD      14   /* INMP441  → dados RX (I2S data in) */
#define HAL_I2S_AMP_DIN     1    /* MAX98357A ← dados TX (I2S data out) */
#define HAL_I2S_MIC_PORT    I2S_NUM_0
#define HAL_I2S_AMP_PORT    I2S_NUM_1
/* ⚠ BCLK/WS compartilhados fisicamente (GPIO41/42).
 *   Uso simultâneo mic+amp requer refactor para I2S0 full-duplex. */
#define HAL_I2S_PORT        HAL_I2S_MIC_PORT  /* alias legado */

/* Aliases por periférico (mapeiam para os #defines acima) */
#define HAL_I2S_MIC_SCK     HAL_I2S_SCK
#define HAL_I2S_MIC_WS      HAL_I2S_WS
#define HAL_I2S_SPK_BCLK    HAL_I2S_SCK
#define HAL_I2S_SPK_LRC     HAL_I2S_WS
#define HAL_I2S_SPK_DIN     HAL_I2S_AMP_DIN

/* ──────────────────────────────────────────────────────────────
 * I2C — IMU · MAX17048 · bq25185  (barramento compartilhado)
 *
 * ⚠ GPIO4 = OV2640 SIOD (SCCB) — barramento físico compartilhado
 * ⚠ GPIO5 = OV2640 SIOC (SCCB) — barramento físico compartilhado
 *   Endereços reservados: 0x3C / 0x78 (OV2640)
 *   Sequência obrigatória: i2c_master_init() → esp_camera_init() (Regra R1)
 * ────────────────────────────────────────────────────────────── */
#define HAL_I2C_SDA         4
#define HAL_I2C_SCL         5
#define HAL_I2C_PORT        I2C_NUM_0
#define HAL_I2C_FREQ_HZ     400000

/* ──────────────────────────────────────────────────────────────
 * RMT — WS2812
 *
 * GPIO48 (onboard): deixado quieto — DOUT não acessível na placa.
 *   Não chamar ws2812_init() para este pino.  [R8]
 *
 * GPIO19 (fita externa): cadeia independente.
 *   Papel dos índices:
 *     0  — status do sistema (ws2812_set_state)
 *     1+ — livres para animações / behavior
 *
 * ⚠ GPIO19 = USB D- quando USB CDC ativo — ver Regra R6
 * ────────────────────────────────────────────────────────────── */
#define HAL_RMT_LED_ONBOARD    48   /* onboard — NÃO USAR, deixado quieto */
#define HAL_RMT_LED            19   /* fita externa — canal ativo          */
#define HAL_RMT_LED_COUNT      2    /* número de LEDs na fita externa       */
#define HAL_RMT_LED_STATUS_IDX 0    /* índice do LED de status na fita      */

/* ──────────────────────────────────────────────────────────────
 * Touch capacitivo nativo ESP32-S3 — fitas de cobre, 4 zonas
 *
 * Pinos touch-capable disponíveis com câmera ativa: T1, T2, T14
 * (apenas GPIO1-GPIO14 são touch no S3; acima disso não há touch).
 *
 * ZONA  PAD           GPIO  Status
 *   0   TOUCH_PAD_NUM2  2   Livre (⚠ LED onboard — SNR reduzido)
 *   1   TOUCH_PAD_NUM1  1   ⚠ compartilhado HAL_I2S_AMP_DIN
 *   2   TOUCH_PAD_NUM14 14  ⚠ compartilhado HAL_I2S_MIC_SD
 *   3   —               —   Sem GPIO touch disponível com câmera ativa
 *
 * Para 4 zonas independentes: desativar câmera OU redesenhar pinout.
 * ────────────────────────────────────────────────────────────── */
#define HAL_TOUCH_PIN_1         2              /* legado — zona 0 */

#define HAL_TOUCH_ZONE_BASE     TOUCH_PAD_NUM2  /* GPIO2  — livre       */
#define HAL_TOUCH_ZONE_TOP      TOUCH_PAD_NUM1  /* GPIO1  — ⚠ amp DIN  */
#define HAL_TOUCH_ZONE_LEFT     TOUCH_PAD_NUM14 /* GPIO14 — ⚠ mic SD   */
/* ZONE_RIGHT: pendente GPIO touch livre */

#define HAL_TOUCH_ZONE_COUNT    1   /* apenas GPIO2 conectado */

/* ──────────────────────────────────────────────────────────────
 * OV2640 — pinos fixos de placa (NÃO REDEFINIR)
 * Listados como referência para checagem de conflitos futuros.
 * ────────────────────────────────────────────────────────────── */
/* #define CAM_XCLK    15 */
/* #define CAM_PCLK    13 */
/* #define CAM_VSYNC    6 */
/* #define CAM_HREF     7 */
/* #define CAM_SIOD     4 */  /* = HAL_I2C_SDA (barramento compartilhado) */
/* #define CAM_SIOC     5 */  /* = HAL_I2C_SCL (barramento compartilhado) */
/* #define CAM_D0      11 */
/* #define CAM_D1       9 */
/* #define CAM_D2       8 */
/* #define CAM_D3      10 */
/* #define CAM_D4      12 */
/* #define CAM_D5      18 */
/* #define CAM_D6      17 */
/* #define CAM_D7      16 */

/*
 * ════════════════════════════════════════════════════════════════
 * TABELA DE GPIOs USADOS
 * ════════════════════════════════════════════════════════════════
 *
 *  GPIO │ Define                  │ Periférico            │ Nota
 *  ─────┼─────────────────────────┼───────────────────────┼──────────────────────
 *     1 │ HAL_I2S_AMP_DIN         │ MAX98357A TX data      │
 *     2 │ HAL_TOUCH_PIN_1         │ Touch T2 (nativo)      │ ⚠ LED onboard R5
 *     4 │ HAL_I2C_SDA             │ I2C / OV2640 SIOD      │ ⚠ compartilhado
 *     5 │ HAL_I2C_SCL             │ I2C / OV2640 SIOC      │ ⚠ compartilhado
 *    14 │ HAL_I2S_MIC_SD          │ INMP441 RX data        │
 *    19 │ HAL_RMT_LED             │ WS2812 fita externa    │ ⚠ USB D- se CDC ativo R6
 *    20 │ HAL_UART1_TX            │ Servo UART1 TX         │ ⚠ USB D+ se CDC ativo R6
 *    21 │ HAL_SPI_MOSI            │ ST7789 MOSI            │
 *    38 │ HAL_SD_CMD              │ SDMMC CMD              │ fixo placa
 *    39 │ HAL_SD_CLK              │ SDMMC CLK              │ fixo placa
 *    40 │ HAL_SD_DATA0            │ SDMMC DATA0            │ fixo placa
 *    41 │ HAL_I2S_SCK             │ I2S BCLK               │ [C1]
 *    42 │ HAL_I2S_WS              │ I2S LRCK (shared)      │
 *    43 │ HAL_UART0_TX            │ Console UART0 TX       │ monitor serial
 *    44 │ HAL_UART0_RX            │ Console UART0 RX       │ monitor serial
 *    45 │ HAL_ST7789_DC           │ ST7789 D/C             │ ⚠ strapping — ok pós-boot
 *    46 │ HAL_UART1_RX            │ Servo UART1 RX         │ ⚠ strapping log R7
 *    47 │ HAL_SPI_SCK             │ ST7789 SCK             │
 *    48 │ HAL_RMT_LED_ONBOARD     │ WS2812 onboard (quieto)│ DOUT inacessível R8
 * ─────┴─────────────────────────┴───────────────────────┴──────────────────────
 *  Livres  : nenhum
 *  CS/RST  : -1 (ST7789 CS hardwired GND; RST via SWRESET)
 *  PSRAM   : GPIO35-37 internos (indisponível)
 *  USB CDC : DESABILITADO — GPIO19/20 em uso para WS2812 e servo
 * ════════════════════════════════════════════════════════════════
 *
 * EXEMPLO LovyanGFX (Bus_SPI + Panel_ST7789):
 *
 *   auto bc = _bus.config();
 *   bc.spi_host    = SPI2_HOST;
 *   bc.freq_write  = 40000000;
 *   bc.freq_read   = 16000000;
 *   bc.spi_3wire   = false;          // DC em pino dedicado (GPIO41)
 *   bc.use_lock    = true;
 *   bc.dma_channel = SPI_DMA_CH_AUTO;
 *   bc.pin_sclk    = HAL_SPI_SCK;    // 47
 *   bc.pin_mosi    = HAL_SPI_MOSI;   // 21
 *   bc.pin_miso    = HAL_SPI_MISO;   // -1
 *   bc.pin_dc      = HAL_ST7789_DC;  // 41
 *   _bus.config(bc);
 *
 *   auto pc = _panel.config();
 *   pc.pin_cs      = HAL_ST7789_CS;  // -1 (GND)
 *   pc.pin_rst     = HAL_ST7789_RST; // -1
 *   pc.pin_busy    = -1;
 *   pc.panel_width  = 240;
 *   pc.panel_height = 240;
 *   _panel.config(pc);
 * ════════════════════════════════════════════════════════════════
 */

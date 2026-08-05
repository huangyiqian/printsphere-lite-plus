// ============================================================
// SD2 PrintSphere — TFT_eSPI 显示配置
// 硬件: SD2 小电视 (ESP8266 + ST7789 240×240)
//
// 关键: 不使用帧缓冲 (Framebuffer-less)
// 所有绘制直接通过 SPI 发送到 ST7789 的 GRAM
// ESP8266 的 80KB DRAM 足够运行
// ============================================================

// 驱动芯片
#define ST7789_2_DRIVER

// 屏幕分辨率
#define TFT_WIDTH  240
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// ----- GPIO 引脚 (SD2 小电视接线) -----
// 硬件 SPI: MOSI=GPIO13(D7), SCLK=GPIO14(D5)
// CS   → GPIO15 (D8)  片选
// DC   → GPIO0  (D3)  数据/命令
// RST  → GPIO2  (D4)  复位
// BL   → GPIO5  (D1)  背光
#define TFT_CS   15
#define TFT_DC   0
#define TFT_RST  2
#define TFT_BL   5

// MISO 未连接, 设为 -1
#define TFT_MISO -1
// MOSI/SCLK 使用 ESP8266 硬件 SPI 默认引脚, 无需显式定义

// ----- SPI 频率 -----
#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  20000000

// ----- 内建字体 (存储于 Flash, 不占 RAM) -----
#define LOAD_GLCD           // 5×7   像素字体
#define LOAD_FONT2          // 16×16 像素字体
#define LOAD_FONT4          // 26×26 像素字体
#define LOAD_FONT6          // 48×36 数字专用
#define LOAD_FONT7          // 48×48 7段数码管
#define LOAD_FONT8          // 75×55 7段数码管
#define LOAD_GFXFF          // FreeFonts 支持

// ----- 性能优化 (ESP8266 专用) -----
// 不启用抗锯齿字体 (省 Flash)
// #define SMOOTH_FONT       // 注释掉, 减少 Flash 占用

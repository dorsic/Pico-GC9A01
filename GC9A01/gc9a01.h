// https://raw.githubusercontent.com/russhughes/gc9a01_mpy/main/src/gc9a01.h

#ifndef __GC9A01_H__
#define __GC9A01_H__

#ifdef __cplusplus
extern "C" {
#endif


// color modes
#define COLOR_MODE_65K      0x50
#define COLOR_MODE_262K     0x60
#define COLOR_MODE_12BIT    0x03
#define COLOR_MODE_16BIT    0x05
#define COLOR_MODE_18BIT    0x06
#define COLOR_MODE_16M      0x07

// commands
#define GC9A01_NOP     0x00
#define GC9A01_SWRESET 0x01
#define GC9A01_RDDID   0x04
#define GC9A01_RDDST   0x09

#define GC9A01_SLPIN   0x10
#define GC9A01_SLPOUT  0x11
#define GC9A01_PTLON   0x12
#define GC9A01_NORON   0x13

#define GC9A01_INVOFF  0x20
#define GC9A01_INVON   0x21
#define GC9A01_DISPOFF 0x28
#define GC9A01_DISPON  0x29
#define GC9A01_CASET   0x2A
#define GC9A01_RASET   0x2B
#define GC9A01_RAMWR   0x2C
#define GC9A01_RAMRD   0x2E

#define GC9A01_PTLAR   0x30
#define GC9A01_VSCRDEF 0x33
#define GC9A01_COLMOD  0x3A
#define GC9A01_MADCTL  0x36
#define GC9A01_VSCSAD  0x37

#define GC9A01_MADCTL_MY  0x80  // Page Address Order
#define GC9A01_MADCTL_MX  0x40  // Column Address Order
#define GC9A01_MADCTL_MV  0x20  // Page/Column Order
#define GC9A01_MADCTL_ML  0x10  // Line Address Order
#define GC9A01_MADCTL_MH  0x04  // Display Data Latch Order
#define GC9A01_MADCTL_RGB 0x00
#define GC9A01_MADCTL_BGR 0x08

#define GC9A01_RDID1   0xDA
#define GC9A01_RDID2   0xDB
#define GC9A01_RDID3   0xDC
#define GC9A01_RDID4   0xDD

// Color definitions
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#ifdef  __cplusplus
}
#endif /*  __cplusplus */

#endif  /*  __GC9A01_H__ */

#include "hardware/spi.h"
#include "gfxfont.h"

// this is the actual C-structure for our new object
typedef struct _gc9a01_GC9A01_obj_t {
    spi_inst_t *spi_obj;
	uint16_t *i2c_buffer;		// resident buffer if buffer_size given
    void *work;                 // work buffer for jpg decoding
	ulong buffer_size;       // resident buffer size, 0=dynamic
    uint16_t display_width;     // physical width
    uint16_t width;             // logical width (after rotation)
    uint16_t display_height;    // physical width
    uint16_t height;            // logical height (after rotation)
    uint8_t xstart;
    uint8_t ystart;
    uint8_t rotation;
    int reset;
    int dc;
    int cs;
    int backlight;
} gc9a01_GC9A01_obj_t;

void gc9a01_init(gc9a01_GC9A01_obj_t *self);
void gc9a01_on(gc9a01_GC9A01_obj_t *self);
void gc9a01_fill(gc9a01_GC9A01_obj_t *self, uint16_t color);
void gc9a01_draw_pixel(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t color);
void gc9a01_rect(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void gc9a01_fill_rect(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void gc9a01_set_rotation(gc9a01_GC9A01_obj_t *self);
void gc9a01_hline(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void gc9a01_vline(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void gc9a01_text(gc9a01_GC9A01_obj_t *self, GFXfont *font, char *str, uint16_t x0, uint16_t y0, uint16_t fg_color, uint16_t bg_color);
uint8_t gc9a01_get_color(uint8_t bpp);
uint16_t color565(uint8_t r, uint8_t g, uint8_t b);

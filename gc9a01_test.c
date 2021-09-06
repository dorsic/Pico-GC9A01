/* See https://github.com/russhughes/gc9a01_mpy/blob/main/fonts/bitmap/vga1_16x32.py
   for other fonts
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "GC9A01/gc9a01.h"
#include "GC9A01/VGA1_16x32.h"
#include "GC9A01/VGA1b_16x32.h"


int32_t t_fine;
uint16_t dig_T1;
int16_t dig_T2, dig_T3;

// LCD config

gc9a01_GC9A01_obj_t create_lcd() {
    spi_init(spi1, 10 * 1000000); // Initialise spi0 at 5000kHz
    //Initialise GPIO pins for SPI communication
    gpio_set_function(12, GPIO_FUNC_SPI);
    gpio_set_function(14, GPIO_FUNC_SPI);
    gpio_set_function(15, GPIO_FUNC_SPI);
    // Configure Chip Select
    gpio_init(13); // Initialise CS Pin
    gpio_set_dir(13, GPIO_OUT); // Set CS as output
    gpio_put(13, 1); // Set CS High to indicate no currect SPI communication
    gpio_init(11); // Initialise RES Pin
    gpio_set_dir(11, GPIO_OUT); // Set RES as output
    gpio_put(11, 0);
    gpio_init(10); // Initialise DC Pin
    gpio_set_dir(10, GPIO_OUT); // Set DC as output
    gpio_put(10, 0);
    gpio_init(9); // Initialise BLK Pin
    gpio_set_dir(9, GPIO_OUT); // Set BLK as output
    gpio_put(9, 0);

    gc9a01_GC9A01_obj_t lcd;
    lcd.spi_obj = spi1;
    lcd.reset = 11;
    lcd.dc = 10;
    lcd.cs = 13;
    lcd.backlight = 9;
    lcd.xstart = 0;
    lcd.ystart = 0;

    lcd.display_width = 240;
    lcd.display_height = 240;
    lcd.rotation = 2;
    lcd.buffer_size = 2048;
    lcd.i2c_buffer = malloc(2048);
    return lcd;
}


int main() {
    int8_t rslt;
    char buf[10];

    GFXfont font = VGA1_16x32;
    GFXfont font_bold = VGA1b_16x32;
    gc9a01_GC9A01_obj_t lcd = create_lcd();
    uint16_t color = BLACK;

    gc9a01_init(&lcd);
    sleep_ms(100);
    gc9a01_fill(&lcd, 0);
    while (true) {
        
        sprintf(buf, "%.2f C", 23.4);
        gc9a01_text(&lcd, &font_bold, buf, 70, 100, CYAN, 0);
        sprintf(buf, "%.2f C", 20.0);
        gc9a01_text(&lcd, &font, buf, 75, 150, WHITE, 0);

        sprintf(buf, "%.2f hPa", 1013.25);
        gc9a01_text(&lcd, &font, buf, 55, 50, WHITE, 0);
        color = (color == BLACK) ? MAGENTA : BLACK;
        for (int i = 720; i>0; i--) {
            int x = 120+(int)(119.0*sin((180.0+i/2.0)*M_TWOPI/360.0));
            int y = 120+(int)(119.0*cos((180.0+i/2.0)*M_TWOPI/360.0));
            gc9a01_draw_pixel(&lcd, x, y, color);
        }
    }
    free(lcd.i2c_buffer);
}
// https://github.com/russhughes/gc9a01_mpy/blob/main/src/gc9a01.c

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ivan Belokobylskiy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#define __GC9A01_VERSION__  "0.1.5"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "gc9a01.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }
#define _swap_bytes(val) ( (((val)>>8)&0x00FF)|(((val)<<8)&0xFF00) )

#define ABS(N) (((N)<0)?(-(N)):(N))

#define CS_LOW()     { if(self->cs) {gpio_put(self->cs, 0);} }
#define CS_HIGH()    { if(self->cs) {gpio_put(self->cs, 1);} }
#define DC_LOW()     (gpio_put(self->dc, 0))
#define DC_HIGH()    (gpio_put(self->dc, 1))
#define RESET_LOW()  { if (self->reset) gpio_put(self->reset, 0); }
#define RESET_HIGH() { if (self->reset) gpio_put(self->reset, 1); }
#define DISP_HIGH()  { if (self->backlight) gpio_put(self->backlight, 1); }
#define DISP_LOW()   { if (self->backlight) gpio_put(self->backlight, 0); }

void gc9a01_write_spi(spi_inst_t *spi_obj, const uint8_t *buf, int len, int cs) {
//    buf[0] = buf[0] & 0x7F; // indicate write operation
    gpio_put(cs, 0); // Indicate beginning of communication
    spi_write_blocking(spi_obj, buf, len); // Send data[]
    gpio_put(cs, 1); // Signal end of communication
}

/* methods start */

void gc9a01_write_cmd(gc9a01_GC9A01_obj_t *self, uint8_t cmd, const uint8_t *data, int len) {
    CS_LOW()
    if (cmd) {
        DC_LOW();
        gc9a01_write_spi(self->spi_obj, &cmd, 1, self->cs);
    }
    if (len > 0) {
        DC_HIGH();
        gc9a01_write_spi(self->spi_obj, data, len, self->cs);
    }
    CS_HIGH()
}


void gc9a01_set_window(gc9a01_GC9A01_obj_t *self, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    if (x0 > x1 || x1 >= self->width) {
        return;
    }
    if (y0 > y1 || y1 >= self->height) {
        return;
    }
    uint8_t bufx[4] = {(x0+self->xstart) >> 8, (x0+self->xstart) & 0xFF, (x1+self->xstart) >> 8, (x1+self->xstart) & 0xFF};
    uint8_t bufy[4] = {(y0+self->ystart) >> 8, (y0+self->ystart) & 0xFF, (y1+self->ystart) >> 8, (y1+self->ystart) & 0xFF};
    gc9a01_write_cmd(self, GC9A01_CASET, bufx, 4);
    gc9a01_write_cmd(self, GC9A01_RASET, bufy, 4);
    gc9a01_write_cmd(self, GC9A01_RAMWR, NULL, 0);
}

void gc9a01_fill_color_buffer(spi_inst_t* spi_obj, uint16_t color, int length, int cs) {
    const int buffer_pixel_size = 128;
    int chunks = length / buffer_pixel_size;
    int rest = length % buffer_pixel_size;
    uint16_t color_swapped = _swap_bytes(color);
    uint16_t buffer[buffer_pixel_size]; // 128 pixels

    // fill buffer with color data
    for (int i = 0; i < length && i < buffer_pixel_size; i++) {
        buffer[i] = color_swapped;
    }
    if (chunks) {
        for (int j = 0; j < chunks; j ++) {
            gc9a01_write_spi(spi_obj, (uint8_t *)buffer, buffer_pixel_size*2, cs);
        }
    }
    if (rest) {
        gc9a01_write_spi(spi_obj, (uint8_t *)buffer, rest*2, cs);
    }
}

void gc9a01_draw_pixel(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t color) {
    uint8_t hi = color >> 8, lo = color;
    gc9a01_set_window(self, x, y, x, y);
    DC_HIGH();
    CS_LOW();
    gc9a01_write_spi(self->spi_obj, &hi, 1, self->cs);
    gc9a01_write_spi(self->spi_obj, &lo, 1, self->cs);
    CS_HIGH();
}


void gc9a01_fast_hline(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t _w, uint16_t color) {
    int w;
    if (x+_w > self->width)
        w = self->width - x;
    else
        w = _w;

    if (w>0) {
        gc9a01_set_window(self, x, y, x + w - 1, y);
        DC_HIGH();
        CS_LOW();
        gc9a01_fill_color_buffer(self->spi_obj, color, w, self->cs);
        CS_HIGH();
    }
}

void gc9a01_fast_vline(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
    gc9a01_set_window(self, x, y, x, y + w - 1);
    DC_HIGH();
    CS_LOW();
    gc9a01_fill_color_buffer(self->spi_obj, color, w, self->cs);
    CS_HIGH();
}


void gc9a01_hard_reset(gc9a01_GC9A01_obj_t *self) {
    CS_LOW();
    RESET_HIGH();
    sleep_ms(50);
    RESET_LOW();
    sleep_ms(50);
    RESET_HIGH();
    sleep_ms(150);
    CS_HIGH();
}

void gc9a01_soft_reset(gc9a01_GC9A01_obj_t *self) {
    gc9a01_write_cmd(self, GC9A01_SWRESET, NULL, 0);
    sleep_ms(150);
}

void gc9a01_sleep_mode(gc9a01_GC9A01_obj_t *self, int value) {
    if(value) {
        gc9a01_write_cmd(self, GC9A01_SLPIN, NULL, 0);
    } else {
        gc9a01_write_cmd(self, GC9A01_SLPOUT, NULL, 0);
    }
}


void gc9a01_inversion_mode(gc9a01_GC9A01_obj_t *self, int value) {
    if(value) {
        gc9a01_write_cmd(self, GC9A01_INVON, NULL, 0);
    } else {
        gc9a01_write_cmd(self, GC9A01_INVOFF, NULL, 0);
    }
}

void gc9a01_fill_rect(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {

    uint16_t right = x + w - 1;
    uint16_t bottom = y + h - 1;

    if (x < self->width && y < self->height) {
        if (right > self->width)
            right = self->width;

        if (bottom > self->height)
            bottom = self->height;

        gc9a01_set_window(self, x, y, right, bottom);
        DC_HIGH();
        CS_LOW();
        gc9a01_fill_color_buffer(self->spi_obj, color, w * h, self->cs);
        CS_HIGH();
    }
}

void gc9a01_fill(gc9a01_GC9A01_obj_t *self, uint16_t color) {
    gc9a01_set_window(self, 0, 0, self->width - 1, self->height - 1);
    DC_HIGH();
    CS_LOW();
    gc9a01_fill_color_buffer(self->spi_obj, color, self->width * self->height, self->cs);
    CS_HIGH();

}

void gc9a01_pixel(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t color) {
    gc9a01_draw_pixel(self, x, y, color);
}

void gc9a01_line(gc9a01_GC9A01_obj_t *self, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color) {
	bool steep = ABS(y1 - y0) > ABS(x1 - x0);
	if (steep) {
		_swap_int16_t(x0, y0);
		_swap_int16_t(x1, y1);
	}

	if (x0 > x1) {
		_swap_int16_t(x0, x1);
		_swap_int16_t(y0, y1);
	}

	int16_t dx = x1 - x0, dy = ABS(y1 - y0);
	int16_t err = dx >> 1, ystep = -1, xs = x0, dlen = 0;

	if (y0 < y1)
		ystep = 1;

	// Split into steep and not steep for FastH/V separation
	if (steep) {
		for (; x0 <= x1; x0++) {
			dlen++;
			err -= dy;
			if (err < 0) {
				err += dx;
				if (dlen == 1)
					gc9a01_draw_pixel(self, y0, xs, color);
				else
					gc9a01_fast_vline(self, y0, xs, dlen, color);
				dlen = 0;
				y0 += ystep;
				xs = x0 + 1;
			}
		}
		if (dlen)
			gc9a01_fast_vline(self, y0, xs, dlen, color);
	} else {
		for (; x0 <= x1; x0++) {
			dlen++;
			err -= dy;
			if (err < 0) {
				err += dx;
				if (dlen == 1)
					gc9a01_draw_pixel(self, xs, y0, color);
				else
					gc9a01_fast_hline(self, xs, y0, dlen, color);
				dlen = 0;
				y0 += ystep;
				xs = x0 + 1;
			}
		}
		if (dlen)
			gc9a01_fast_hline(self, xs, y0, dlen, color);
	}
}

void gc9a01_blit_buffer(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *buf, int len) {
    gc9a01_set_window(self, x, y, x + w - 1, y + h - 1);
    DC_HIGH();
    CS_LOW();

    const int buf_size = 256;
    int limit = MIN(len, w * h * 2);
    int chunks = limit / buf_size;
    int rest = limit % buf_size;
    int i = 0;
    for (; i < chunks; i ++) {
        gc9a01_write_spi(self->spi_obj, (uint8_t*)buf + i*buf_size, buf_size, self->cs);
    }
    if (rest) {
        gc9a01_write_spi(self->spi_obj, (uint8_t*)buf + i*buf_size, rest, self->cs);
    }
    CS_HIGH();
}

/*
#define LROUND(x) (lround(x))

void gc9a01_GC9A01_draw(gc9a01_GC9A01_obj_t *self, uint16_t herskey, char *s, uint16_t x, uint16_t y, uint16_t color, float scale) {
	char		single_char_s[] = {0, 0};

	mp_obj_module_t *hershey   = MP_OBJ_TO_PTR(args[1]);

	mp_obj_dict_t *	 dict = MP_OBJ_TO_PTR(hershey->globals);
	mp_obj_t *		 index_data_buff = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_INDEX));
	mp_buffer_info_t index_bufinfo;
	mp_get_buffer_raise(index_data_buff, &index_bufinfo, MP_BUFFER_READ);
	uint8_t *index = index_bufinfo.buf;

	mp_obj_t *		 font_data_buff = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_FONT));
	mp_buffer_info_t font_bufinfo;
	mp_get_buffer_raise(font_data_buff, &font_bufinfo, MP_BUFFER_READ);
	int8_t *font = font_bufinfo.buf;

    int16_t from_x = x;
    int16_t from_y  = y;
    int16_t to_x = x;
    int16_t to_y = y;
    int16_t pos_x = x;
    int16_t pos_y = y;
    bool penup = true;
    char c;
    int16_t ii;

    while ((c = *s++)) {
        if (c >= 32 && c <= 127) {
            ii = (c-32) * 2;

			int16_t offset = index[ii] | (index[ii+1] << 8);
            int16_t length = font[offset++];
            int16_t left = LROUND((font[offset++] - 0x52) * scale);
            int16_t right = LROUND((font[offset++] - 0x52) * scale);
            int16_t width = right - left;

            if (length) {
                int16_t i;
                for (i = 0; i < length; i++) {
                    if (font[offset] == ' ') {
                        offset+=2;
                        penup = true;
                        continue;
                    }

                    int16_t vector_x = LROUND((font[offset++] - 0x52) * scale);
                    int16_t vector_y = LROUND((font[offset++] - 0x52) * scale);

                    if (!i ||  penup) {
                        from_x = pos_x + vector_x - left;
                        from_y = pos_y + vector_y;
                    } else {
                        to_x = pos_x + vector_x - left;
                        to_y = pos_y + vector_y;

                        gc9a01_line(self, from_x, from_y, to_x, to_y, color);
                        from_x = to_x;
                        from_y = to_y;
                    }
                    penup = false;
                }
            }
            pos_x += width;
        }
    }
}
*/

uint32_t bs_bit = 0;
uint8_t *bitmap_data = NULL;

uint8_t gc9a01_get_color(uint8_t bpp) {
	uint8_t color = 0;
	int		i;

	for (i = 0; i < bpp; i++) {
		color <<= 1;
		color |= (bitmap_data[bs_bit / 8] & 1 << (7 - (bs_bit % 8))) > 0;
		bs_bit++;
	}
	return color;
}

/*
mp_obj_t dict_lookup(mp_obj_t self_in, mp_obj_t index) {
    mp_obj_dict_t *self = MP_OBJ_TO_PTR(self_in);
    mp_map_elem_t *elem = mp_map_lookup(&self->map, index, MP_MAP_LOOKUP);
    if (elem == NULL) {
        return NULL;
    } else {
        return elem->value;
    }
}

STATIC mp_obj_t gc9a01_GC9A01_write_len(size_t n_args, const mp_obj_t *args) {
	mp_obj_module_t *font = MP_OBJ_TO_PTR(args[1]);
	char single_char_s[2] = {0, 0};
	const char *str;

	if (mp_obj_is_int(args[2])) {
		mp_int_t c = mp_obj_get_int(args[2]);
		single_char_s[0] = c & 0xff;
		str	= single_char_s;
	} else {
		str = mp_obj_str_get_str(args[2]);
	}

	mp_obj_dict_t *dict	= MP_OBJ_TO_PTR(font->globals);
	const char 	  *map  = mp_obj_str_get_str(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_MAP)));

	mp_obj_t widths_data_buff = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_WIDTHS));
	mp_buffer_info_t widths_bufinfo;
	mp_get_buffer_raise(widths_data_buff, &widths_bufinfo, MP_BUFFER_READ);
	const uint8_t *widths_data = widths_bufinfo.buf;

	uint print_width = 0;
	uint8_t chr;

	while ((chr = *str++)) {
		char *char_pointer = strchr(map, chr);
		if (char_pointer) {
			uint char_index = char_pointer - map;
			print_width += widths_data[char_index];
        }
    }
	return mp_obj_new_int(print_width);
}
*/

//
//	write(font_module, s, x, y[, fg, bg])
//
/*
void write(size_t n_args, const mp_obj_t *args) {
	gc9a01_GC9A01_obj_t *self = MP_OBJ_TO_PTR(args[0]);
	mp_obj_module_t *font = MP_OBJ_TO_PTR(args[1]);

	char single_char_s[2] = {0, 0};
	const char *str;

	if (mp_obj_is_int(args[2])) {
		mp_int_t c = mp_obj_get_int(args[2]);
		single_char_s[0] = c & 0xff;
		str	= single_char_s;
	} else {
		str = mp_obj_str_get_str(args[2]);
	}

	mp_int_t x = mp_obj_get_int(args[3]);
	mp_int_t y = mp_obj_get_int(args[4]);
	mp_int_t fg_color;
	mp_int_t bg_color;

	fg_color = (n_args > 5) ? _swap_bytes(mp_obj_get_int(args[5])) : _swap_bytes(WHITE);
	bg_color = (n_args > 6) ? _swap_bytes(mp_obj_get_int(args[6])) : _swap_bytes(BLACK);

	mp_obj_dict_t *dict			  = MP_OBJ_TO_PTR(font->globals);
	const char 	  *map 			  = mp_obj_str_get_str(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_MAP)));
	const uint8_t  bpp			  = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_BPP)));
	const uint8_t  height		  = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_HEIGHT)));
	const uint8_t  offset_width	  = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_OFFSET_WIDTH)));
	const uint8_t  max_width	  = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_MAX_WIDTH)));

	mp_obj_t widths_data_buff = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_WIDTHS));
	mp_buffer_info_t widths_bufinfo;
	mp_get_buffer_raise(widths_data_buff, &widths_bufinfo, MP_BUFFER_READ);
	const uint8_t *widths_data = widths_bufinfo.buf;

	mp_obj_t offsets_data_buff = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_OFFSETS));
	mp_buffer_info_t offsets_bufinfo;
	mp_get_buffer_raise(offsets_data_buff, &offsets_bufinfo, MP_BUFFER_READ);
	const uint8_t *offsets_data = offsets_bufinfo.buf;

	mp_obj_t bitmaps_data_buff = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_BITMAPS));
	mp_buffer_info_t bitmaps_bufinfo;
	mp_get_buffer_raise(bitmaps_data_buff, &bitmaps_bufinfo, MP_BUFFER_READ);
	bitmap_data = bitmaps_bufinfo.buf;

	ulong buf_size = max_width * height * 2;
	if (self->buffer_size == 0) {
		self->i2c_buffer = m_malloc(buf_size);
	}

	uint print_width = 0;
	uint8_t chr;

	while ((chr = *str++)) {
		char *char_pointer = strchr(map, chr);
		if (char_pointer) {
			uint char_index = char_pointer - map;
			uint8_t width = widths_data[char_index];

			bs_bit = 0;
			switch (offset_width) {
				case 1:
					bs_bit = offsets_data[char_index * offset_width];
					break;

				case 2:
					bs_bit = (offsets_data[char_index * offset_width] << 8) +
                			 (offsets_data[char_index * offset_width + 1]);
					break;

				case 3:
					bs_bit = (offsets_data[char_index * offset_width] << 16) +
                		     (offsets_data[char_index * offset_width + 1] << 8) +
                		     (offsets_data[char_index * offset_width + 2]);
					break;
			}

			ulong ofs = 0;
			for (int yy = 0; yy < height; yy++) {
				for (int xx = 0; xx < width; xx++) {
					self->i2c_buffer[ofs++] = get_color(bpp) ? fg_color : bg_color;
				}
			}

			ulong data_size = width * height * 2;
			uint x1 = x + width - 1;
			if (x1 < self->width) {
				set_window(self, x, y, x1, y + height - 1);
				DC_HIGH();
				CS_LOW();
				write_spi(self->spi_obj, (uint8_t *) self->i2c_buffer, data_size, self->cs);
				CS_HIGH();
				print_width += width;
			}
			else
				break;

			x += width;
		}
	}

	if (self->buffer_size == 0) {
		m_free(self->i2c_buffer);
	}

	return mp_obj_new_int(print_width);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(gc9a01_GC9A01_write_obj, 5, 7, gc9a01_GC9A01_write);


void bitmap(size_t n_args, const mp_obj_t *args) {
	gc9a01_GC9A01_obj_t *self = MP_OBJ_TO_PTR(args[0]);

	mp_obj_module_t *bitmap		 = MP_OBJ_TO_PTR(args[1]);
	mp_int_t		 x			 = mp_obj_get_int(args[2]);
	mp_int_t		 y			 = mp_obj_get_int(args[3]);

    mp_int_t idx;
    if (n_args > 3) {
        idx = mp_obj_get_int(args[4]);
    } else {
        idx = 0;
    }

	mp_obj_dict_t *	 dict		 = MP_OBJ_TO_PTR(bitmap->globals);
	const uint	 height		 = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_HEIGHT)));
	const uint	 width		 = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_WIDTH)));
    uint         bitmaps     = 0;
	const uint8_t	 bpp		 = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_BPP)));
	mp_obj_t *		 palette_arg = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_PALETTE));
	mp_obj_t *		 palette	 = NULL;
	size_t			 palette_len = 0;

    mp_map_elem_t *elem = dict_lookup(bitmap->globals, MP_OBJ_NEW_QSTR(MP_QSTR_BITMAPS));
    if (elem) {
        bitmaps = mp_obj_get_int(elem);
    }

	mp_obj_get_array(palette_arg, &palette_len, &palette);

	mp_obj_t *		 bitmap_data_buff = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_BITMAP));
	mp_buffer_info_t bufinfo;

	mp_get_buffer_raise(bitmap_data_buff, &bufinfo, MP_BUFFER_READ);
	bitmap_data = bufinfo.buf;

	ulong buf_size = width * height * 2;
	if (self->buffer_size == 0) {
		self->i2c_buffer = m_malloc(buf_size);
	}

	ulong ofs = 0;
    bs_bit = 0;
    if (bitmaps) {
        if (idx < bitmaps ) {
            bs_bit = height * width * bpp * idx;
        } else {
            mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("index out of range"));
        }
    }

	for (int yy = 0; yy < height; yy++) {
		for (int xx = 0; xx < width; xx++) {
			self->i2c_buffer[ofs++] = mp_obj_get_int(palette[get_color(bpp)]);
		}
	}

	uint x1 = x + width - 1;
	if (x1 < self->width) {
		set_window(self, x, y, x1, y + height - 1);
		DC_HIGH();
		CS_LOW();
		write_spi(self->spi_obj, (uint8_t *) self->i2c_buffer, buf_size, self->cs);
		CS_HIGH();
	}

	if (self->buffer_size == 0) {
		m_free(self->i2c_buffer);
	}
	return mp_const_none;
}

STATIC mp_obj_t gc9a01_GC9A01_pbitmap(size_t n_args, const mp_obj_t *args) {
	char single_char_s[2] = { 0, 0};
    const char *str;

    gc9a01_GC9A01_obj_t *self = MP_OBJ_TO_PTR(args[0]);

	mp_obj_module_t *bitmap		 = MP_OBJ_TO_PTR(args[1]);

    if (mp_obj_is_int(args[2])) {
        mp_int_t c = mp_obj_get_int(args[2]);
        single_char_s[0] = c & 0xff;
        str = single_char_s;
    } else {
        str = mp_obj_str_get_str(args[2]);
    }

	mp_int_t		 x			 = mp_obj_get_int(args[3]);
	mp_int_t		 y			 = mp_obj_get_int(args[4]);

	mp_obj_dict_t *	 dict		 = MP_OBJ_TO_PTR(bitmap->globals);
	const uint	 height		 = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_HEIGHT)));
	const char *     map         = mp_obj_str_get_str(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_MAP)));
    uint         bitmaps     = strlen(map);
	const uint8_t	 bpp		 = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_BPP)));
	mp_obj_t *		 palette_arg = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_PALETTE));
	mp_obj_t *		 palette	 = NULL;
	size_t			 palette_len = 0;

    // OFFSETS - 16 bit ints
    mp_obj_t ofs_data_buff = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_OFFSETS));
    mp_buffer_info_t ofs_bufinfo;
    mp_get_buffer_raise(ofs_data_buff, &ofs_bufinfo, MP_BUFFER_READ);
    const uint8_t *ofs_data = ofs_bufinfo.buf;

    // WIDTHS - 8 bit bytes
    mp_obj_t widths_data_buff = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_WIDTHS));
    mp_buffer_info_t widths_bufinfo;
    mp_get_buffer_raise(widths_data_buff, &widths_bufinfo, MP_BUFFER_READ);
    const uint8_t *widths = widths_bufinfo.buf;

	mp_obj_get_array(palette_arg, &palette_len, &palette);

	mp_obj_t *		 bitmap_data_buff = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_BITMAP));
	mp_buffer_info_t bitmap_bufinfo;
	mp_get_buffer_raise(bitmap_data_buff, &bitmap_bufinfo, MP_BUFFER_READ);
	bitmap_data = bitmap_bufinfo.buf;

	ulong ofs = 0;
    uint idx = 0;
    uint8_t chr;
    uint8_t width = 0;

    while ((chr = *str++)) {
        idx = 0;

        while (idx < bitmaps) {
            if (map[idx] == chr)
                break;
            idx++;
        }

        if (idx > bitmaps ) {
            mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("index out of range"));
        }

        bs_bit = (ofs_data[idx*2] << 8) + ofs_data[idx*2+1];
        width = widths[idx];

        ulong buf_size = width * height * 2;
	    if (self->buffer_size == 0) {
		    self->i2c_buffer = m_malloc(buf_size);
	    }

        for (int yy = 0; yy < height; yy++) {
            for (int xx = 0; xx < width; xx++) {
                self->i2c_buffer[ofs++] = mp_obj_get_int(palette[get_color(bpp)]);
            }
        }

        uint x1 = x + width - 1;
        if (x1 < self->width) {
            set_window(self, x, y, x1, y + height - 1);
            DC_HIGH();
            CS_LOW();
            write_spi(self->spi_obj, (uint8_t *) self->i2c_buffer, buf_size, self->cs);
            CS_HIGH();
        }

	    if (self->buffer_size == 0) {
		    m_free(self->i2c_buffer);
        }
	}

	return mp_const_none;
}
*/

void gc9a01_text(gc9a01_GC9A01_obj_t *self, GFXfont *font, char *str, uint16_t x0, uint16_t y0, uint16_t fg_color, uint16_t bg_color) {
//    const uint8_t width = font->glyph->width;
//    const uint8_t height = font->glyph->height;
    const uint8_t width = 16;
    const uint8_t height = 32;
    const uint16_t first = font->first;
    const uint16_t last = font->last;
    const uint8_t *font_data = font->bitmap;

    if (fg_color == 0) 
        fg_color = _swap_bytes(WHITE);

    if (bg_color == 0)
        bg_color = _swap_bytes(BLACK);

    uint8_t wide = width / 8;
    ulong buf_size = width * height * 2;

	if (self->buffer_size == 0) {
		self->i2c_buffer = malloc(buf_size);
	}

	if (self->i2c_buffer) {
        uint8_t chr;
        while ((chr = *str++)) {
            if (chr >= first && chr <= last) {
                uint buf_idx = 0;
                uint chr_idx = (chr-first)*(height*wide);
                for (uint8_t line = 0; line < height; line++) {
                    for (uint8_t line_byte = 0; line_byte < wide; line_byte++) {
                        uint8_t chr_data = font_data[chr_idx];
                        for (uint8_t bit = 8; bit; bit--) {
                            if (chr_data >> (bit-1) & 1)
                                self->i2c_buffer[buf_idx] = fg_color;
                            else
                                self->i2c_buffer[buf_idx] = bg_color;
                            buf_idx++;
                        }
                        chr_idx++;
                    }
                }
                uint x1 = x0+width-1;
                if (x1 < self->width) {
                    gc9a01_set_window(self, x0, y0, x1, y0+height-1);
                    DC_HIGH();
                    CS_LOW();
                    gc9a01_write_spi(self->spi_obj, (uint8_t *) self->i2c_buffer, buf_size, self->cs);
                    CS_HIGH();
                }
                x0 += width;
            }
        }
        if (self->buffer_size == 0) {
            free(self->i2c_buffer);
        }
    }
}

void gc9a01_set_rotation(gc9a01_GC9A01_obj_t *self) {
    uint8_t madctl_value = GC9A01_MADCTL_BGR;

	if (self->rotation == 0) {                  // Portrait
		madctl_value |= GC9A01_MADCTL_MX;
		self->width	 = self->display_width;
		self->height = self->display_height;
	} else if (self->rotation == 1) {           // Landscape
		madctl_value |=  GC9A01_MADCTL_MV;
		self->width	 = self->display_height;
		self->height = self->display_width;
	} else if (self->rotation == 2) {           // Inverted Portrait
		madctl_value |= GC9A01_MADCTL_MY;
		self->width	 = self->display_width;
		self->height = self->display_height;
	} else if (self->rotation == 3) {           // Inverted Landscape
		madctl_value |= GC9A01_MADCTL_MX | GC9A01_MADCTL_MY | GC9A01_MADCTL_MV;
		self->width	 = self->display_height;
		self->height = self->display_width;
	} else if (self->rotation == 4) {           // Portrait Mirrored
        self->width = self->display_width;
        self->height = self->display_height;
    } else if (self->rotation == 5) {           // Landscape Mirrored
        madctl_value |= GC9A01_MADCTL_MX | GC9A01_MADCTL_MV;
        self->width = self->display_height;
        self->height = self->display_width;
    } else if (self->rotation == 6) {           // Inverted Portrait Mirrored
        madctl_value |= GC9A01_MADCTL_MX | GC9A01_MADCTL_MY;
        self->width = self->display_width;
        self->height = self->display_height;
    } else if (self->rotation == 7) {           // Inverted Landscape Mirrored
        madctl_value |= GC9A01_MADCTL_MV | GC9A01_MADCTL_MY;
        self->width = self->display_height;
        self->height = self->display_width;
    }
    const uint8_t madctl[] = { madctl_value };
    gc9a01_write_cmd(self, GC9A01_MADCTL, madctl, 1);
}

uint16_t gc9a01_width(gc9a01_GC9A01_obj_t *self) {
    return self->width;
}


uint16_t gc9a01_height(gc9a01_GC9A01_obj_t *self) {
    return self->height;
}

void gc9a01_vscrdef(gc9a01_GC9A01_obj_t *self, uint16_t tfa, uint16_t vsa, uint16_t bfa) {
    uint8_t buf[6] = {(tfa) >> 8, (tfa) & 0xFF, (vsa) >> 8, (vsa) & 0xFF, (bfa) >> 8, (bfa) & 0xFF};
    gc9a01_write_cmd(self, GC9A01_VSCRDEF, buf, 6);
}

void gc9a01_vscsad(gc9a01_GC9A01_obj_t *self, uint16_t vssa) {
    uint8_t buf[2] = {(vssa) >> 8, (vssa) & 0xFF};
    gc9a01_write_cmd(self, GC9A01_VSCSAD, buf, 2);
}

void gc9a01_init(gc9a01_GC9A01_obj_t *self) {
    gc9a01_hard_reset(self);
    sleep_ms(100);

    gc9a01_soft_reset(self);
    sleep_ms(100);

    gc9a01_write_cmd(self, 0xEF, (const uint8_t *) NULL, 0);
	gc9a01_write_cmd(self, 0xEB, (const uint8_t *) "\x14", 1);
    gc9a01_write_cmd(self, 0xFE, (const uint8_t *) NULL, 0);
	gc9a01_write_cmd(self, 0xEF, (const uint8_t *) NULL, 0);
	gc9a01_write_cmd(self, 0xEB, (const uint8_t *) "\x14", 1);
	gc9a01_write_cmd(self, 0x84, (const uint8_t *) "\x40", 1);
	gc9a01_write_cmd(self, 0x85, (const uint8_t *) "\xFF", 1);
	gc9a01_write_cmd(self, 0x86, (const uint8_t *) "\xFF", 1);
	gc9a01_write_cmd(self, 0x87, (const uint8_t *) "\xFF", 1);
	gc9a01_write_cmd(self, 0x88, (const uint8_t *) "\x0A", 1);
	gc9a01_write_cmd(self, 0x89, (const uint8_t *) "\x21", 1);
	gc9a01_write_cmd(self, 0x8A, (const uint8_t *) "\x00", 1);
	gc9a01_write_cmd(self, 0x8B, (const uint8_t *) "\x80", 1);
	gc9a01_write_cmd(self, 0x8C, (const uint8_t *) "\x01", 1);
	gc9a01_write_cmd(self, 0x8D, (const uint8_t *) "\x01", 1);
	gc9a01_write_cmd(self, 0x8E, (const uint8_t *) "\xFF", 1);
	gc9a01_write_cmd(self, 0x8F, (const uint8_t *) "\xFF", 1);
	gc9a01_write_cmd(self, 0xB6, (const uint8_t *) "\x00\x00", 2);
	gc9a01_write_cmd(self, 0x3A, (const uint8_t *) "\x55", 1); // COLMOD
	gc9a01_write_cmd(self, 0x90, (const uint8_t *) "\x08\x08\x08\x08", 4);
	gc9a01_write_cmd(self, 0xBD, (const uint8_t *) "\x06", 1);
	gc9a01_write_cmd(self, 0xBC, (const uint8_t *) "\x00", 1);
	gc9a01_write_cmd(self, 0xFF, (const uint8_t *) "\x60\x01\x04", 3);
	gc9a01_write_cmd(self, 0xC3, (const uint8_t *) "\x13", 1);
	gc9a01_write_cmd(self, 0xC4, (const uint8_t *) "\x13", 1);
	gc9a01_write_cmd(self, 0xC9, (const uint8_t *) "\x22", 1);
	gc9a01_write_cmd(self, 0xBE, (const uint8_t *) "\x11", 1);
	gc9a01_write_cmd(self, 0xE1, (const uint8_t *) "\x10\x0E", 2);
	gc9a01_write_cmd(self, 0xDF, (const uint8_t *) "\x21\x0c\x02", 3);
	gc9a01_write_cmd(self, 0xF0, (const uint8_t *) "\x45\x09\x08\x08\x26\x2A", 6);
 	gc9a01_write_cmd(self, 0xF1, (const uint8_t *) "\x43\x70\x72\x36\x37\x6F", 6);
 	gc9a01_write_cmd(self, 0xF2, (const uint8_t *) "\x45\x09\x08\x08\x26\x2A", 6);
 	gc9a01_write_cmd(self, 0xF3, (const uint8_t *) "\x43\x70\x72\x36\x37\x6F", 6);
	gc9a01_write_cmd(self, 0xED, (const uint8_t *) "\x1B\x0B", 2);
	gc9a01_write_cmd(self, 0xAE, (const uint8_t *) "\x77", 1);
	gc9a01_write_cmd(self, 0xCD, (const uint8_t *) "\x63", 1);
	gc9a01_write_cmd(self, 0x70, (const uint8_t *) "\x07\x07\x04\x0E\x0F\x09\x07\x08\x03", 9);
	gc9a01_write_cmd(self, 0xE8, (const uint8_t *) "\x34", 1);
	gc9a01_write_cmd(self, 0x62, (const uint8_t *) "\x18\x0D\x71\xED\x70\x70\x18\x0F\x71\xEF\x70\x70", 12);
	gc9a01_write_cmd(self, 0x63, (const uint8_t *) "\x18\x11\x71\xF1\x70\x70\x18\x13\x71\xF3\x70\x70", 12);
	gc9a01_write_cmd(self, 0x64, (const uint8_t *) "\x28\x29\xF1\x01\xF1\x00\x07", 7);
	gc9a01_write_cmd(self, 0x66, (const uint8_t *) "\x3C\x00\xCD\x67\x45\x45\x10\x00\x00\x00", 10);
	gc9a01_write_cmd(self, 0x67, (const uint8_t *) "\x00\x3C\x00\x00\x00\x01\x54\x10\x32\x98", 10);
	gc9a01_write_cmd(self, 0x74, (const uint8_t *) "\x10\x85\x80\x00\x00\x4E\x00", 7);
    gc9a01_write_cmd(self, 0x98, (const uint8_t *) "\x3e\x07", 2);
	gc9a01_write_cmd(self, 0x35, (const uint8_t *) NULL, 0);
	gc9a01_write_cmd(self, 0x21, (const uint8_t *) NULL, 0);
	gc9a01_write_cmd(self, 0x11, (const uint8_t *) NULL, 0);
    sleep_ms(120);

	gc9a01_write_cmd(self, 0x29, (const uint8_t *) NULL, 0);
	sleep_ms(20);

    gc9a01_set_rotation(self);

    if (self->backlight)
        gpio_put(self->backlight, 1);

    gc9a01_write_cmd(self, GC9A01_DISPON, (const uint8_t *) NULL, 0);
    sleep_ms(120);
}

void gc9a01_on(gc9a01_GC9A01_obj_t *self) {
    DISP_HIGH();
    sleep_ms(10);
}

void gc9a01_off(gc9a01_GC9A01_obj_t *self) {
    DISP_LOW();
    sleep_ms(10);
}

void gc9a01_hline(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
    gc9a01_fast_hline(self, x, y, w, color);
}

void gc9a01_vline(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
    gc9a01_fast_vline(self, x, y, w, color);
}

void gc9a01_rect(gc9a01_GC9A01_obj_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    gc9a01_fast_hline(self, x, y, w, color);
    gc9a01_fast_vline(self, x, y, h, color);
    gc9a01_fast_hline(self, x, y + h - 1, w, color);
    gc9a01_fast_vline(self, x + w - 1, y, h, color);
}

void gc9a01_offset(gc9a01_GC9A01_obj_t *self, uint16_t xstart, uint16_t ystart) {
    self->xstart = xstart;
    self->ystart = ystart;
}

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

void map_bitarray_to_rgb565(uint8_t const *bitarray, uint8_t *buffer, int length, int width,
                                  uint color, uint bg_color) {
    int row_pos = 0;
    for (int i = 0; i < length; i++) {
        uint8_t byte = bitarray[i];
        for (int bi = 7; bi >= 0; bi--) {
            uint8_t b = byte & (1 << bi);
            uint cur_color = b ? color : bg_color;
            *buffer = (cur_color & 0xff00) >> 8;
            buffer++;
            *buffer = cur_color & 0xff;
            buffer++;

            row_pos++;
            if (row_pos >= width) {
                row_pos = 0;
                break;
            }
        }
    }
}

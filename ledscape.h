/** \file
 * LEDscape for the BeagleBone Black.
 *
 * Drives up to 32 ws281x LED strips using the PRU to have no CPU overhead.
 * Allows easy double buffering of frames.
 */

#ifndef _ledscape_h_
#define _ledscape_h_

#include <stdint.h>
#include "pru.h"

/** The number of strips supported.
 *
 * Changing this also requires changes in ws281x.p to stride the
 * correct number of bytes per row..
 */
#define LEDSCAPE_NUM_STRIPS 48


/**
 * An LEDscape "pixel" consists of three channels of output and an unused fourth channel. The color mapping of these
 * channels is not defined by the pixel construct, but is specified by color_channel_order_t. Use ledscape_pixel_set_color
 * to assign color values to a pixel.
 */
typedef struct {
	uint8_t a;// was blue
	uint8_t b;// was red
	uint8_t c;// was green
	uint8_t d;// is  white
} __attribute__((__packed__)) ledscape_pixel_t;


/** LEDscape frame buffer is "strip-major".
 *
 * All 32 strips worth of data for each pixel are stored adjacent.
 * This makes it easier to clock out while reading from the DDR
 * in a burst mode.
 */
typedef struct {
	ledscape_pixel_t strip[LEDSCAPE_NUM_STRIPS];
} __attribute__((__packed__)) ledscape_frame_t;

typedef struct ws281x_command ws281x_command_t;

typedef struct {
	ws281x_command_t * ws281x_0;
	ws281x_command_t * ws281x_1;
	pru_t * pru0;
	pru_t * pru1;
	const char* pru0_program_filename;
	const char* pru1_program_filename;
	unsigned num_pixels;
	size_t frame_size;
} ledscape_t;


typedef enum {
	COLOR_ORDER_RGB,
	COLOR_ORDER_RBG,
	COLOR_ORDER_GRB,
	COLOR_ORDER_GBR,
	COLOR_ORDER_BGR,
	COLOR_ORDER_BRG,
	COLOR_ORDER_RGBW,
	COLOR_ORDER_RBGW,
	COLOR_ORDER_GRBW,
	COLOR_ORDER_GBRW,
	COLOR_ORDER_BGRW,
	COLOR_ORDER_BRGW,
	COLOR_ORDER_RGWB,
	COLOR_ORDER_RBWG,
	COLOR_ORDER_GRWB,
	COLOR_ORDER_GBWR,
	COLOR_ORDER_BGWR,
	COLOR_ORDER_BRWG,
	COLOR_ORDER_RWGB,
	COLOR_ORDER_RWBG,
	COLOR_ORDER_GWRB,
	COLOR_ORDER_GWBR,
	COLOR_ORDER_BWGR,
	COLOR_ORDER_BWRG,
	COLOR_ORDER_WRGB,
	COLOR_ORDER_WRBG,
	COLOR_ORDER_WGRB,
	COLOR_ORDER_WGBR,
	COLOR_ORDER_WBGR,
	COLOR_ORDER_WBRG
} color_channel_order_t;


extern ledscape_t * ledscape_init(
unsigned num_pixels
);

extern ledscape_t * ledscape_init_with_programs(
	unsigned num_pixels,
	const char* pru0_program_filename,
	const char* pru1_program_filename
);


extern ledscape_frame_t *
ledscape_frame(
	ledscape_t * const leds,
	unsigned frame
);

extern void
ledscape_draw(
	ledscape_t * const leds,
	unsigned frame
);

inline void ledscape_pixel_set_color(
	ledscape_pixel_t * const out_pixel,
	color_channel_order_t color_channel_order,
	uint8_t r,
	uint8_t g,
	uint8_t b,
	uint8_t w
) {
	switch (color_channel_order) {
		case COLOR_ORDER_RGB:
			out_pixel->a = r;
			out_pixel->b = g;
			out_pixel->c = b;
		break;

		case COLOR_ORDER_RBG:
			out_pixel->a = r;
			out_pixel->b = b;
			out_pixel->c = g;
		break;

		case COLOR_ORDER_GRB:
			out_pixel->a = g;
			out_pixel->b = r;
			out_pixel->c = b;
		break;

		case COLOR_ORDER_GBR:
			out_pixel->a = g;
			out_pixel->b = b;
			out_pixel->c = r;
		break;

		case COLOR_ORDER_BGR:
			out_pixel->a = b;
			out_pixel->b = g;
			out_pixel->c = r;
		break;

		case COLOR_ORDER_BRG:
			out_pixel->a = b;
			out_pixel->b = r;
			out_pixel->c = g;
		break;


		case COLOR_ORDER_RGBW:
			out_pixel->a = r;
			out_pixel->b = g;
			out_pixel->c = w;
                        out_pixel->d = b;
		break;

		case COLOR_ORDER_RBGW:
			out_pixel->a = r;
			out_pixel->b = b;
			out_pixel->c = w;
                        out_pixel->d = g;
		break;

		case COLOR_ORDER_GRBW:
			out_pixel->a = g;
			out_pixel->b = r;
			out_pixel->c = w;
                        out_pixel->d = b;
		break;

		case COLOR_ORDER_GBRW:
			out_pixel->a = g;
			out_pixel->b = b;
			out_pixel->c = w;
                        out_pixel->d = r;
		break;

		case COLOR_ORDER_BGRW:
			out_pixel->a = b;
			out_pixel->b = g;
			out_pixel->c = w;
                        out_pixel->d = r;
		break;

		case COLOR_ORDER_BRGW:
			out_pixel->a = b;
			out_pixel->b = r;
			out_pixel->c = w;
                        out_pixel->d = g;
		break;


		case COLOR_ORDER_RGWB:
			out_pixel->a = r;
			out_pixel->b = g;
			out_pixel->c = w;
                        out_pixel->d = b;
		break;

		case COLOR_ORDER_RBWG:
			out_pixel->a = r;
			out_pixel->b = b;
			out_pixel->c = w;
                        out_pixel->d = g;
		break;

		case COLOR_ORDER_GRWB:
			out_pixel->a = g;
			out_pixel->b = r;
			out_pixel->c = w;
                        out_pixel->d = b;
		break;

		case COLOR_ORDER_GBWR:
			out_pixel->a = g;
			out_pixel->b = b;
			out_pixel->c = w;
                        out_pixel->d = r;
		break;

		case COLOR_ORDER_BGWR:
			out_pixel->a = b;
			out_pixel->b = g;
			out_pixel->c = w;
                        out_pixel->d = g;
		break;

		case COLOR_ORDER_BRWG:
			out_pixel->a = b;
			out_pixel->b = r;
			out_pixel->c = w;
                        out_pixel->d = g;
		break;


		case COLOR_ORDER_RWGB:
			out_pixel->a = r;
			out_pixel->b = w;
			out_pixel->c = g;
                        out_pixel->d = b;
		break;

		case COLOR_ORDER_RWBG:
			out_pixel->a = r;
			out_pixel->b = w;
			out_pixel->c = b;
                        out_pixel->d = g;
		break;

		case COLOR_ORDER_GWRB:
			out_pixel->a = g;
			out_pixel->b = w;
			out_pixel->c = r;
                        out_pixel->d = b;
		break;

		case COLOR_ORDER_GWBR:
			out_pixel->a = g;
			out_pixel->b = w;
			out_pixel->c = b;
                        out_pixel->d = r;
		break;

		case COLOR_ORDER_BWGR:
			out_pixel->a = b;
			out_pixel->b = w;
			out_pixel->c = g;
                        out_pixel->d = r;
		break;

		case COLOR_ORDER_BWRG:
			out_pixel->a = b;
			out_pixel->b = w;
			out_pixel->c = r;
                        out_pixel->d = g;
		break;

		case COLOR_ORDER_WRGB:
			out_pixel->a = w;
			out_pixel->b = r;
			out_pixel->c = g;
                        out_pixel->d = b;
		break;

		case COLOR_ORDER_WRBG:
			out_pixel->a = w;
			out_pixel->b = r;
			out_pixel->c = b;
                        out_pixel->d = g;
		break;

		case COLOR_ORDER_WGRB:
			out_pixel->a = w;
			out_pixel->b = g;
			out_pixel->c = r;
                        out_pixel->d = b;
		break;

		case COLOR_ORDER_WGBR:
			out_pixel->a = w;
			out_pixel->b = g;
			out_pixel->c = b;
                        out_pixel->d = r;
		break;

		case COLOR_ORDER_WBGR:
			out_pixel->a = w;
			out_pixel->b = b;
			out_pixel->c = g;
                        out_pixel->d = r;

		break;

		case COLOR_ORDER_WBRG:
			out_pixel->a = w;
			out_pixel->b = b;
			out_pixel->c = r;
                        out_pixel->d = g;
		break;

	}
}

inline void ledscape_set_color(
	ledscape_frame_t * const frame,
	color_channel_order_t color_channel_order,
	uint8_t strip,
	uint16_t pixel,
	uint8_t r,
	uint8_t g,
	uint8_t b,
	uint8_t w
) {
	ledscape_pixel_set_color(
		&frame[pixel].strip[strip],
		color_channel_order,
		r,
		g,
		b,
		w
	);
}

extern void
ledscape_wait(
	ledscape_t * const leds
);


extern void
ledscape_close(
	ledscape_t * const leds
);


extern const char* color_channel_order_to_string(
	color_channel_order_t color_channel_order
);

extern color_channel_order_t color_channel_order_from_string(
	const char* str
);

#endif

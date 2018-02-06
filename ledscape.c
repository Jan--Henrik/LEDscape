/** \file
 * Userspace interface to the WS281x LED strip driver.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#include "ledscape.h"


/** GPIO pins used by the LEDscape.
 *
 * The device tree should handle this configuration for us, but it
 * seems horribly broken and won't configure these pins as outputs.
 * So instead we have to repeat them here as well.
 *
 * If these are changed, be sure to check the mappings in
 * ws281x.p!
 *
 * See https://github.com/ehayon/BeagleBone-GPIO/blob/master/src/am335x.h
 * for a complete list of pins.
 *
 * TODO: Find a way to unify this with the defines in the .p file
 */
static const uint8_t gpios0[] = {
	2, 3, 7, 8, 9, 10, 11, 14, 20, 22, 23, 26, 27, 30, 31
};

static const uint8_t gpios1[] = {
	12, 13, 14, 15, 16, 17, 18, 19, 28, 29
};

static const uint8_t gpios2[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 22, 23, 24, 25,
};

static const uint8_t gpios3[] = {
	14, 15, 16, 17, 19, 21
};

#define ARRAY_COUNT(a) ((sizeof(a) / sizeof(*a)))


/** Command structure shared with the PRU.
 *
 * This is mapped into the PRU data RAM and points to the
 * frame buffer in the shared DDR segment.
 *
 * Changing this requires changes in ws281x.p
 */
typedef struct ws281x_command
{
	// in the DDR shared with the PRU
	uintptr_t pixels_dma;

	// Length in pixels of the longest LED strip.
	unsigned num_pixels;

	// write 1 to start, 0xFF to abort. will be cleared when started
	volatile unsigned command;

	// will have a non-zero response written when done
	volatile unsigned response;
} __attribute__((__packed__)) ws281x_command_t;


/** Retrieve one of the two frame buffers. */
ledscape_frame_t *
ledscape_frame(
	ledscape_t * const leds,
	unsigned int frame
)
{
	if (frame >= 2)
		return NULL;

	return (ledscape_frame_t*)((uint8_t*) leds->pru0->ddr + leds->frame_size * frame);
}


/** Initiate the transfer of a frame to the LED strips */
void
ledscape_draw(
	ledscape_t * const leds,
	unsigned int frame
)
{

	leds->ws281x_0->pixels_dma = leds->pru0->ddr_addr + leds->frame_size * frame;
	leds->ws281x_1->pixels_dma = leds->pru0->ddr_addr + leds->frame_size * frame;

	// Wait for any current command to have been acknowledged
	while (leds->ws281x_0->command || leds->ws281x_1->command);

	// Zero the responses so we can wait for them
	leds->ws281x_0->response = leds->ws281x_1->response = 0;

	// Send the start command
	leds->ws281x_0->command = 1;
	leds->ws281x_1->command = 1;
}


/** Wait for the current frame to finish transfering to the strips.
 * \returns a token indicating the response code.
 */
void
ledscape_wait(
	ledscape_t * const leds
)
{
	while (1)
	{
		pru_wait_interrupt();

		// printf("pru0: (%d,%d), pru1: (%d,%d)\n",
		// 	leds->ws281x_0->command, leds->ws281x_0->response,
		// 	leds->ws281x_1->command, leds->ws281x_1->response
		// );

		if (leds->ws281x_0->response && leds->ws281x_1->response) return;
	}
}


ledscape_t * ledscape_init( unsigned num_pixels ) {
	return ledscape_init_with_programs(
		num_pixels,
		"pru/bin/ws281x-original-ledscape-pru0.bin",
		"pru/bin/ws281x-original-ledscape-pru1.bin"
	);
}

ledscape_t * ledscape_init_with_programs(
	unsigned num_pixels,
	const char* pru0_program_filename,
	const char* pru1_program_filename
)
{
	pru_t * const pru0 = pru_init(0);
	pru_t * const pru1 = pru_init(1);

	const size_t frame_size = num_pixels * LEDSCAPE_NUM_STRIPS * 4;

	if (2*frame_size > pru0->ddr_size)
		die("Pixel data needs at least 2 * %zu, only %zu in DDR\n",
			frame_size,
			pru0->ddr_size
		);

	ledscape_t * const leds = calloc(1, sizeof(*leds));

	*leds = (ledscape_t) {
		.pru0		= pru0,
		.pru1		= pru1,
		.num_pixels	= num_pixels,
		.frame_size	= frame_size,
		.pru0_program_filename  = pru0_program_filename,
		.pru1_program_filename  = pru1_program_filename,
		.ws281x_0	= pru0->data_ram,
		.ws281x_1	= pru1->data_ram
	};

	*(leds->ws281x_0) = *(leds->ws281x_1) = (ws281x_command_t) {
		.pixels_dma	= 0, // will be set in draw routine
		.command	= 0,
		.response	= 0,
		.num_pixels	= leds->num_pixels,
	};

	// Configure all of our output pins.
	for (unsigned i = 0 ; i < ARRAY_COUNT(gpios0) ; i++)
		pru_gpio(0, gpios0[i], 1, 0);
	for (unsigned i = 0 ; i < ARRAY_COUNT(gpios1) ; i++)
		pru_gpio(1, gpios1[i], 1, 0);
	for (unsigned i = 0 ; i < ARRAY_COUNT(gpios2) ; i++)
		pru_gpio(2, gpios2[i], 1, 0);
	for (unsigned i = 0 ; i < ARRAY_COUNT(gpios3) ; i++)
		pru_gpio(3, gpios3[i], 1, 0);

	// Initiate the PRU0 program
	pru_exec(pru0, pru0_program_filename);

	// Watch for a done response that indicates a proper startup
	// \todo timeout if it fails
	fprintf(stdout, "String PRU0 with %s... ", pru0_program_filename);
	while (!leds->ws281x_0->response);
	printf("OK\n");


	// Initiate the PRU1 program
	pru_exec(pru1, pru1_program_filename);

	// Watch for a done response that indicates a proper startup
	// \todo timeout if it fails
	fprintf(stdout, "String PRU1 with %s... ", pru1_program_filename);
	while (!leds->ws281x_1->response);
	printf("OK\n");

	return leds;
}


const char* color_channel_order_to_string(color_channel_order_t color_channel_order) {
	switch (color_channel_order) {
		case COLOR_ORDER_RGB: return "RGB";
		case COLOR_ORDER_RBG: return "RBG";
		case COLOR_ORDER_GRB: return "GRB";
		case COLOR_ORDER_GBR: return "GBR";
		case COLOR_ORDER_BGR: return "BGR";
		case COLOR_ORDER_BRG: return "BRG";

		case COLOR_ORDER_RGBW: return "RGB";
		case COLOR_ORDER_RBGW: return "RBG";
		case COLOR_ORDER_GRBW: return "GRB";
		case COLOR_ORDER_GBRW: return "GBR";
		case COLOR_ORDER_BGRW: return "BGR";
		case COLOR_ORDER_BRGW: return "BRG";

		case COLOR_ORDER_RGWB: return "RGWB";
		case COLOR_ORDER_RBWG: return "RBWG";
		case COLOR_ORDER_GRWB: return "GRWB";
		case COLOR_ORDER_GBWR: return "GBWR";
		case COLOR_ORDER_BGWR: return "BGWR";
		case COLOR_ORDER_BRWG: return "BRWG";

		case COLOR_ORDER_RWGB: return "RWGB";
		case COLOR_ORDER_RWBG: return "RWBG";
		case COLOR_ORDER_GWRB: return "GWRB";
		case COLOR_ORDER_GWBR: return "GWBR";
		case COLOR_ORDER_BWGR: return "BWGR";
		case COLOR_ORDER_BWRG: return "BWRG";

		case COLOR_ORDER_WRGB: return "WRGB";
		case COLOR_ORDER_WRBG: return "WRBG";
		case COLOR_ORDER_WGRB: return "WGRB";
		case COLOR_ORDER_WGBR: return "WGBR";
		case COLOR_ORDER_WBGR: return "WBGR";
		case COLOR_ORDER_WBRG: return "WBRG";

		default: return  "<invalid color_channel_order>";
	}
}

color_channel_order_t color_channel_order_from_string(const char* str) {
	if (strcasecmp(str, "RGB") == 0) {
		return COLOR_ORDER_RGB;
	}
	else if (strcasecmp(str, "RBG") == 0) {
		return COLOR_ORDER_RBG;
	}
	else if (strcasecmp(str, "GRB") == 0) {
		return COLOR_ORDER_GRB;
	}
	else if (strcasecmp(str, "GBR") == 0) {
		return COLOR_ORDER_GBR;
	}
	else if (strcasecmp(str, "BGR") == 0) {
		return COLOR_ORDER_BGR;
	}
	else if (strcasecmp(str, "BRG") == 0) {
		return COLOR_ORDER_BRG;
	}

	else if (strcasecmp(str, "RGBW") == 0) {
		return COLOR_ORDER_RGBW;
	}
	else if (strcasecmp(str, "RBGW") == 0) {
		return COLOR_ORDER_RBGW;
	}
	else if (strcasecmp(str, "GRBW") == 0) {
		return COLOR_ORDER_GRBW;
	}
	else if (strcasecmp(str, "GBRW") == 0) {
		return COLOR_ORDER_GBRW;
	}
	else if (strcasecmp(str, "BGRW") == 0) {
		return COLOR_ORDER_BGRW;
	}
	else if (strcasecmp(str, "BRGW") == 0) {
		return COLOR_ORDER_BRGW;
	}

	else if (strcasecmp(str, "RGWB") == 0) {
		return COLOR_ORDER_RGWB;
	}
	else if (strcasecmp(str, "RBWG") == 0) {
		return COLOR_ORDER_RBWG;
	}
	else if (strcasecmp(str, "GRWB") == 0) {
		return COLOR_ORDER_GRWB;
	}
	else if (strcasecmp(str, "GBWR") == 0) {
		return COLOR_ORDER_GBWR;
	}
	else if (strcasecmp(str, "BGWR") == 0) {
		return COLOR_ORDER_BGWR;
	}
	else if (strcasecmp(str, "BRWG") == 0) {
		return COLOR_ORDER_BRWG;
	}

	else if (strcasecmp(str, "RWGB") == 0) {
		return COLOR_ORDER_RWGB;
	}
	else if (strcasecmp(str, "RWBG") == 0) {
		return COLOR_ORDER_RWBG;
	}
	else if (strcasecmp(str, "GWRB") == 0) {
		return COLOR_ORDER_GWRB;
	}
	else if (strcasecmp(str, "GWBR") == 0) {
		return COLOR_ORDER_GWBR;
	}
	else if (strcasecmp(str, "BWGR") == 0) {
		return COLOR_ORDER_BWGR;
	}
	else if (strcasecmp(str, "BWRG") == 0) {
		return COLOR_ORDER_BWRG;
	}

	else if (strcasecmp(str, "WRGB") == 0) {
		return COLOR_ORDER_WRGB;
	}
	else if (strcasecmp(str, "WRBG") == 0) {
		return COLOR_ORDER_WRBG;
	}
	else if (strcasecmp(str, "WGRB") == 0) {
		return COLOR_ORDER_WGRB;
	}
	else if (strcasecmp(str, "WGBR") == 0) {
		return COLOR_ORDER_WGBR;
	}
	else if (strcasecmp(str, "WBGR") == 0) {
		return COLOR_ORDER_WBGR;
	}
	else if (strcasecmp(str, "WBRG") == 0) {
		return COLOR_ORDER_WBRG;
	}
	else {
		return -1;
	}
}

void
ledscape_close(
	ledscape_t * const leds
)
{
	// Signal a halt command
	leds->ws281x_0->command = 0xFF;
	leds->ws281x_1->command = 0xFF;
	pru_close(leds->pru0);
	pru_close(leds->pru1);
}

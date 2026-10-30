/* Y900 M138 boot instrumentation.
 *
 * M135/M136/M137 established:
 *  - fbcon works: the console shows kernel printk on 0xfc800000 (a8r8g8b8,
 *    3840x2560, stride 15360). The display only dies if dispcc probes.
 *  - With disp_cc_sm8750_driver_init blacklisted, all 853 device_initcalls
 *    complete and the machine reaches late_initcall, then resets ~10 s later
 *    with no oops output (so it is not a kernel panic).
 *
 * So the cheapest way to name the culprit is to print it: before every level-7
 * (late) initcall, printk its sequence number and symbol name. The last line on
 * screen is then the initcall that never returned. A binary square counter is
 * kept as a fallback in case fbcon stops updating again.
 *
 * The framebuffer mapping is cached in a static pointer. M137 remapped 39 MiB
 * on every single initcall, which inflated kernel time roughly 100x (simplefb
 * at 14.97 s instead of 0.089 s). Mapping once removes that distortion.
 */
#ifndef _Y900_BEACON_H
#define _Y900_BEACON_H

#include <linux/io.h>
#include <linux/types.h>

#define Y900_FB_PHYS   0xfc800000UL
#define Y900_FB_STRIDE 15360U            /* 3840 * 4 */
#define Y900_FB_WIDTH  3840U
#define Y900_FB_HEIGHT 2560U
#define Y900_FB_BYTES  (Y900_FB_STRIDE * Y900_FB_HEIGHT)
#define Y900_BAND_H    48U               /* level stripe height */

#define Y900_BITS      10U               /* 10 bits -> 0..1023 */
#define Y900_SQ        160U              /* square side */
#define Y900_SQ_GAP    32U
#define Y900_SQ_ROW_A  400U              /* device-level counter row */
#define Y900_SQ_ROW_B  620U              /* late-level counter row  */

#define Y900_ON        0x00ffffff        /* white  = bit set */
#define Y900_OFF       0x00202020        /* dark   = bit clear */

static void __iomem *y900_fb_map __initdata;

static const u32 y900_beacon_colors[] = {
	0x00ff0000, /* 0 red     */
	0x00ff8000, /* 1 orange  */
	0x00ffff00, /* 2 yellow  */
	0x0000ff00, /* 3 green   */
	0x0000ffff, /* 4 cyan    */
	0x000080ff, /* 5 azure   */
	0x000000ff, /* 6 blue    */
	0x008000ff, /* 7 violet  */
	0x00ff00ff, /* 8 magenta */
	0x00ffffff, /* 9 white   */
};

static void __iomem *__init y900_fb(void)
{
	if (!y900_fb_map)
		y900_fb_map = ioremap_wc(Y900_FB_PHYS, Y900_FB_BYTES);
	return y900_fb_map;
}

static void __init y900_fill(void __iomem *fb, unsigned int x, unsigned int y,
			     unsigned int w, unsigned int h, u32 color)
{
	unsigned int row, col;

	if (!fb)
		return;
	if (y + h > Y900_FB_HEIGHT || x + w > Y900_FB_WIDTH)
		return;
	for (row = y; row < y + h; row++) {
		void __iomem *line = fb + (unsigned long)row * Y900_FB_STRIDE;

		for (col = x; col < x + w; col++)
			writel(color, line + (unsigned long)col * 4);
	}
}

/* Level stripe, as in M135. Painted before a level starts. */
static void __init y900_beacon(unsigned int index)
{
	void __iomem *fb = y900_fb();
	u32 color;

	if (index >= ARRAY_SIZE(y900_beacon_colors))
		index = ARRAY_SIZE(y900_beacon_colors) - 1;
	color = y900_beacon_colors[index];
	y900_fill(fb, 0, index * Y900_BAND_H, Y900_FB_WIDTH, Y900_BAND_H, color);
	wmb();
}

/* Binary index readout for a given row (row_base selects device or late). */
static void __init y900_beacon_row(unsigned int row_base, unsigned int value)
{
	void __iomem *fb = y900_fb();
	unsigned int bit;

	for (bit = 0; bit < Y900_BITS; bit++) {
		unsigned int slot = Y900_BITS - 1 - bit;   /* MSB leftmost */
		unsigned int x = Y900_SQ_GAP + slot * (Y900_SQ + Y900_SQ_GAP);
		u32 color = (value & (1U << bit)) ? Y900_ON : Y900_OFF;

		y900_fill(fb, x, row_base, Y900_SQ, Y900_SQ, color);
	}
	wmb();
}

static void __init y900_beacon_count(unsigned int value)
{
	y900_beacon_row(Y900_SQ_ROW_A, value);
}

static void __init y900_beacon_late(unsigned int value)
{
	y900_beacon_row(Y900_SQ_ROW_B, value);
}

#endif /* _Y900_BEACON_H */

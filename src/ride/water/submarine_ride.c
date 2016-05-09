#pragma region Copyright (c) 2014-2016 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include "../track_paint.h"
#include "../track.h"
#include "../vehicle_paint.h"
#include "../../interface/viewport.h"
#include "../../paint/paint.h"
#include "../../paint/supports.h"

enum
{
	SPR_MONORAIL_ELEM_FLAT_NE_SW = 23231,
	SPR_MONORAIL_ELEM_FLAT_SE_NW = 23232,
} SPR_MONORAIL;

enum
{
	SEGMENT_B4 = (1 << 0),
	SEGMENT_0 = SEGMENT_B4,
	SEGMENT_B8 = (1 << 1),
	SEGMENT_1 = SEGMENT_B8,
	SEGMENT_BC = (1 << 2),
	SEGMENT_2 = SEGMENT_BC,
	SEGMENT_C0 = (1 << 3),
	SEGMENT_3 = SEGMENT_C0,
	SEGMENT_C4 = (1 << 4),
	SEGMENT_4 = SEGMENT_C4,
	SEGMENT_C8 = (1 << 5),
	SEGMENT_5 = SEGMENT_C8,
	SEGMENT_CC = (1 << 6),
	SEGMENT_6 = SEGMENT_CC,
	SEGMENT_D0 = (1 << 7),
	SEGMENT_7 = SEGMENT_D0,
	SEGMENT_D4 = (1 << 8),
	SEGMENT_8 = SEGMENT_D4,
};

const int SEGMENTS_ALL = SEGMENT_B4 | SEGMENT_B8 | SEGMENT_BC | SEGMENT_C0 | SEGMENT_C4 | SEGMENT_C8 | SEGMENT_CC | SEGMENT_D0 | SEGMENT_D4;

const int segmentOffsets[9] = {
	1 << 0, // 0 -> 0
	1 << 6,  // 1 -> 6
	1 << 2,  // 2 -> 2
	1 << 4,  // 3 -> 4
	0,          // center is skipped
	1 << 7,
	1 << 1,
	1 << 5,
	1 << 3
};

static uint16 segments_rotate(uint16 segments, uint8 rotation)
{
	uint8 temp = 0;

	for (int s = 0; s < 9; s++) {
		if (segments & (1 << s)) {
			temp |= segmentOffsets[s];
		}
	}

	temp = ror8(temp, rotation * 2);

	uint16 out = 0;

	for (int s = 0; s < 9; s++) {
		if (temp & segmentOffsets[s]) {
			out |= (1 << s);
		}
	}

	if (segments & SEGMENT_4) {
		out |= SEGMENT_4;
	}

	return out;
}

static void paint_util_set_segment_support_height(int flags, uint16 height, uint8 segment_flags)
{
	for (int s = 0; s < 9; s++) {
		if (flags & (1 << s)) {
			RCT2_GLOBAL(0x0141E9B4 + s * 4, uint16) = height;
			RCT2_GLOBAL(0x0141E9B6 + s * 4, uint8) = segment_flags;
		}
	}
}

static void paint_util_set_support_height(uint16 height, uint8 flags)
{
	if (RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_PAINT_TILE_MAX_HEIGHT, sint16) < height) {
		RCT2_GLOBAL(RCT2_ADDRESS_CURRENT_PAINT_TILE_MAX_HEIGHT, sint16) = height;
		RCT2_GLOBAL(0x141E9DA, uint8) = flags;
	}
}

enum
{
	TUNNEL_0,
	TUNNEL_FLAT = 6,
	TUNNEL_7 = 7,
	TUNNEL_UP = 8,
	TUNNEL_14 = 0x0E
};

static void paint_util_push_tunnel_left(uint16 height, uint8 type)
{
	uint32 eax = 0xFFFF0000 | ((height / 16) & 0xFF) | type << 8;
	RCT2_ADDRESS(0x009E3138, uint32)[RCT2_GLOBAL(0x141F56A, uint8) / 2] = eax;
	RCT2_GLOBAL(0x141F56A, uint8)++;
}

static void paint_util_push_tunnel_right(uint16 height, uint8 type)
{
	uint32 eax = 0xFFFF0000 | ((height / 16) & 0xFF) | type << 8;
	RCT2_ADDRESS(0x009E30B6, uint32)[RCT2_GLOBAL(0x141F56B, uint8) / 2] = eax;
	RCT2_GLOBAL(0x141F56B, uint8)++;
}

/* Supports are only placed every 2 tiles for flat pieces*/
bool paint_util_should_paint_supports(rct_xy16 position)
{
	if ((position.x & (1 << 5)) == (position.y & (1 << 5)))
		return true;

	if ((!(position.x & (1 << 5))) && (!(position.y & (1 << 5))))
		return true;

	return false;
}

/**
 *
 *  rct2: 0x006D44D5
 */
void vehicle_visual_submarine(int x, int imageDirection, int y, int z, rct_vehicle *vehicle, const rct_ride_entry_vehicle *vehicleEntry)
{
	int baseImage_id = imageDirection;
	int image_id;
	if (vehicle->restraints_position >= 64) {
		if ((vehicleEntry->sprite_flags & 0x2000) && !(imageDirection & 3)) {
			baseImage_id /= 8;
			baseImage_id += ((vehicle->restraints_position - 64) / 64) * 4;
			baseImage_id *= vehicleEntry->var_16;
			baseImage_id += vehicleEntry->var_1C;
		}
	} else {
		if (vehicleEntry->flags_a & 0x800) {
			baseImage_id /= 2;
		}
		if (vehicleEntry->sprite_flags & 0x8000) {
			baseImage_id /= 8;
		}
		baseImage_id *= vehicleEntry->var_16;
		baseImage_id += vehicleEntry->base_image_id;
		baseImage_id += vehicle->var_4A;
	}

	vehicle_boundbox bb = VehicleBoundboxes[vehicleEntry->draw_order][imageDirection / 2];

	image_id = baseImage_id | (vehicle->colours.body_colour << 19) | (vehicle->colours.trim_colour << 24) | 0x80000000;
	paint_struct* ps = sub_98197C(image_id, 0, 0, bb.length_x, bb.length_y, bb.length_z, z, bb.offset_x, bb.offset_y, bb.offset_z + z, get_current_rotation());
	if (ps != NULL) {
		ps->tertiary_colour = vehicle->colours_extended;
	}

	image_id = (baseImage_id + 1) | (vehicle->colours.body_colour << 19) | (vehicle->colours.trim_colour << 24) | 0x80000000;
	ps = sub_98197C(image_id, 0, 0, bb.length_x, bb.length_y, 2, z, bb.offset_x, bb.offset_y, bb.offset_z + z - 10, get_current_rotation());
	if (ps != NULL) {
		ps->tertiary_colour = vehicle->colours_extended;
	}

	assert(vehicleEntry->effect_visual == 1);
}

enum
{
	SPR_SUBMARINE_RIDE_FLAT_NE_SW = 16870,
	SPR_SUBMARINE_RIDE_FLAT_SE_NW = 16871,

	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_SW_SE_PART_0 = 16872,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_SW_SE_PART_1 = 16873,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_SW_SE_PART_2 = 16874,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_NW_SW_PART_0 = 16875,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_NW_SW_PART_1 = 16876,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_NW_SW_PART_2 = 16877,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_NE_NW_PART_0 = 16878,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_NE_NW_PART_1 = 16879,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_NE_NW_PART_2 = 16880,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_SE_NE_PART_0 = 16881,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_SE_NE_PART_1 = 16882,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_SE_NE_PART_2 = 16883,

	SPR_SUBMARINE_RIDE_FLAT_TO_25_DEG_UP_SW_NE,
	SPR_SUBMARINE_RIDE_FLAT_TO_25_DEG_UP_NW_SE,
	SPR_SUBMARINE_RIDE_FLAT_TO_25_DEG_UP_NE_SW,
	SPR_SUBMARINE_RIDE_FLAT_TO_25_DEG_UP_SE_NW,

	SPR_SUBMARINE_RIDE_25_DEG_UP_TO_FLAT_SW_NE,
	SPR_SUBMARINE_RIDE_25_DEG_UP_TO_FLAT_NW_SE,
	SPR_SUBMARINE_RIDE_25_DEG_UP_TO_FLAT_NE_SW,
	SPR_SUBMARINE_RIDE_25_DEG_UP_TO_FLAT_SE_NW,

	SPR_SUBMARINE_RIDE_25_DEG_UP_SW_NE,
	SPR_SUBMARINE_RIDE_25_DEG_UP_NW_SE,
	SPR_SUBMARINE_RIDE_25_DEG_UP_NE_SW,
	SPR_SUBMARINE_RIDE_25_DEG_UP_SE_NW,

	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_1_TILE_SW_NW = 16896,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_1_TILE_NW_NE = 16897,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_1_TILE_NE_SE = 16898,
	SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_1_TILE_SE_SW = 16899,
};

static void submarine_ride_paint_track_station(uint8 rideIndex, uint8 trackSequence, uint8 direction, int height, rct_map_element * mapElement)
{
	rct_xy16 position = {RCT2_GLOBAL(0x009DE56A, sint16), RCT2_GLOBAL(0x009DE56E, sint16)};
	int heightLower = height - 16;
	uint32 imageId;
}

static void submarine_ride_paint_track_flat(uint8 rideIndex, uint8 trackSequence, uint8 direction, int height, rct_map_element * mapElement)
{
	rct_xy16 position = {RCT2_GLOBAL(0x009DE56A, sint16), RCT2_GLOBAL(0x009DE56E, sint16)};
	int heightLower = height - 16;
	uint32 imageId;

	if (direction & 1) {
		imageId = SPR_SUBMARINE_RIDE_FLAT_SE_NW | RCT2_GLOBAL(0x00F44198, uint32);
		sub_98197C(imageId, 0, 0, 20, 32, 3, heightLower, 6, 0, heightLower, get_current_rotation());
		paint_util_push_tunnel_right(heightLower, TUNNEL_0);
	} else {
		imageId = SPR_SUBMARINE_RIDE_FLAT_NE_SW | RCT2_GLOBAL(0x00F44198, uint32);
		sub_98197C(imageId, 0, 0, 32, 20, 3, heightLower, 0, 6, heightLower, get_current_rotation());
		paint_util_push_tunnel_left(heightLower, TUNNEL_0);
	}

	if (paint_util_should_paint_supports(position)) {
		metal_a_supports_paint_setup((direction & 1) ? 4 : 5, 4, -1, heightLower, RCT2_GLOBAL(0x00F4419C, uint32));
	}

	paint_util_set_segment_support_height(segments_rotate(SEGMENT_D0 | SEGMENT_C4 | SEGMENT_CC, direction), 0xFFFF, 0);
	paint_util_set_support_height(height + 16, 0x20);
}

static void submarine_ride_paint_track_quarter_turn_3_tiles_nw_sw(uint8 rideIndex, uint8 trackSequence, uint8 direction, int height, rct_map_element * mapElement)
{
	int heightLower = height - 16;
	uint32 imageId;

	switch (trackSequence) {
		case 0:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_NW_SW_PART_2 | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 32, 20, 3, heightLower, 0, 6, heightLower, get_current_rotation());

			paint_util_push_tunnel_right(heightLower, TUNNEL_0);
			metal_a_supports_paint_setup(4, 4, -1, heightLower, RCT2_GLOBAL(0x00F4419C, uint32));
			paint_util_set_segment_support_height(SEGMENT_D0 | SEGMENT_C4 | SEGMENT_CC | SEGMENT_B4, 0xFFFF, 0);
			break;
		case 1:
			break;
		case 2:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_NW_SW_PART_1 | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 16, 16, 3, heightLower, 16, 0, heightLower, get_current_rotation());

			paint_util_set_segment_support_height(SEGMENT_C8 | SEGMENT_C4 | SEGMENT_D0 | SEGMENT_B8, 0xFFFF, 0);
			break;
		case 3:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_NW_SW_PART_0 | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 20, 32, 3, heightLower, 6, 0, heightLower, get_current_rotation());

			metal_a_supports_paint_setup(4, 4, -1, heightLower, RCT2_GLOBAL(0x00F4419C, uint32));
			paint_util_set_segment_support_height(SEGMENT_C8 | SEGMENT_C4 | SEGMENT_D4 | SEGMENT_C0, 0xFFFF, 0);
			break;
	}

	paint_util_set_support_height(height + 16, 0x20);
}

static void submarine_ride_paint_track_quarter_turn_3_tiles_ne_nw(uint8 rideIndex, uint8 trackSequence, uint8 direction, int height, rct_map_element * mapElement)
{
	int heightLower = height - 16;
	uint32 imageId;

	switch (trackSequence) {
		case 0:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_NE_NW_PART_2 | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 20, 32, 3, heightLower, 6, 0, heightLower, get_current_rotation());

			metal_a_supports_paint_setup(4, 4, -1, heightLower, RCT2_GLOBAL(0x00F4419C, uint32));
			paint_util_set_segment_support_height(SEGMENT_C8 | SEGMENT_C4 | SEGMENT_D4 | SEGMENT_BC, 0xFFFF, 0);
			break;
		case 1:
			break;
		case 2:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_NE_NW_PART_1 | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 16, 16, 3, heightLower, 0, 0, heightLower, get_current_rotation());

			paint_util_set_segment_support_height(SEGMENT_B4 | SEGMENT_C4 | SEGMENT_CC | SEGMENT_C8, 0xFFFF, 0);
			break;
		case 3:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_NE_NW_PART_0 | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 32, 20, 3, heightLower, 0, 6, heightLower, get_current_rotation());

			metal_a_supports_paint_setup(4, 4, -1, heightLower, RCT2_GLOBAL(0x00F4419C, uint32));
			paint_util_set_segment_support_height(SEGMENT_D0 | SEGMENT_C4 | SEGMENT_CC | SEGMENT_B8, 0xFFFF, 0);
			break;
	}

	paint_util_set_support_height(height + 16, 0x20);
}

static void submarine_ride_paint_track_quarter_turn_3_tiles_se_ne(uint8 rideIndex, uint8 trackSequence, uint8 direction, int height, rct_map_element * mapElement)
{
	int heightLower = height - 16;
	uint32 imageId;

	switch (trackSequence) {
		case 0:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_SE_NE_PART_2 | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 32, 20, 3, heightLower, 0, 6, heightLower, get_current_rotation());

			metal_a_supports_paint_setup(4, 4, -1, heightLower, RCT2_GLOBAL(0x00F4419C, uint32));
			paint_util_set_segment_support_height(SEGMENT_CC | SEGMENT_C4 | SEGMENT_D0 | SEGMENT_C0, 0xFFFF, 0);
			break;
		case 1:
			break;
		case 2:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_SE_NE_PART_1 | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 16, 16, 3, heightLower, 0, 16, heightLower, get_current_rotation());

			paint_util_set_segment_support_height(SEGMENT_CC | SEGMENT_C4 | SEGMENT_D4 | SEGMENT_BC, 0xFFFF, 0);
			break;
		case 3:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_SE_NE_PART_0 | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 20, 32, 3, heightLower, 6, 0, heightLower, get_current_rotation());

			metal_a_supports_paint_setup(4, 4, -1, heightLower, RCT2_GLOBAL(0x00F4419C, uint32));
			paint_util_set_segment_support_height(SEGMENT_D0 | SEGMENT_C4 | SEGMENT_CC | SEGMENT_BC, 0xFFFF, 0);
			paint_util_push_tunnel_left(heightLower, TUNNEL_0);
			break;
	}

	paint_util_set_support_height(height + 16, 0x20);
}

static void submarine_ride_paint_track_quarter_turn_3_tiles_sw_se(uint8 rideIndex, uint8 trackSequence, uint8 direction, int height, rct_map_element * mapElement)
{
	int heightLower = height - 16;
	uint32 imageId;

	switch (trackSequence) {
		case 0:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_SW_SE_PART_2 | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 20, 32, 3, heightLower, 6, 0, heightLower, get_current_rotation());

			metal_a_supports_paint_setup(4, 4, -1, heightLower, RCT2_GLOBAL(0x00F4419C, uint32));
			paint_util_set_segment_support_height(SEGMENT_D4 | SEGMENT_C4 | SEGMENT_C8 | SEGMENT_B8, 0xFFFF, 0);
			paint_util_push_tunnel_right(heightLower, TUNNEL_0);
			break;
		case 1:
			break;
		case 2:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_SW_SE_PART_1 | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 16, 16, 3, heightLower, 16, 16, heightLower, get_current_rotation());

			paint_util_set_segment_support_height(SEGMENT_D0 | SEGMENT_C4 | SEGMENT_D4 | SEGMENT_C0, 0xFFFF, 0);
			break;
		case 3:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_3_TILES_SW_SE_PART_0 | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 32, 20, 3, heightLower, 0, 6, heightLower, get_current_rotation());

			metal_a_supports_paint_setup(4, 4, -1, heightLower, RCT2_GLOBAL(0x00F4419C, uint32));
			paint_util_set_segment_support_height(SEGMENT_D0 | SEGMENT_C4 | SEGMENT_D4 | SEGMENT_B4, 0xFFFF, 0);
			break;
	}

	paint_util_set_support_height(height + 16, 0x20);
}

static void submarine_ride_paint_track_left_quarter_turn_3_tiles(uint8 rideIndex, uint8 trackSequence, uint8 direction, int height, rct_map_element * mapElement)
{
	switch (direction) {
		case 0:
			submarine_ride_paint_track_quarter_turn_3_tiles_nw_sw(rideIndex, trackSequence, direction, height, mapElement);
			break;
		case 1:
			submarine_ride_paint_track_quarter_turn_3_tiles_ne_nw(rideIndex, trackSequence, direction, height, mapElement);
			break;
		case 2:
			submarine_ride_paint_track_quarter_turn_3_tiles_se_ne(rideIndex, trackSequence, direction, height, mapElement);
			break;
		case 3:
			submarine_ride_paint_track_quarter_turn_3_tiles_sw_se(rideIndex, trackSequence, direction, height, mapElement);
			break;
	}
}

uint8 right_quarter_turn_3_tiles_to_left_turn_map[] = {3, 1, 2, 0};
static void submarine_ride_paint_track_right_quarter_turn_3_tiles(uint8 rideIndex, uint8 trackSequence, uint8 direction, int height, rct_map_element * mapElement)
{
	trackSequence = right_quarter_turn_3_tiles_to_left_turn_map[trackSequence];
	return submarine_ride_paint_track_left_quarter_turn_3_tiles(rideIndex, trackSequence, (direction + 3) % 4, height, mapElement);
}

static void submarine_ride_paint_track_left_quarter_turn_1_tile(uint8 rideIndex, uint8 trackSequence, uint8 direction, int height, rct_map_element * mapElement)
{
	int heightLower = height - 16;
	uint32 imageId;

	switch (direction) {
		case 0:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_1_TILE_SW_NW | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 26, 24, 1, heightLower, 6, 2, heightLower, get_current_rotation());
			paint_util_push_tunnel_left(heightLower, TUNNEL_0);
			break;
		case 1:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_1_TILE_NW_NE | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 26, 26, 1, heightLower, 0, 0, heightLower, get_current_rotation());
			break;
		case 2:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_1_TILE_NE_SE | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 24, 26, 1, heightLower, 2, 6, heightLower, get_current_rotation());
			paint_util_push_tunnel_right(heightLower, TUNNEL_0);
			break;
		case 3:
			imageId = SPR_SUBMARINE_RIDE_FLAT_QUARTER_TURN_1_TILE_SE_SW | RCT2_GLOBAL(0x00F44198, uint32);
			sub_98197C(imageId, 0, 0, 24, 24, 1, heightLower, 6, 6, heightLower, get_current_rotation());
			paint_util_push_tunnel_right(heightLower, TUNNEL_0);
			paint_util_push_tunnel_left(heightLower, TUNNEL_0);
			break;
	}

	paint_util_set_segment_support_height(segments_rotate(SEGMENT_B8 | SEGMENT_C8 | SEGMENT_C4 | SEGMENT_D0, direction), 0xFFFF, 0);
	paint_util_set_support_height(height + 16, 0x20);
}

static void submarine_ride_paint_track_right_quarter_turn_1_tile(uint8 rideIndex, uint8 trackSequence, uint8 direction, int height, rct_map_element * mapElement)
{
	submarine_ride_paint_track_left_quarter_turn_1_tile(rideIndex, trackSequence, (direction + 3) % 4, height, mapElement);
}

/**
 * rct2: 0x008995D4
 */
TRACK_PAINT_FUNCTION get_track_paint_function_submarine_ride(int trackType, int direction)
{
	switch (trackType) {
		case TRACK_ELEM_BEGIN_STATION:
		case TRACK_ELEM_MIDDLE_STATION:
		case TRACK_ELEM_END_STATION:
			return submarine_ride_paint_track_station;

		case TRACK_ELEM_FLAT:
			return submarine_ride_paint_track_flat;

		case TRACK_ELEM_LEFT_QUARTER_TURN_3_TILES:
			return submarine_ride_paint_track_left_quarter_turn_3_tiles;
		case TRACK_ELEM_RIGHT_QUARTER_TURN_3_TILES:
			return submarine_ride_paint_track_right_quarter_turn_3_tiles;

		case TRACK_ELEM_LEFT_QUARTER_TURN_1_TILE:
			return submarine_ride_paint_track_left_quarter_turn_1_tile;
		case TRACK_ELEM_RIGHT_QUARTER_TURN_1_TILE:
			return submarine_ride_paint_track_right_quarter_turn_1_tile;
	}

	return NULL;
}

// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "world.h"
#include "map.h"

//#define PREVIEW_SIZE (TIC80_WIDTH * TIC80_HEIGHT * TIC_PALETTE_BPP / BITS_IN_BYTE)
#define PREVIEW_SIZE (TIC_MAP_WIDTH*TIC_MAP_HEIGHT)
#define WORLD_X (0)
#define WORLD_Y (0)
#define WORLD_WIDTH (TIC80_WIDTH)
#define WORLD_HEIGHT (TIC80_HEIGHT)
#define MAX_SCROLL_X (512)
#define MAX_SCROLL_Y (448)

static void normalizeWorld(s32* x, s32* y)
{
	while (*x < 0) *x += MAX_SCROLL_X;
	while (*y < 0) *y += MAX_SCROLL_Y;
	while (*x >= MAX_SCROLL_X) *x -= MAX_SCROLL_X;
	while (*y >= MAX_SCROLL_Y) *y -= MAX_SCROLL_Y;
}

static void processScrolling(World* world, bool pressed)
{
	tic_rect rect = { WORLD_X, WORLD_Y, WORLD_WIDTH, WORLD_HEIGHT };

	if (world->scroll.active)
	{
		if (pressed)
		{
			world->scroll.x = world->scroll.start.x - getMouseX();
			world->scroll.y = world->scroll.start.y - getMouseY();

			normalizeWorld(&world->scroll.x, &world->scroll.y);

			setCursor(tic_cursor_hand);
		}
		else world->scroll.active = false;
	}
	else if (checkMousePos(&rect))
	{
		if (pressed)
		{
			world->scroll.active = true;

			world->scroll.start.x = getMouseX() + world->scroll.x;
			world->scroll.start.y = getMouseY() + world->scroll.y;
		}
	}
}

static void processMouseDragMode(World* world)
{
	tic_rect rect = { WORLD_X, WORLD_Y, WORLD_WIDTH, WORLD_HEIGHT };

	processScrolling(world, checkMouseDown(&rect, tic_mouse_left) ||
		checkMouseDown(&rect, tic_mouse_right));
}

static void drawMapCursor(World* world)
{
	Map* map = world->map;
	tic_rect rect = { 0, 0, TIC80_WIDTH, TIC80_HEIGHT };

	if (checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		s32 mx = getMouseX();
		s32 my = getMouseY();
		
		if (checkMouseDown(&rect, tic_mouse_left))
		{
			map->scroll.x = (mx - TIC_MAP_SCREEN_WIDTH / 2 + world->scroll.x) * TIC_SPRITESIZE;
			map->scroll.y = (my - TIC_MAP_SCREEN_HEIGHT / 2 + world->scroll.y) * TIC_SPRITESIZE;
			if (map->scroll.x < 0)
				map->scroll.x += TIC_MAP_WIDTH * TIC_SPRITESIZE;
			if (map->scroll.y < 0)
				map->scroll.y += TIC_MAP_HEIGHT * TIC_SPRITESIZE;
		}

		if (checkMouseClick(&rect, tic_mouse_left))
			setStudioMode(TIC_MAP_MODE);
	}

	s32 x = map->scroll.x / TIC_SPRITESIZE - world->scroll.x;
	s32 y = map->scroll.y / TIC_SPRITESIZE - world->scroll.y;

	normalizeWorld(&x, &y);

	tic_api_rectb(world->tic, x, y, TIC_MAP_SCREEN_WIDTH + 1, TIC_MAP_SCREEN_HEIGHT + 1, tic_color_2);
	/*
	if (x >= TIC_MAP_WIDTH - TIC_MAP_SCREEN_WIDTH)
		tic_api_rectb(world->tic, x - TIC_MAP_WIDTH, y, TIC_MAP_SCREEN_WIDTH + 1, TIC_MAP_SCREEN_HEIGHT + 1, tic_color_3);

	if (y >= TIC_MAP_HEIGHT - TIC_MAP_SCREEN_HEIGHT)
		tic_api_rectb(world->tic, x, y - TIC_MAP_HEIGHT, TIC_MAP_SCREEN_WIDTH + 1, TIC_MAP_SCREEN_HEIGHT + 1, tic_color_4);

	if (x >= TIC_MAP_WIDTH - TIC_MAP_SCREEN_WIDTH && y >= TIC_MAP_HEIGHT - TIC_MAP_SCREEN_HEIGHT)
		tic_api_rectb(world->tic, x - TIC_MAP_WIDTH, y - TIC_MAP_HEIGHT, TIC_MAP_SCREEN_WIDTH + 1, TIC_MAP_SCREEN_HEIGHT + 1, tic_color_5);
	*/
}

static void drawGrid(World* world)
{
    Map* map = world->map;
    u8 color = tic_color_14;
	{
		s32 screenScrollX = world->scroll.x % TIC_MAP_SCREEN_WIDTH;
		s32 screenScrollY = world->scroll.y % TIC_MAP_SCREEN_HEIGHT;

		for (s32 c = 0; c <= TIC80_WIDTH; c += TIC_MAP_SCREEN_WIDTH)	
			tic_api_line(world->tic, c - screenScrollX, 0, c - screenScrollX, TIC80_HEIGHT, color);

		for (s32 r = 0; r <= TIC80_HEIGHT; r += TIC_MAP_SCREEN_HEIGHT)
			tic_api_line(world->tic, 0, r-screenScrollY, TIC80_WIDTH, r-screenScrollY, color);
	}
	
	{ 
		// world boundaries (do I really need do draw 2 extra lines instead of having more clever solution in the 'for'cycles up above?)
		s32 screenScrollX2 = world->scroll.x % (TIC_MAP_WIDTH);
		s32 screenScrollY2 = world->scroll.y % (TIC_MAP_HEIGHT);
		tic_api_line(world->tic, TIC_MAP_WIDTH - screenScrollX2, 0, TIC_MAP_WIDTH - screenScrollX2, TIC80_HEIGHT, tic_color_2);
		tic_api_line(world->tic, 0, TIC_MAP_HEIGHT - screenScrollY2, TIC80_WIDTH, TIC_MAP_HEIGHT - screenScrollY2, tic_color_2);

	}
}

static void processKeyboard(World* world)
{
	if (keyWasPressed(tic_key_tab)) setStudioMode(TIC_MAP_MODE);
}

static void tick(World* world)
{
	tic_mem* tic = world->tic;

	//world->tickCounter++;
	processKeyboard(world);
}

static void tock(tic_mem* tic, void* data)
{
	tic_rect rect = { WORLD_X, WORLD_Y, WORLD_WIDTH, WORLD_HEIGHT };

	World* world = (World*)data;
	
	//memcpy(&world->tic->ram.vram, world->preview, PREVIEW_SIZE);
	for (u32 x = 0; x < TIC80_WIDTH; x++)
		for (u32 y = 0; y < TIC80_HEIGHT; y++)
		{
			u32 xx = x + world->scroll.x;
			u32 yy = y + world->scroll.y;
			normalizeWorld(&xx, &yy);

			u8 buf = tic_tool_peek(world->preview, xx + yy * TIC80_WIDTH * 2);
			//tic_api_pix(&world->tic->ram.vram, x, y, buf, false);
			//if (buf != 255) tic_tool_poke(&world->tic->ram.vram, x + y * TIC80_WIDTH, buf);
			if (buf != 255) tic_api_pix(&tic->ram.vram, x, y, buf, false);
		}

	if (checkMousePos(&rect))
	{
				processScrolling(world, checkMouseDown(&rect, tic_mouse_right));
	}
}

static void scanline(tic_mem* tic, s32 row, void* data)
{
   // if(row == 0)
   //     memcpy(&tic->ram.vram.palette, getBankPalette(), sizeof(tic_palette));
}

static void overline(tic_mem* tic, void* data)
{
    World* world = (World*)data;

    drawGrid(world);
	drawMapCursor(world);
}

static void drawBackground(tic_mem *tic, void* data)
{
	World* world = (World*)data;
	//draw map background for showing transparency on map
	for (u8 j = 0; j < TIC80_WIDTH / TIC_SPRITESIZE; j++)
		for (u8 i = 0; i < TIC80_HEIGHT / TIC_SPRITESIZE; i++)
			tic_api_spr(tic, &getConfig()->cart->bank0.tiles, world->map->bgsprite, j*TIC_SPRITESIZE, i*TIC_SPRITESIZE, 1, 1, NULL, 0, 1, tic_no_flip, tic_no_rotate);
}

void initWorld(World* world, tic_mem* tic, Map* map)
{
    if(!world->preview)
        world->preview = malloc(PREVIEW_SIZE);

    *world = (World)
    {
        .tic = tic,
        .map = map,
        .tick = tick,
        .preview = world->preview,
		.scroll =
		{
			.x = map->scroll.x/TIC_SPRITESIZE-TIC80_WIDTH/2+TIC_MAP_SCREEN_WIDTH/2,
			.y = map->scroll.y/TIC_SPRITESIZE-TIC80_HEIGHT/2+TIC_MAP_SCREEN_HEIGHT/2,
			.active = false,
			.gesture = false,
			.start = {0, 0},
		},
        .overline = overline,
        .scanline = scanline,
		.background = drawBackground,
		.tock = tock,
    };

    memset(world->preview, 0, PREVIEW_SIZE);
    s32 colors[TIC_PALETTE_SIZE];

    for(s32 i = 0; i < TIC_MAP_WIDTH * TIC_MAP_HEIGHT; i++)
    {
        //u8 index = getBankMap()->data[i];
        u16 index = CLAMP(getBankMap()->data[i],0,TIC_BANK_SPRITES*2);

        if(index<=2047)
        {
            memset(colors, 0, sizeof colors);

            tic_tile* tile = &getBankTiles()->data[index];

            for(s32 p = 0; p < TIC_SPRITESIZE * TIC_SPRITESIZE; p++)
            {
                u8 color = tic_tool_peek(tile, p);

                if(color)
                    colors[color]++;
            }

            s32 max = 0;

            for(s32 c = 0; c < COUNT_OF(colors); c++)
                if(colors[c] > colors[max]) max = c;

            tic_tool_poke(world->preview, i, max);
		}
		else {
			tic_tool_poke(world->preview, i, 255);
		}
    }
}

void freeWorld(World* world)
{
    free(world->preview);
    free(world);
}
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

#include "sprite.h"
#include "history.h"

#define CANVAS_SIZE (64)
#define PALETTE_CELL_SIZE 8
#define PALETTE_ROWS 2
#define PALETTE_COLS (TIC_PALETTE_SIZE / PALETTE_ROWS)
#define PALETTE_WIDTH (PALETTE_COLS * PALETTE_CELL_SIZE)
#define PALETTE_HEIGHT (PALETTE_ROWS * PALETTE_CELL_SIZE)
#define SHEET_COLS (TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE)

enum
{
    ToolbarH = TOOLBAR_SIZE,
    CanvasX = 24, CanvasY = 20, CanvasW = CANVAS_SIZE, CanvasH = CANVAS_SIZE,
    PaletteX = 24, PaletteY = 112, PaletteW = PALETTE_WIDTH, PaletteH = PALETTE_HEIGHT,
    SheetX = TIC80_WIDTH - TIC_SPRITESHEET_SIZE - 1, SheetY = CanvasY-1, SheetW = TIC_SPRITESHEET_SIZE, SheetH = TIC_SPRITESHEET_SIZE,
};

// !TODO: move it to helpers place
static void drawPanelBorder(tic_mem* tic, s32 x, s32 y, s32 w, s32 h)
{
    tic_api_rect(tic, x, y-1, w, 1, tic_color_15);
    tic_api_rect(tic, x-1, y, 1, h, tic_color_15);
    tic_api_rect(tic, x, y+h, w, 1, tic_color_13);
    tic_api_rect(tic, x+w, y, 1, h, tic_color_13);
}

static void clearCanvasSelection(Sprite* sprite)
{
    memset(&sprite->select.rect, 0, sizeof(tic_rect));
}

static u8 getSheetPixel(Sprite* sprite, s32 x, s32 y)
{
    //return getSpritePixel(sprite->src->data, x, sprite->index >= TIC_BANK_SPRITES ? y + TIC_SPRITESHEET_SIZE: y);
    //return getSpritePixel(sprite->src->data, x, sprite->index >= TIC_PAGE_SPRITES ? y + TIC_SPRITESHEET_SIZE: y);
    return getSpritePixel(sprite->src->data, x, y + TIC_SPRITESHEET_SIZE*(sprite->index / TIC_PAGE_SPRITES));
}

static void setSheetPixel(Sprite* sprite, s32 x, s32 y, u8 color)
{
    //setSpritePixel(sprite->src->data, x, sprite->index >= TIC_BANK_SPRITES ? y + TIC_SPRITESHEET_SIZE: y, color);
    //setSpritePixel(sprite->src->data, x, sprite->index >= TIC_PAGE_SPRITES ? y + TIC_SPRITESHEET_SIZE: y, color);
    setSpritePixel(sprite->src->data, x,  y + TIC_SPRITESHEET_SIZE*(sprite->index / TIC_PAGE_SPRITES), color);
}

static void setSheetLine(Sprite* sprite, s32 x1, s32 y1, s32 x2, s32 y2, u8 color)
{
	u32 dx = x2 - x1;
	u32 dy = y2 - y1;
	if (dx != 0) {
		for (s32 x = x1; x < x2; x++)
		{
			s32 y = y1 + dy * (x - x1) / dx;
			setSheetPixel(sprite, x, y, color);
		}
	}
}

static s32 getIndexPosX(Sprite* sprite)
{
    //s32 index = sprite->index % TIC_BANK_SPRITES;
    s32 index = sprite->index % TIC_PAGE_SPRITES;
    //s32 index = sprite->index % TIC_BANK_SPRITES*TIC_SPRITE_BANKS - sprite->index % TIC_PAGE_SPRITES;
    return index % SHEET_COLS * TIC_SPRITESIZE;
    //return index % SHEET_COLS;
}

static s32 getIndexPosY(Sprite* sprite)
{
    //s32 index = sprite->index % TIC_BANK_SPRITES;
    s32 index = sprite->index % TIC_PAGE_SPRITES;
    //s32 index = sprite->index % TIC_BANK_SPRITES*TIC_SPRITE_BANKS - sprite->index % TIC_PAGE_SPRITES;
    return index / SHEET_COLS * TIC_SPRITESIZE;
    //return index / SHEET_COLS;
}

static void drawSelection(Sprite* sprite, s32 x, s32 y, s32 w, s32 h) // draw selection inside sprite canvas
{
    tic_mem* tic = sprite->tic;

    enum{Step = 3};
    u8 color = tic_color_12;

    s32 index = sprite->tickCounter / 10;
    for(s32 i = x; i < (x+w); i++)      { tic_api_pix(tic, i, y, index++ % Step ? color : 0, false);} index++;
    for(s32 i = y; i < (y+h); i++)      { tic_api_pix(tic, x + w-1, i, index++ % Step ? color : 0, false);} index++;
    for(s32 i = (x+w-1); i >= x; i--)   { tic_api_pix(tic, i, y + h-1, index++ % Step ? color : 0, false);} index++;
    for(s32 i = (y+h-1); i >= y; i--)   { tic_api_pix(tic, x, i, index++ % Step ? color : 0, false);}
}

static tic_rect getSpriteRect(Sprite* sprite)
{
    s32 x = getIndexPosX(sprite);
    s32 y = getIndexPosY(sprite);

    return (tic_rect){x, y, sprite->size, sprite->size};
}

static void drawCursorBorder(Sprite* sprite, s32 x, s32 y, s32 w, s32 h)
{
    tic_mem* tic = sprite->tic;

    tic_api_rectb(tic, x, y, w, h, tic_color_0);
    tic_api_rectb(tic, x-1, y-1, w+2, h+2, tic_color_12);
}

static void processPickerCanvasMouse(Sprite* sprite, s32 x, s32 y, s32 sx, s32 sy)
{
    tic_rect rect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
    const s32 Size = CANVAS_SIZE / sprite->size;

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        s32 mx = getMouseX() - x;
        s32 my = getMouseY() - y;

        mx -= mx % Size;
        my -= my % Size;

        drawCursorBorder(sprite, x + mx, y + my, Size, Size);

        if(checkMouseDown(&rect, tic_mouse_left))
            sprite->color = getSheetPixel(sprite, sx + mx / Size, sy + my / Size);

        if(checkMouseDown(&rect, tic_mouse_right))
            sprite->color2 = getSheetPixel(sprite, sx + mx / Size, sy + my / Size);
    }
}

static void processDrawCanvasMouse(Sprite* sprite, s32 x, s32 y, s32 sx, s32 sy)
{
    tic_rect rect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
    const s32 Size = CANVAS_SIZE / sprite->size;

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        s32 mx = getMouseX() - x;
        s32 my = getMouseY() - y;

        s32 brushSize = sprite->brushSize*Size;
        s32 offset = (brushSize - Size) / 2;

        mx -= offset;
        my -= offset;
        mx -= mx % Size;
        my -= my % Size;

        if(mx < 0) mx = 0;
        if(my < 0) my = 0;
        if(mx+brushSize >= CANVAS_SIZE) mx = CANVAS_SIZE - brushSize;
        if(my+brushSize >= CANVAS_SIZE) my = CANVAS_SIZE - brushSize;

        SHOW_TOOLTIP("[x=%02i y=%02i]", mx / Size, my / Size);

        drawCursorBorder(sprite, x + mx, y + my, brushSize, brushSize);

        bool left = checkMouseDown(&rect, tic_mouse_left);
        bool right = checkMouseDown(&rect, tic_mouse_right);

        if(left || right)
        {
            sx += mx / Size;
            sy += my / Size;
            u8 color = left ? sprite->color : sprite->color2;
            s32 pixels = sprite->brushSize;

            for(s32 j = 0; j < pixels; j++)
                for(s32 i = 0; i < pixels; i++)
                    setSheetPixel(sprite, sx+i, sy+j, color);

            history_add(sprite->history);
        }
    }
}

static void pasteSelection(Sprite* sprite)
{
    s32 l = getIndexPosX(sprite);
    s32 t = getIndexPosY(sprite);
    s32 r = l + sprite->size;
    s32 b = t + sprite->size;

    for(s32 sy = t, i = 0; sy < b; sy++)
        for(s32 sx = l; sx < r; sx++)
            setSheetPixel(sprite, sx, sy, sprite->select.back[i++]);

    tic_rect* rect = &sprite->select.rect;

    l += rect->x;
    t += rect->y;
    r = l + rect->w;
    b = t + rect->h;

    for(s32 sy = t, i = 0; sy < b; sy++)
        for(s32 sx = l; sx < r; sx++)
            setSheetPixel(sprite, sx, sy, sprite->select.front[i++]);

    history_add(sprite->history);
}

static void copySelection(Sprite* sprite)
{
    tic_rect rect = getSpriteRect(sprite);
    s32 r = rect.x + rect.w;
    s32 b = rect.y + rect.h;

    for(s32 sy = rect.y, i = 0; sy < b; sy++)
        for(s32 sx = rect.x; sx < r; sx++)
            sprite->select.back[i++] = getSheetPixel(sprite, sx, sy);

    {
        tic_rect* rect = &sprite->select.rect;
        memset(sprite->select.front, 0, CANVAS_SIZE * CANVAS_SIZE);

        for(s32 j = rect->y, index = 0; j < (rect->y + rect->h); j++)
            for(s32 i = rect->x; i < (rect->x + rect->w); i++)
            {
                u8* color = &sprite->select.back[i+j*sprite->size];
                sprite->select.front[index++] = *color;
                *color = sprite->color2;
            }
    }
}

static void processSelectCanvasMouse(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    tic_rect rect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
    const s32 Size = CANVAS_SIZE / sprite->size;

    bool endDrag = false;

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        s32 mx = getMouseX() - x;
        s32 my = getMouseY() - y;

        mx -= mx % Size;
        my -= my % Size;

        drawCursorBorder(sprite, x + mx, y + my, Size, Size);

        if(checkMouseDown(&rect, tic_mouse_left))
        {
            if(sprite->select.drag)
            {
                s32 x = mx / Size;
                s32 y = my / Size;

                s32 rl = MIN(x, sprite->select.start.x);
                s32 rt = MIN(y, sprite->select.start.y);
                s32 rr = MAX(x, sprite->select.start.x);
                s32 rb = MAX(y, sprite->select.start.y);

                sprite->select.rect = (tic_rect){rl, rt, rr - rl + 1, rb - rt + 1};
            }
            else
            {
                sprite->select.drag = true;
                sprite->select.start = (tic_point){mx / Size, my / Size};
                sprite->select.rect = (tic_rect){sprite->select.start.x, sprite->select.start.y, 1, 1};
            }
        }
        else endDrag = sprite->select.drag;
    }
    else endDrag = !tic->ram.input.mouse.left && sprite->select.drag;

    if(endDrag)
    {
        copySelection(sprite);
        sprite->select.drag = false;
    }
}

static void floodFill(Sprite* sprite, s32 l, s32 t, s32 r, s32 b, s32 x, s32 y, u8 color, u8 fill)
{
    if(getSheetPixel(sprite, x, y) == color)
    {
        setSheetPixel(sprite, x, y, fill);

        if(x > l) floodFill(sprite, l, t, r, b, x-1, y, color, fill);
        if(x < r) floodFill(sprite, l, t, r, b, x+1, y, color, fill);
        if(y > t) floodFill(sprite, l, t, r, b, x, y-1, color, fill);
        if(y < b) floodFill(sprite, l, t, r, b, x, y+1, color, fill);
    }
}

static void replaceColor(Sprite* sprite, s32 l, s32 t, s32 r, s32 b, s32 x, s32 y, u8 color, u8 fill)
{
    for(s32 sy = t; sy <= b; sy++)
        for(s32 sx = l; sx <= r; sx++)
            if(getSheetPixel(sprite, sx, sy) == color)
                setSheetPixel(sprite, sx, sy, fill);
}

static void processFillCanvasMouse(Sprite* sprite, s32 x, s32 y, s32 l, s32 t)
{
    tic_mem* tic = sprite->tic;
    tic_rect rect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
    const s32 Size = CANVAS_SIZE / sprite->size;

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        s32 mx = getMouseX() - x;
        s32 my = getMouseY() - y;

        mx -= mx % Size;
        my -= my % Size;

        drawCursorBorder(sprite, x + mx, y + my, Size, Size);

        bool left = checkMouseClick(&rect, tic_mouse_left);
        bool right = checkMouseClick(&rect, tic_mouse_right);

        if(left || right)
        {
            s32 sx = l + mx / Size;
            s32 sy = t + my / Size;

            u8 color = getSheetPixel(sprite, sx, sy);
            u8 fill = left ? sprite->color : sprite->color2;

            if(color != fill)
            {
                tic_api_key(tic, tic_key_ctrl)
                    ? replaceColor(sprite, l, t, l + sprite->size-1, t + sprite->size-1, sx, sy, color, fill)
                    : floodFill(sprite, l, t, l + sprite->size-1, t + sprite->size-1, sx, sy, color, fill);
            }

            history_add(sprite->history);
        }
    }
}

static bool hasCanvasSelection(Sprite* sprite)
{
    return sprite->mode == SPRITE_SELECT_MODE && sprite->select.rect.w && sprite->select.rect.h;
}

static void drawBrushSlider(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    enum {Count = 4, Size = 5};

    tic_rect rect = {x, y, Size, (Size+1)*Count};

    bool over = false;
    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        showTooltip("BRUSH SIZE");
        over = true;

        if(checkMouseDown(&rect, tic_mouse_left))
        {
            s32 my = getMouseY() - y;

            sprite->brushSize = Count - my / (Size+1);
        }
    }

    tic_api_rect(tic, x+1, y, Size-2, Size*Count, tic_color_0);

    for(s32 i = 0; i < Count; i++)
    {
        s32 offset = y + i*(Size+1);

        tic_api_rect(tic, x, offset, Size, Size, tic_color_0);
        tic_api_rect(tic, x + 6, offset + 2, Count - i, 1, tic_color_0);
    }

    tic_api_rect(tic, x+2, y+1, 1, Size*Count+1, (over ? tic_color_12 : tic_color_14));

    s32 offset = y + (Count - sprite->brushSize)*(Size+1);
    tic_api_rect(tic, x, offset, Size, Size, tic_color_0);
    tic_api_rect(tic, x+1, offset+1, Size-2, Size-2, (over ? tic_color_12 : tic_color_14));
}

static void drawCanvasOvr(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    const s32 Size = CANVAS_SIZE / sprite->size;
    const tic_rect rect = getSpriteRect(sprite);

    const tic_rect canvasRect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
    if(checkMouseDown(&canvasRect, tic_mouse_middle))
    {
        s32 mx = getMouseX() - x;
        s32 my = getMouseY() - y;
        sprite->color = getSheetPixel(sprite, rect.x + mx / Size, rect.y + my / Size);
    }

    drawPanelBorder(tic, canvasRect.x - 1, canvasRect.y - 1, canvasRect.w + 2, canvasRect.h + 2);
    tic_api_rectb(tic, canvasRect.x - 1, canvasRect.y - 1, canvasRect.w + 2, canvasRect.h + 2, tic_color_0);

    if(!sprite->editPalette)
    {
        switch(sprite->mode)
        {
        case SPRITE_DRAW_MODE: 
            processDrawCanvasMouse(sprite, x, y, rect.x, rect.y);
            drawBrushSlider(sprite, x - 15, y + 20);
            break;
        case SPRITE_PICK_MODE: processPickerCanvasMouse(sprite, x, y, rect.x, rect.y); break;
        case SPRITE_SELECT_MODE: processSelectCanvasMouse(sprite, x, y); break;
        case SPRITE_FILL_MODE: processFillCanvasMouse(sprite, x, y, rect.x, rect.y); break;
        }       
    }

    if(hasCanvasSelection(sprite))
        drawSelection(sprite, x + sprite->select.rect.x * Size - 1, y + sprite->select.rect.y * Size - 1, 
            sprite->select.rect.w * Size + 2, sprite->select.rect.h * Size + 2);
    else
    {
        static const char Format[] = "#%03i";
        char buf[sizeof Format];
        sprintf(buf, Format, sprite->index);

        s32 ix = x + (CANVAS_SIZE - 4*TIC_FONT_WIDTH)/2;
        s32 iy = TIC_SPRITESIZE + 2;
        tic_api_print(tic, buf, ix, iy+1, tic_color_0, true, 1, false);
        tic_api_print(tic, buf, ix, iy, tic_color_12, true, 1, false);
    }
}

static void drawCanvas(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    tic_rect rect = getSpriteRect(sprite);
    s32 r = rect.x + rect.w;
    s32 b = rect.y + rect.h;

    const s32 Size = CANVAS_SIZE / sprite->size;

    for(s32 sy = rect.y, j = y; sy < b; sy++, j += Size)
        for(s32 sx = rect.x, i = x; sx < r; sx++, i += Size)
            tic_api_rect(tic, i, j, Size, Size, getSheetPixel(sprite, sx, sy));
}

static void upCanvas(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    if(rect->y > 0) rect->y--;
    pasteSelection(sprite);
}

static void downCanvas(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    if(rect->y + rect->h < sprite->size) rect->y++;
    pasteSelection(sprite);
}

static void leftCanvas(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    if(rect->x > 0) rect->x--;
    pasteSelection(sprite);
}

static void rightCanvas(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    if(rect->x + rect->w < sprite->size) rect->x++;
    pasteSelection(sprite);
}

static void rotateSelectRect(Sprite* sprite)
{
    tic_rect rect = sprite->select.rect;
    
    s32 selection_center_x = rect.x + rect.w/2;
    s32 selection_center_y = rect.y + rect.h/2;
    
    // Rotate
    sprite->select.rect.w = rect.h;
    sprite->select.rect.h = rect.w;
    
    // Make the new center be at the position of the previous center
    sprite->select.rect.x -= (sprite->select.rect.x + sprite->select.rect.w/2) - selection_center_x;
    sprite->select.rect.y -= (sprite->select.rect.y + sprite->select.rect.h/2) - selection_center_y;
    
    // Check if we are not out of boundaries
    if (sprite->select.rect.x < 0) sprite->select.rect.x = 0;
    if (sprite->select.rect.y < 0) sprite->select.rect.y = 0;
    
    if (sprite->select.rect.x + sprite->select.rect.w >= sprite->size)
    {
        sprite->select.rect.x -= sprite->select.rect.x + sprite->select.rect.w - sprite->size;
    }
    
    if (sprite->select.rect.y + sprite->select.rect.h >= sprite->size)
    {
        sprite->select.rect.y -= sprite->select.rect.y + sprite->select.rect.h - sprite->size;
    }
}

static void rotateCanvas(Sprite* sprite)
{
    u8* buffer = (u8*)malloc(CANVAS_SIZE*CANVAS_SIZE);

    if(buffer)
    {
        {
            tic_rect rect = sprite->select.rect;
            const s32 Size = rect.h * rect.w;
            s32 diff = 0;

            for(s32 y = 0, i = 0; y < rect.w; y++)
                for(s32 x = 0; x < rect.h; x++)
                {
                    diff = rect.w * (x + 1) -y;
                    buffer[i++] = sprite->select.front[Size - diff];
                }
            
            for (s32 i = 0; i<Size; i++)
                sprite->select.front[i] = buffer[i];
            
            rotateSelectRect(sprite);
            pasteSelection(sprite);
            history_add(sprite->history);
        }

        free(buffer);
    }
}

static void deleteCanvas(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    
    s32 left = getIndexPosX(sprite) + rect->x;
    s32 top = getIndexPosY(sprite) + rect->y;
    s32 right = left + rect->w;
    s32 bottom = top + rect->h;

    for(s32 pixel_y = top; pixel_y < bottom; pixel_y++)
        for(s32 pixel_x = left; pixel_x < right; pixel_x++)
            setSheetPixel(sprite, pixel_x, pixel_y, sprite->color2);

    clearCanvasSelection(sprite);
    
    history_add(sprite->history);
}

static void flipCanvasHorz(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    
    s32 sprite_x = getIndexPosX(sprite);
    s32 sprite_y = getIndexPosY(sprite);
    
    s32 right = sprite_x + rect->x + rect->w/2;
    s32 bottom = sprite_y + rect->y + rect->h;

    for(s32 y = sprite_y + rect->y; y < bottom; y++)
        for(s32 x = sprite_x + rect->x, i = sprite_x + rect->x + rect->w - 1; x < right; x++, i--)
        {
            u8 color = getSheetPixel(sprite, x, y);
            setSheetPixel(sprite, x, y, getSheetPixel(sprite, i, y));
            setSheetPixel(sprite, i, y, color);
        }

    history_add(sprite->history);
    copySelection(sprite);
}

static void flipCanvasVert(Sprite* sprite)
{
    tic_rect* rect = &sprite->select.rect;
    
    s32 sprite_x = getIndexPosX(sprite);
    s32 sprite_y = getIndexPosY(sprite);
    
    s32 right = sprite_x + rect->x + rect->w;
    s32 bottom = sprite_y + rect->y + rect->h/2;

    for(s32 y = sprite_y + rect->y, i = sprite_y + rect->y + rect->h - 1; y < bottom; y++, i--)
        for(s32 x = sprite_x + rect->x; x < right; x++)
        {
            u8 color = getSheetPixel(sprite, x, y);
            setSheetPixel(sprite, x, y, getSheetPixel(sprite, x, i));
            setSheetPixel(sprite, x, i, color);
        }

    history_add(sprite->history);
    copySelection(sprite);
}

static s32* getSpriteIndexes(Sprite* sprite)
{
    static s32 indexes[TIC_SPRITESIZE*TIC_SPRITESIZE+1];
    memset(indexes, -1, sizeof indexes);

    {
        tic_rect r = {sprite->index % SHEET_COLS, sprite->index / SHEET_COLS
            , sprite->size / TIC_SPRITESIZE, sprite->size / TIC_SPRITESIZE};

        s32 c = 0;
        for(s32 j = r.y; j < r.h + r.y; j++)
            for(s32 i = r.x; i < r.w + r.x; i++)
                indexes[c++] = i + j * SHEET_COLS;

    }

    return indexes;
}

static void drawFlags(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    if(hasCanvasSelection(sprite)) return;

    enum {Flags = 8, Size = 5};

    u8* flags = getBankFlags()->data;
    u8 or = 0;
    u8 and = 0xff;

    s32* indexes = getSpriteIndexes(sprite);

    {
        s32* i = indexes;
        while(*i >= 0)
        {
            or |= flags[*i];
            and &= flags[*i];
            i++;
        }       
    }

    for(s32 i = 0; i < Flags; i++)
    {
        const u8 mask = 1 << i;
        tic_rect rect = {x, y + (Size+1)*i, Size, Size};

        bool over = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            SHOW_TOOLTIP("set flag [%i]", i);

            if(checkMouseClick(&rect, tic_mouse_left))
            {
                s32* i = indexes;

                if(or & mask)
                    while(*i >= 0)
                        flags[*i++] &= ~mask;
                else
                    while(*i >= 0)
                        flags[*i++] |= mask;
            }
        }

        tic_api_rect(tic, rect.x, rect.y, Size, Size, tic_color_0);

        u8 flagColor = i + 2;

        if(or & mask)
            tic_api_pix(tic, rect.x+2, rect.y+2, flagColor, false);
        else if(over)
            tic_api_rect(tic, rect.x+1, rect.y+1, Size-2, Size-2, flagColor);
        
        if(and & mask)
        {
            tic_api_rect(tic, rect.x+1, rect.y+1, Size-2, Size-2, flagColor);
            tic_api_pix(tic, rect.x+3, rect.y+1, tic_color_12, false);
        }

        tic_api_print(tic, (char[]){'0' + i, '\0'}, rect.x + (Size+2), rect.y, tic_color_13, false, 1, true);
    }
}

static void drawMoveButtons(Sprite* sprite)
{
    if(hasCanvasSelection(sprite))
    {
        enum { x = 24 };
        enum { y = 20 };

        static const u8 Icons[] = 
        {
            0b00010000,
            0b00111000,
            0b01111100,
            0b11111110,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,

            0b11111110,
            0b01111100,
            0b00111000,
            0b00010000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,

            0b00010000,
            0b00110000,
            0b01110000,
            0b11110000,
            0b01110000,
            0b00110000,
            0b00010000,
            0b00000000,

            0b10000000,
            0b11000000,
            0b11100000,
            0b11110000,
            0b11100000,
            0b11000000,
            0b10000000,
            0b00000000,
        };

        static const tic_rect Rects[] = 
        {
            {x + (CANVAS_SIZE - TIC_SPRITESIZE)/2, y - TIC_SPRITESIZE, TIC_SPRITESIZE, TIC_SPRITESIZE/2},
            {x + (CANVAS_SIZE - TIC_SPRITESIZE)/2, y + CANVAS_SIZE + TIC_SPRITESIZE/2, TIC_SPRITESIZE, TIC_SPRITESIZE/2},
            {x - TIC_SPRITESIZE, y + (CANVAS_SIZE - TIC_SPRITESIZE)/2, TIC_SPRITESIZE/2, TIC_SPRITESIZE},
            {x + CANVAS_SIZE + TIC_SPRITESIZE/2, y + (CANVAS_SIZE - TIC_SPRITESIZE)/2, TIC_SPRITESIZE/2, TIC_SPRITESIZE},
        };

        static void(* const Func[])(Sprite*) = {upCanvas, downCanvas, leftCanvas, rightCanvas};

        bool down = false;
        for(s32 i = 0; i < sizeof Icons / 8; i++)
        {
            down = false;

            if(checkMousePos(&Rects[i]))
            {
                setCursor(tic_cursor_hand);

                if(checkMouseDown(&Rects[i], tic_mouse_left)) down = true;

                if(checkMouseClick(&Rects[i], tic_mouse_left))
                    Func[i](sprite);
            }

            drawBitIcon(Rects[i].x, Rects[i].y+1, Icons + i*8, down ? tic_color_12 : tic_color_0);

            if(!down) drawBitIcon(Rects[i].x, Rects[i].y, Icons + i*8, tic_color_12);
        }
    }
}

static void drawRGBSlider(Sprite* sprite, s32 x, s32 y, u8* value)
{
    tic_mem* tic = sprite->tic;

    enum {Size = CANVAS_SIZE, Max = 255};

    {
        static const u8 Icon[] =
        {
            0b11100000,
            0b11100000,
            0b11100000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
        };

        tic_rect rect = {x, y-2, Size, 5};

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            if(checkMouseDown(&rect, tic_mouse_left))
            {
                s32 mx = getMouseX() - x;
                *value = mx * Max / (Size-1);
            }
        }

        tic_api_rect(tic, x, y+1, Size, 1, tic_color_0);
        tic_api_rect(tic, x, y, Size, 1, tic_color_12);

        {
            s32 offset = x + *value * (Size-1) / Max - 1;
            drawBitIcon(offset, y, Icon, tic_color_0);
            drawBitIcon(offset, y-1, Icon, tic_color_12);
        }

        {
            char buf[] = "FF";
            sprintf(buf, "%02X", *value);
            tic_api_print(tic, buf, x - 18, y - 2, tic_color_13, true, 1, false);
        }
    }

    {
        static const u8 Icon[] =
        {
            0b01000000,
            0b11000000,
            0b01000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
        };

        tic_rect rect = {x - 4, y - 1, 2, 3};

        bool down = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            if(checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(&rect, tic_mouse_left))
                (*value)--;
        }

        if(down)
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_12);
        }
        else
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_0);
            drawBitIcon(rect.x, rect.y, Icon, tic_color_12);
        }
    }

    {
        static const u8 Icon[] =
        {
            0b10000000,
            0b11000000,
            0b10000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
            0b00000000,
        };

        tic_rect rect = {x + Size + 2, y - 1, 2, 3};

        bool down = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            if(checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(&rect, tic_mouse_left))
                (*value)++;
        }

        if(down)
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_12);
        }
        else
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_0);
            drawBitIcon(rect.x, rect.y, Icon, tic_color_12);
        }
    }
}

static void pasteColor(Sprite* sprite)
{
    fromClipboard(getBankPalette()->data, sizeof(tic_palette), false, true);
    fromClipboard(&getBankPalette()->colors[sprite->color], sizeof(tic_rgb), false, true);
}

static void drawRGBTools(Sprite* sprite, s32 x, s32 y)
{
    {
        enum{Size = 5};
        static const u8 Icon[] = 
        {
            0b11110000,
            0b10010000,
            0b10111000,
            0b11101000,
            0b00111000,
            0b00000000,
            0b00000000,
            0b00000000, 
        };

        tic_rect rect = {x, y, Size, Size};

        bool over = false;
                bool down = false;

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip("COPY PALETTE");
            over = true;

            if(checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(&rect, tic_mouse_left))
                toClipboard(getBankPalette()->data, sizeof(tic_palette), false);
        }

        if(down)
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_13);
        }
        else
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_0);
            drawBitIcon(rect.x, rect.y, Icon, (over ? tic_color_13 : tic_color_12));
        }
    }

    {
        enum{Size = 5};
        static const u8 Icon[] = 
        {
            0b01110000,
            0b10001000,
            0b11111000,
            0b11011000,
            0b11111000,
            0b00000000,
            0b00000000,
            0b00000000,
        };

        tic_rect rect = {x + 8, y, Size, Size};
        bool over = false;
        bool down = false;

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip("PASTE PALETTE");
            over = true;

            if(checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(&rect, tic_mouse_left))
            {
                pasteColor(sprite);
            }
        }

        if(down)
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_13);
        }
        else
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_0);
            drawBitIcon(rect.x, rect.y, Icon, (over ? tic_color_13 : tic_color_12));
        }
    }
}

static void drawRGBSliders(Sprite* sprite, s32 x, s32 y)
{
    enum{Gap = 6, Count = sizeof(tic_rgb)};

    u8* data = &getBankPalette()->data[sprite->color * Count];

    for(s32 i = 0; i < Count; i++)
        drawRGBSlider(sprite, x, y + Gap*i, &data[i]);

    drawRGBTools(sprite, x - 18, y + 26);
}

static void drawPaletteOvr(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;
    tic_rect rect = {x, y, PALETTE_WIDTH-1, PALETTE_HEIGHT-1};

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        s32 mx = getMouseX() - x;
        s32 my = getMouseY() - y;

        mx /= PALETTE_CELL_SIZE;
        my /= PALETTE_CELL_SIZE;

        s32 index = mx + my * PALETTE_COLS;

        SHOW_TOOLTIP("color [%02i]", index);

        bool left = checkMouseDown(&rect, tic_mouse_left);
        bool right = checkMouseDown(&rect, tic_mouse_right);

        if(left || right)
        {
            if(left) sprite->color = index;
            if(right) sprite->color2 = index;
        }
    }

    enum {Gap = 1};

    drawPanelBorder(tic, x - Gap, y - Gap, PaletteW + Gap, PaletteH + Gap);

    for(s32 row = 0, i = 0; row < PALETTE_ROWS; row++)
        for(s32 col = 0; col < PALETTE_COLS; col++)
            tic_api_rectb(tic, x + col * PALETTE_CELL_SIZE - Gap, y + row * PALETTE_CELL_SIZE - Gap, 
                PALETTE_CELL_SIZE + Gap, PALETTE_CELL_SIZE + Gap, tic_color_0);

    {
        s32 offsetX = x + (sprite->color % PALETTE_COLS) * PALETTE_CELL_SIZE;
        s32 offsetY = y + (sprite->color / PALETTE_COLS) * PALETTE_CELL_SIZE;
        tic_api_rectb(tic, offsetX - 1, offsetY - 1, PALETTE_CELL_SIZE + 1, PALETTE_CELL_SIZE + 1, tic_color_12);
    }

    {
        static const u16 Icon[] = 
        {
            0b1010101010000000,
            0b0000000000000000,
            0b1000000010000000,
            0b0000000000000000,
            0b1000000010000000,
            0b0000000000000000,
            0b1000000010000000,
            0b0000000000000000,
            0b1010101010000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        };

        s32 offsetX = x + (sprite->color2 % PALETTE_COLS) * PALETTE_CELL_SIZE;
        s32 offsetY = y + (sprite->color2 / PALETTE_COLS) * PALETTE_CELL_SIZE;
        drawBitIcon16(tic, offsetX - 1, offsetY - 1, Icon, tic_color_12);
    }

    {
        static const u8 Icon[] = 
        {
            0b01000000,
            0b11111111,
            0b00000000,
            0b00000010,
            0b11111111,
            0b00000000,
            0b00010000,
            0b11111111,
        };

        tic_rect rect = {x + PALETTE_WIDTH + 3, y + (PALETTE_HEIGHT-8)/2-1, 8, 8};

        bool down = false;
        bool over = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            showTooltip("EDIT PALETTE");

            if(checkMouseDown(&rect, tic_mouse_left))
                down = true;

            if(checkMouseClick(&rect, tic_mouse_left))
                sprite->editPalette = !sprite->editPalette;
        }

        if(sprite->editPalette || down)
        {
            drawBitIcon(rect.x, rect.y+1, Icon, (over ? tic_color_13 : tic_color_12));
        }
        else
        {
            drawBitIcon(rect.x, rect.y+1, Icon, tic_color_0);
            drawBitIcon(rect.x, rect.y, Icon, (over ? tic_color_13 : tic_color_12));            
        }
    }
}

static void drawPalette(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    for(s32 row = 0, i = 0; row < PALETTE_ROWS; row++)
        for(s32 col = 0; col < PALETTE_COLS; col++)
            tic_api_rect(tic, x + col * PALETTE_CELL_SIZE, y + row * PALETTE_CELL_SIZE, PALETTE_CELL_SIZE-1, PALETTE_CELL_SIZE-1, i++);
}

static void selectSprite(Sprite* sprite, s32 x, s32 y)
{
    {
        s32 size = TIC_SPRITESHEET_SIZE - sprite->size;     
        if(x < 0) x = 0;
        if(y < 0) y = 0;
        if(x > size) x = size;
        if(y > size) y = size;
    }

    x /= TIC_SPRITESIZE;
    y /= TIC_SPRITESIZE;

    //sprite->index -= sprite->index % TIC_BANK_SPRITES;
    sprite->index -= sprite->index % TIC_PAGE_SPRITES;
    sprite->index += x + y * SHEET_COLS;

    clearCanvasSelection(sprite);
}

static void updateSpriteSize(Sprite* sprite, s32 size)
{
    if(size != sprite->size)
    {
        sprite->size = size;
        selectSprite(sprite, getIndexPosX(sprite), getIndexPosY(sprite));
    }
}

static void drawSheetOvr(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    tic_rect rect = {x, y, TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE};
	//tic_api_rect(tic, TIC80_WIDTH - TIC_SPRITESHEET_SIZE, ToolbarH, TIC_SPRITESHEET_SIZE, TIC80_HEIGHT - ToolbarH,tic_color_14);
    tic_api_rectb(tic, rect.x - 1, rect.y - 1, rect.w + 2, rect.h + 2, tic_color_12);

    if(checkMousePos(&rect))
    {
        setCursor(tic_cursor_hand);

        if(checkMouseDown(&rect, tic_mouse_left))
        {
            s32 offset = (sprite->size - TIC_SPRITESIZE) / 2;
            selectSprite(sprite, getMouseX() - x - offset, getMouseY() - y - offset);
        }
    }

    s32 bx = getIndexPosX(sprite) + x - 1;
    s32 by = getIndexPosY(sprite) + y - 1;

    tic_api_rectb(tic, bx, by, sprite->size + 2, sprite->size + 2, tic_color_12);
}

static void drawSheet(Sprite* sprite, s32 x, s32 y)
{
    tic_mem* tic = sprite->tic;

    tic_rect rect = {x, y, TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE};

    for(s32 j = 0, index = (sprite->index - sprite->index % TIC_PAGE_SPRITES); j < rect.h; j += TIC_SPRITESIZE)
        for(s32 i = 0; i < rect.w; i += TIC_SPRITESIZE, index++)
            tic_api_spr(tic, sprite->src, index, x + i, y + j, 1, 1, NULL, 0, 1, tic_no_flip, tic_no_rotate);
}

static void flipSpriteHorz(Sprite* sprite)
{
    tic_rect rect = getSpriteRect(sprite);
    s32 r = rect.x + rect.w/2;
    s32 b = rect.y + rect.h;

    for(s32 y = rect.y; y < b; y++)
        for(s32 x = rect.x, i = rect.x + rect.w - 1; x < r; x++, i--)
        {
            u8 color = getSheetPixel(sprite, x, y);
            setSheetPixel(sprite, x, y, getSheetPixel(sprite, i, y));
            setSheetPixel(sprite, i, y, color);
        }

    history_add(sprite->history);
}

static void flipSpriteVert(Sprite* sprite)
{
    tic_rect rect = getSpriteRect(sprite);
    s32 r = rect.x + rect.w;
    s32 b = rect.y + rect.h/2;

    for(s32 y = rect.y, i = rect.y + rect.h - 1; y < b; y++, i--)
        for(s32 x = rect.x; x < r; x++)
        {
            u8 color = getSheetPixel(sprite, x, y);
            setSheetPixel(sprite, x, y, getSheetPixel(sprite, x, i));
            setSheetPixel(sprite, x, i, color);
        }

    history_add(sprite->history);
}

static void rotateSprite(Sprite* sprite)
{
    const s32 Size = sprite->size;
    u8* buffer = (u8*)malloc(Size * Size);

    if(buffer)
    {
        {
            tic_rect rect = getSpriteRect(sprite);
            s32 r = rect.x + rect.w;
            s32 b = rect.y + rect.h;

            for(s32 y = rect.y, i = 0; y < b; y++)
                for(s32 x = rect.x; x < r; x++)
                    buffer[i++] = getSheetPixel(sprite, x, y);

            for(s32 y = rect.y, j = 0; y < b; y++, j++)
                for(s32 x = rect.x, i = 0; x < r; x++, i++)
                    setSheetPixel(sprite, x, y, buffer[j + (Size-i-1)*Size]);

            history_add(sprite->history);
        }

        free(buffer);
    }
}

static void deleteSprite(Sprite* sprite)
{
    tic_rect rect = getSpriteRect(sprite);
    s32 r = rect.x + rect.w;
    s32 b = rect.y + rect.h;

    for(s32 y = rect.y; y < b; y++)
        for(s32 x = rect.x; x < r; x++)
            setSheetPixel(sprite, x, y, sprite->color2);

    clearCanvasSelection(sprite);

    history_add(sprite->history);
}

static void(* const SpriteToolsFunc[])(Sprite*) = {flipSpriteHorz, flipSpriteVert, rotateSprite, deleteSprite};
static void(* const CanvasToolsFunc[])(Sprite*) = {flipCanvasHorz, flipCanvasVert, rotateCanvas, deleteCanvas};

static void drawSpriteTools(Sprite* sprite, s32 x, s32 y)
{
    static const u8 Icons[] =
    {
        0b11101110,
        0b11010110,
        0b11101110,
        0b11101110,
        0b11101110,
        0b11010110,
        0b11101110,
        0b00000000,

        0b11111110,
        0b11111110,
        0b10111010,
        0b01000100,
        0b10111010,
        0b11111110,
        0b11111110,
        0b00000000,

        0b00111000,
        0b01000100,
        0b10010101,
        0b10001110,
        0b10000100,
        0b01000000,
        0b00111000,
        0b00000000,

        0b00111110,
        0b01111111,
        0b00101010,
        0b00101010,
        0b00101010,
        0b00101010,
        0b00111110,
        0b00000000,
    };
    static const char* Tooltips[] = {"FLIP HORZ [5]", "FLIP VERT [6]", "ROTATE [7]", "ERASE [8]"};

    enum{Gap = TIC_SPRITESIZE + 3};

    for(s32 i = 0; i < COUNT_OF(Icons)/BITS_IN_BYTE; i++)
    {
        bool pushed = false;
        bool over = false;
        
        tic_rect rect = {x + i * Gap, y, TIC_SPRITESIZE, TIC_SPRITESIZE};

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            over = true;

            showTooltip(Tooltips[i]);

            if(checkMouseDown(&rect, tic_mouse_left)) pushed = true;

            if(checkMouseClick(&rect, tic_mouse_left))
            {       
                if(hasCanvasSelection(sprite))
                {
                    CanvasToolsFunc[i](sprite);
                }
                else
                {
                    SpriteToolsFunc[i](sprite);
                    clearCanvasSelection(sprite);
                }
            }
        }

        if(pushed)
        {
            drawBitIcon(rect.x, y + 1, Icons + i*BITS_IN_BYTE, (over ? tic_color_13 : tic_color_12));
        }
        else
        {
            drawBitIcon(rect.x, y+1, Icons + i*BITS_IN_BYTE, tic_color_0);
            drawBitIcon(rect.x, y, Icons + i*BITS_IN_BYTE, (over ? tic_color_13 : tic_color_12));
        }
    }
}

static void drawTools(Sprite* sprite, s32 x, s32 y)
{
    static const u8 Icons[] = 
    {
        0b00001000,
        0b00011100,
        0b00111110,
        0b01111100,
        0b10111000,
        0b10010000,
        0b11100000,
        0b00000000,

        0b00111000,
        0b00111000,
        0b01111100,
        0b00101000,
        0b00101000,
        0b00101000,
        0b00010000,
        0b00000000,

        0b10101010,
        0b00000000,
        0b10000010,
        0b00000000,
        0b10000010,
        0b00000000,
        0b10101010,
        0b00000000,

        0b00001000,
        0b00000100,
        0b00000010,
        0b01111111,
        0b10111110,
        0b10011100,
        0b10001000,
        0b00000000,
    };

    enum{Gap = TIC_SPRITESIZE + 3};

    for(s32 i = 0; i < COUNT_OF(Icons)/BITS_IN_BYTE; i++)
    {
        tic_rect rect = {x + i * Gap, y, TIC_SPRITESIZE, TIC_SPRITESIZE};

        bool over = false;
        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);
            over = true;

            static const char* Tooltips[] = {"BRUSH [1]", "COLOR PICKER [2]", "SELECT [3]", "FILL [4]"};

            showTooltip(Tooltips[i]);

            if(checkMouseClick(&rect, tic_mouse_left))
            {               
                sprite->mode = i;

                clearCanvasSelection(sprite);
            }
        }

        bool pushed = i == sprite->mode;

        if(pushed)
        {
            static const u8 Icon[] = 
            {
                0b01111100,
                0b00111000,
                0b00010000,
                0b00000000,
                0b00000000,
                0b00000000,
                0b00000000,
                0b00000000,
            };

            drawBitIcon(rect.x, y - 4, Icon, tic_color_0);
            drawBitIcon(rect.x, y - 5, Icon, tic_color_12);

            drawBitIcon(rect.x, y + 1, Icons + i*BITS_IN_BYTE, (over ? tic_color_13 : tic_color_12));
        }
        else
        {
            drawBitIcon(rect.x, y+1, Icons + i*BITS_IN_BYTE, tic_color_0);
            drawBitIcon(rect.x, y, Icons + i*BITS_IN_BYTE, (over ? tic_color_13 : tic_color_12));
        }
    }

    drawSpriteTools(sprite, x + COUNT_OF(Icons)/BITS_IN_BYTE * Gap + 1, y);
}

static void copyToClipboard(Sprite* sprite)
{
    s32 size = sprite->size * sprite->size * TIC_PALETTE_BPP / BITS_IN_BYTE;
    u8* buffer = malloc(size);

    if(buffer)
    {
        tic_rect rect = getSpriteRect(sprite);
        s32 r = rect.x + rect.w;
        s32 b = rect.y + rect.h;

        for(s32 y = rect.y, i = 0; y < b; y++)
            for(s32 x = rect.x; x < r; x++)
                tic_tool_poke4(buffer, i++, getSheetPixel(sprite, x, y) & 0xf);

        toClipboard(buffer, size, true);

        free(buffer);
    }
}

static void cutToClipboard(Sprite* sprite)
{
    copyToClipboard(sprite);
    deleteSprite(sprite);
}

static void copyFromClipboard(Sprite* sprite)
{
    if(sprite->editPalette)
        pasteColor(sprite);

    s32 size = sprite->size * sprite->size * TIC_PALETTE_BPP / BITS_IN_BYTE;
    u8* buffer = malloc(size);

    if(buffer)
    {
        if(fromClipboard(buffer, size, true, false))
        {
            tic_rect rect = getSpriteRect(sprite);
            s32 r = rect.x + rect.w;
            s32 b = rect.y + rect.h;

            for(s32 y = rect.y, i = 0; y < b; y++)
                for(s32 x = rect.x; x < r; x++)
                    setSheetPixel(sprite, x, y, tic_tool_peek4(buffer, i++));

            history_add(sprite->history);
        }

        free(buffer);

    }
}

static void upSprite(Sprite* sprite)
{
    if(getIndexPosY(sprite) > 0) sprite->index -= SHEET_COLS;
}

static void downSprite(Sprite* sprite)
{
    if(getIndexPosY(sprite) < TIC_SPRITESHEET_SIZE - sprite->size) sprite->index += SHEET_COLS;
}

static void leftSprite(Sprite* sprite)
{
    if(getIndexPosX(sprite) > 0) sprite->index--;
}

static void rightSprite(Sprite* sprite)
{
    if(getIndexPosX(sprite) < TIC_SPRITESHEET_SIZE - sprite->size) sprite->index++;
}

static void undo(Sprite* sprite)
{
    history_undo(sprite->history);
}

static void redo(Sprite* sprite)
{
    history_redo(sprite->history);
}

static void switchBanks(Sprite* sprite)
{
    //bool bg = sprite->index < TIC_BANK_SPRITES;
	u16 idx = sprite->index;
	if (idx < TIC_BANK_SPRITES*TIC_SPRITE_BANKS - TIC_PAGE_SPRITES)
	{
		//sprite->index += bg ? TIC_BANK_SPRITES : -TIC_BANK_SPRITES;
		sprite->index += TIC_PAGE_SPRITES;
	}
	else
	{
		sprite->index = 0;
	}
    
    clearCanvasSelection(sprite);
}

static void processKeyboard(Sprite* sprite)
{
    tic_mem* tic = sprite->tic;

    if(tic->ram.input.keyboard.data == 0) return;

    switch(getClipboardEvent())
    {
    case TIC_CLIPBOARD_CUT: cutToClipboard(sprite); break;
    case TIC_CLIPBOARD_COPY: copyToClipboard(sprite); break;
    case TIC_CLIPBOARD_PASTE: copyFromClipboard(sprite); break;
    default: break;
    }

    bool ctrl = tic_api_key(tic, tic_key_ctrl);

    if(ctrl)
    {   
        if(keyWasPressed(tic_key_z))        undo(sprite);
        else if(keyWasPressed(tic_key_y))   redo(sprite);
    }
    else
    {
        if(hasCanvasSelection(sprite))
        {
            if(!sprite->select.drag)
            {
                if(keyWasPressed(tic_key_up))           upCanvas(sprite);
                else if(keyWasPressed(tic_key_down))    downCanvas(sprite);
                else if(keyWasPressed(tic_key_left))    leftCanvas(sprite);
                else if(keyWasPressed(tic_key_right))   rightCanvas(sprite);
                else if(keyWasPressed(tic_key_delete))  deleteCanvas(sprite);                
            }
        }
        else
        {
            if(keyWasPressed(tic_key_up))           upSprite(sprite);
            else if(keyWasPressed(tic_key_down))    downSprite(sprite);
            else if(keyWasPressed(tic_key_left))    leftSprite(sprite);
            else if(keyWasPressed(tic_key_right))   rightSprite(sprite);
            else if(keyWasPressed(tic_key_delete))  deleteSprite(sprite);
            else if(keyWasPressed(tic_key_tab))     switchBanks(sprite);

            if(!sprite->editPalette)
            {

                if(keyWasPressed(tic_key_1))        sprite->mode = SPRITE_DRAW_MODE;
                else if(keyWasPressed(tic_key_2))   sprite->mode = SPRITE_PICK_MODE;
                else if(keyWasPressed(tic_key_3))   sprite->mode = SPRITE_SELECT_MODE;
                else if(keyWasPressed(tic_key_4))   sprite->mode = SPRITE_FILL_MODE;

                else if(keyWasPressed(tic_key_5))   flipSpriteHorz(sprite);
                else if(keyWasPressed(tic_key_6))   flipSpriteVert(sprite);
                else if(keyWasPressed(tic_key_7))   rotateSprite(sprite);
                else if(keyWasPressed(tic_key_8))   deleteSprite(sprite);

                if(sprite->mode == SPRITE_DRAW_MODE)
                {
                    if(keyWasPressed(tic_key_leftbracket)) {if(sprite->brushSize > 1) sprite->brushSize--;}
                    else if(keyWasPressed(tic_key_rightbracket)) {if(sprite->brushSize < 4) sprite->brushSize++;}
                }               
            }
        }
    }
}

static void drawSpriteToolbar(Sprite* sprite)
{
    tic_mem* tic = sprite->tic;

    tic_api_rect(tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, tic_color_12);

    // draw sprite size control
    {
        tic_rect rect = {TIC80_WIDTH - 58, 1, 23, 5};

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip("CANVAS ZOOM");

            if(checkMouseDown(&rect, tic_mouse_left))
            {
                s32 mx = getMouseX() - rect.x;
                mx /= 6;

                s32 size = 1;
                while(mx--) size <<= 1;

                updateSpriteSize(sprite, size * TIC_SPRITESIZE);
            }
        }

        for(s32 i = 0; i < 4; i++)
            tic_api_rect(tic, rect.x + i*6, 1, 5, 5, tic_color_0);

        tic_api_rect(tic, rect.x, 2, 23, 3, tic_color_0);
        tic_api_rect(tic, rect.x+1, 3, 21, 1, tic_color_12);

        s32 size = sprite->size / TIC_SPRITESIZE, val = 0;
        while(size >>= 1) val++;

        tic_api_rect(tic, rect.x + val*6, 1, 5, 5, tic_color_0);
        tic_api_rect(tic, rect.x+1 + val*6, 2, 3, 3, tic_color_12);
    }

	
	for (s32 i = 0; i <TIC_BANK_SPRITES*2/TIC_PAGE_SPRITES; i++)
	{
		static char Label[] = "SWITCH PAGE";
		tic_rect rect = { SheetX - 1 + i*8, SheetY-9, 7,7 };
		bool over = false;
		if (checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);
			over = true;
			showTooltip("SWITCH PAGE [Tab]");
			//tic_api_print(tic, Label + (char[]) {i,'/0'}, 20, 0, tic_color_12, false, 1, false);
			if (checkMouseClick(&rect, tic_mouse_left))
			{
				sprite->index = TIC_PAGE_SPRITES * i;
			}
		}
		tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_15);
		tic_api_rect(tic, rect.x+1, rect.y+1, rect.w-2, rect.h-2, i == sprite->index / TIC_PAGE_SPRITES ? tic_color_12 : over ? tic_color_2: tic_color_15);
	}
	
	
	//bool bg = sprite->index < TIC_SPRITESHEET_SIZE % TIC_PAGE_SPRITES;//TIC_BANK_SPRITES;
	u16 idx = sprite->index;
    {
		static const char Label[] = "<<";// "BG";
        tic_rect rect = {TIC80_WIDTH - 4 * TIC_FONT_WIDTH - 4, 0, 2 * TIC_FONT_WIDTH + 1, TIC_SPRITESIZE-1};
        //tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, bg ? tic_color_0 : tic_color_14);
        tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_14);
        tic_api_print(tic, Label, rect.x+1, rect.y+1, tic_color_12, true, 1, false);

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip("PREVIOUS PAGE");

            if((idx > TIC_PAGE_SPRITES) && checkMouseClick(&rect, tic_mouse_left))
            {
				sprite->index -= TIC_PAGE_SPRITES;//TIC_BANK_SPRITES;
                clearCanvasSelection(sprite);
            }
        }
    }

    {
		static const char Label[] = ">>";//"FG";
        tic_rect rect = {TIC80_WIDTH - 2 * TIC_FONT_WIDTH - 2, 0, 2 * TIC_FONT_WIDTH + 1, TIC_SPRITESIZE-1};
        //tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, bg ? tic_color_14 : tic_color_0);
        tic_api_rect(tic, rect.x, rect.y, rect.w, rect.h, tic_color_14);
        tic_api_print(tic, Label, rect.x+1, rect.y+1, tic_color_12, true, 1, false);

        if(checkMousePos(&rect))
        {
            setCursor(tic_cursor_hand);

            showTooltip("NEXT PAGE");

            if((idx < (TIC_BANK_SPRITES*TIC_SPRITE_BANKS-TIC_PAGE_SPRITES))&& checkMouseClick(&rect, tic_mouse_left))
            {
				sprite->index += TIC_PAGE_SPRITES;//TIC_BANK_SPRITES;
                clearCanvasSelection(sprite);
            }
        }
    }
}

static void tick(Sprite* sprite)
{
    tic_mem* tic = sprite->tic;

    // process scroll
    {
        tic80_input* input = &tic->ram.input;

        if(input->mouse.scrolly)
        {
            s32 size = sprite->size;
            s32 delta = input->mouse.scrolly;

            if(delta > 0) 
            {
                if(size < (TIC_SPRITESIZE * TIC_SPRITESIZE)) size <<= 1;                    
            }
            else if(size > TIC_SPRITESIZE) size >>= 1;

            updateSpriteSize(sprite, size); 
        }
    }

    processKeyboard(sprite);

    drawCanvas(sprite, CanvasX, CanvasY);
    drawPalette(sprite, PaletteX, PaletteY);
    drawSheet(sprite, SheetX, SheetY);

    sprite->tickCounter++;
}

static void onStudioEvent(Sprite* sprite, StudioEvent event)
{
    switch(event)
    {
    case TIC_TOOLBAR_CUT: cutToClipboard(sprite); break;
    case TIC_TOOLBAR_COPY: copyToClipboard(sprite); break;
    case TIC_TOOLBAR_PASTE: copyFromClipboard(sprite); break;
    case TIC_TOOLBAR_UNDO: undo(sprite); break;
    case TIC_TOOLBAR_REDO: redo(sprite); break;
    }
}

static void scanline(tic_mem* tic, s32 row, void* data)
{
    if(row == 0)
        memcpy(&tic->ram.vram.palette, getBankPalette(), sizeof(tic_palette));
}

static void overline(tic_mem* tic, void* data)	//UI, empty spaces fillers between editors
{
    static const tic_rect bg[] = 
    {
        {0, ToolbarH, SheetX, CanvasY-ToolbarH},
        {0, CanvasY, CanvasX, CanvasH},
        {CanvasX + CanvasW, CanvasY, SheetX - (CanvasX + CanvasW), CanvasH},

        {0, CanvasY + CanvasH, SheetX, PaletteY - CanvasY - CanvasH},

        {0, PaletteY, PaletteX, PaletteH},
        {PaletteX + PaletteW, PaletteY, SheetX - PaletteX - PaletteW, PaletteH},

        {0, PaletteY + PaletteH, SheetX, TIC80_HEIGHT - PaletteY - PaletteH},
		
		{SheetX,0, SheetW+1, SheetY},//filler for spritesheet top
		{SheetX,SheetY+TIC_SPRITESHEET_SIZE, SheetW+1, TIC80_HEIGHT-SheetH-SheetY} //filler for spritesheet bottom
    };

    Sprite* sprite = (Sprite*)data;

    for(const tic_rect* r = bg; r < bg + COUNT_OF(bg); r++)
        tic_api_rect(tic, r->x, r->y, r->w, r->h, tic_color_14);

    drawCanvasOvr(sprite, 24, 20);
    drawMoveButtons(sprite);
    drawFlags(sprite, 24+64+7, 20+8);

    sprite->editPalette 
        ? drawRGBSliders(sprite, 24, 91) 
        : drawTools(sprite, 12, 96);

    drawPaletteOvr(sprite, 24, 112);
    drawSheetOvr(sprite, SheetX,SheetY);
    
    drawSpriteToolbar(sprite);
    drawToolbar(tic, false);
}

void initSprite(Sprite* sprite, tic_mem* tic, tic_tiles* src)
{
    if(sprite->select.back == NULL) sprite->select.back = (u8*)malloc(CANVAS_SIZE*CANVAS_SIZE);
    if(sprite->select.front == NULL) sprite->select.front = (u8*)malloc(CANVAS_SIZE*CANVAS_SIZE);
    if(sprite->history) history_delete(sprite->history);
    
    *sprite = (Sprite)
    {
        .tic = tic,
        .tick = tick,
        .tickCounter = 0,
        .src = src,
        .index = 1,
        .color = 2,
        .color2 = 0,
        .size = TIC_SPRITESIZE,
        .editPalette = false,
        .brushSize = 1,
        .select = 
        {
            .rect = {0,0,0,0},
            .start = {0,0},
            .drag = false,
            .back = sprite->select.back,
            .front = sprite->select.front,
        },
        .mode = SPRITE_DRAW_MODE,
        .history = history_create(src, TIC_SPRITES * sizeof(tic_tile)),
        .event = onStudioEvent,
        .overline = overline,
        .scanline = scanline,
    };
}

void freeSprite(Sprite* sprite)
{
    free(sprite->select.back);
    free(sprite->select.front);
    history_delete(sprite->history);
    free(sprite);
}
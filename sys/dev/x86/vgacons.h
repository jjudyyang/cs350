/*
 * Copyright (c) 2006-2008 Ali Mashtizadeh
 */

#ifndef __VGA_H__
#define __VGA_H__

/*
 * Using 4 80x60 screens with an 8x8 font
 */
#define SCREEN_CONSOLE  1
#define SCREEN_KLOG     2
#define SCREEN_KDEBUG   3
#define SCREEN_EXTRA    4
#define MAX_SCREENS     4

#define COLOR_BLACK         0
#define COLOR_BLUE          1
#define COLOR_DARKGRAY      2
#define COLOR_LIGHTBLUE     3
#define COLOR_RED           4
#define COLOR_MAGENTA       5
#define COLOR_LIGHTRED      6
#define COLOR_LIGHTMAGENTA  7
#define COLOR_GREEN         8
#define COLOR_CYAN          9
#define COLOR_LIGHTGREEN    10
#define COLOR_LIGHTCYAN     11
#define COLOR_BROWN         12
#define COLOR_LIGHTGREY     13
#define COLOR_YELLOW        14
#define COLOR_WHITE         15

void VGA_Init(void);
void VGA_LateInit(void);
void VGA_SwitchTo(int screen);
void VGA_LoadFont(void *fontBuffer, int maxLength);
void VGA_SaveFont(void *fontBuffer, int maxLength);
void VGA_ClearDisplay(void);
void VGA_ScollDisplay(void);
void VGA_Putc(short ch);
void VGA_Puts(const char *str);
void Panic(const char *str);

#endif /* __VGA_H__ */


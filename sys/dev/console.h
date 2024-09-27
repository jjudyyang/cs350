
#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <sys/spinlock.h>

#define KEY_F1		0xF1
#define KEY_F2		0xF2
#define KEY_F3		0XF3
#define KEY_F4		0xF4
#define KEY_F5		0xF5
#define KEY_F6		0xF6
#define KEY_F7		0xF7
#define KEY_F8		0xF8
#define KEY_F9		0xF9
#define KEY_F10		0xFA
#define KEY_F11		0xFB
#define KEY_F12		0XFC

#define CONSOLE_KEYBUF_MAXLEN   256

typedef struct Console {
    // Keyboard Buffer
    int nextKey;
    int lastKey;
    char keyBuf[CONSOLE_KEYBUF_MAXLEN];
    Spinlock keyLock;
} Console;

void Console_Init();
char Console_Getc();
void Console_Gets(char *str, size_t n);
void Console_Putc(char ch);
void Console_Puts(const char *str);

void Console_EnqueueKey(char key);

#endif /* __CONSOLE_H__ */


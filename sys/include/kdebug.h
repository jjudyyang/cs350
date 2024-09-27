
#ifndef __KDEBUG_H__
#define __KDEBUG_H__

typedef struct DebugCommand {
    const char name[40];
    const char description[80];
    void (*func)(int, const char **);
} DebugCommand;

#define REGISTER_DBGCMD(_NAME, _DESC, _FUNC) \
    __attribute__((section(".kdbgcmd"))) \
    DebugCommand cmd_##_NAME = { #_NAME, _DESC, _FUNC }

void Debug_PrintHex(const char *data, size_t length, off_t off, size_t limit);

// Platform Functions
uintptr_t db_disasm(uintptr_t loc, bool altfmt);

// Generic Functions
void Debug_Prompt();

// Helper Functions
uint64_t Debug_GetValue(uintptr_t addr, int size, bool isSigned);
void Debug_PrintSymbol(uintptr_t off, int strategy);
uint64_t Debug_StrToInt(const char *s);
uint64_t Debug_SymbolToInt(const char *s);

#endif /* __KDEBUG_H__ */


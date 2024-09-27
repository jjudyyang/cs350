
void Serial_Init(void);
void Serial_LateInit(void);
void Serial_Interrupt(void *arg);
bool Serial_HasData();
char Serial_Getc();
void Serial_Putc(char ch);
void Serial_Puts(const char *str);


/* ui.h — friendly terminal output and prompts */
#ifndef UI_H
#define UI_H
#include <stdint.h>
#include <stddef.h>

void ui_banner(void);
void ui_ok(const char *fmt, ...);      /* green  ✔ message */
void ui_err(const char *fmt, ...);     /* red    ✘ message */
void ui_warn(const char *fmt, ...);    /* yellow ! message */
void ui_info(const char *fmt, ...);    /* cyan   • message */

/* Receiver-side question. Returns 0 = reject, 1 = accept, 2 = accept and pair (always trust) */
int  ui_ask_accept(const char *sender, const char *ip, const char *label,
                   uint32_t nfiles, uint64_t total, int pin_protected, int encrypted);
/* Asks for a PIN without echoing it. Returns 0 on success, -1 if nothing was entered. */
int  ui_ask_pin(char *buf, size_t cap);
/* Reads a line from stdin (prompt shown first). Returns 0 on success. */
int  ui_ask_line(const char *prompt, char *buf, size_t cap);
#endif

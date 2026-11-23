#include "common.h"
#include "util.h"
#include "ui.h"
#include <poll.h>
#include <termios.h>

static pthread_mutex_t ask_lock = PTHREAD_MUTEX_INITIALIZER;

void ui_banner(void)
{
    us_print("%s", C_CYN);
    us_print("  _   _       _ ____                   \n");
    us_print(" | | | |_ __ (_) ___| _   _ _ __   ___ \n");
    us_print(" | | | | '_ \\| \\___ \\| | | | '_ \\ / __|\n");
    us_print(" | |_| | | | | |___) | |_| | | | | (__ \n");
    us_print("  \\___/|_| |_|_|____/ \\__, |_| |_|\\___|\n");
    us_print("                      |___/ %s\n", C_RST);
    us_print("  %sSame network, same vibe, no cloud needed.  v%s%s\n\n", C_DIM, UNISYNC_VERSION_STR, C_RST);
}

static void say(const char *color, const char *sym, const char *fmt, va_list ap)
{
    char buf[1800]; vsnprintf(buf, sizeof buf, fmt, ap);
    ui_line("%s%s%s %s", color, sym, C_RST, buf);
}
void ui_ok(const char *fmt, ...)   { va_list ap; va_start(ap, fmt); say(C_GRN, "\xe2\x9c\x94", fmt, ap); va_end(ap); }
void ui_err(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); say(C_RED, "\xe2\x9c\x98", fmt, ap); va_end(ap); }
void ui_warn(const char *fmt, ...) { va_list ap; va_start(ap, fmt); say(C_YEL, "!", fmt, ap); va_end(ap); }
void ui_info(const char *fmt, ...) { va_list ap; va_start(ap, fmt); say(C_CYN, "\xe2\x80\xa2", fmt, ap); va_end(ap); }

/* wait up to `secs` for stdin, then read a line */
static int read_line_timeout(char *buf, size_t cap, int secs)
{
    struct pollfd p = { STDIN_FILENO, POLLIN, 0 };
    int r;
    do { r = poll(&p, 1, secs * 1000); } while (r < 0 && errno == EINTR && g_running);
    if (r <= 0) return -1;
    if (!fgets(buf, (int)cap, stdin)) return -1;
    buf[strcspn(buf, "\r\n")] = 0;
    return 0;
}

int ui_ask_line(const char *prompt, char *buf, size_t cap)
{
    us_print("%s", prompt);
    return read_line_timeout(buf, cap, 120);
}

int ui_ask_pin(char *buf, size_t cap)
{
    struct termios old, nw; int tty = isatty(STDIN_FILENO);
    pthread_mutex_lock(&ask_lock);
    us_print("%s\xf0\x9f\x94\x92 This device needs a PIN.%s Enter PIN: ", C_YEL, C_RST);
    if (tty && tcgetattr(STDIN_FILENO, &old) == 0) { nw = old; nw.c_lflag &= ~(tcflag_t)ECHO; tcsetattr(STDIN_FILENO, TCSANOW, &nw); }
    int r = read_line_timeout(buf, cap, 60);
    if (tty) { tcsetattr(STDIN_FILENO, TCSANOW, &old); us_print("\n"); }
    pthread_mutex_unlock(&ask_lock);
    return (r == 0 && buf[0]) ? 0 : -1;
}

int ui_ask_accept(const char *sender, const char *ip, const char *label,
                  uint32_t nfiles, uint64_t total, int pin_protected, int encrypted)
{
    char sz[32], ans[16];
    fmt_size(total, sz, sizeof sz);
    pthread_mutex_lock(&ask_lock);                 /* only ONE thread asks at a time */
    us_print("\n%s  \xe2\x94\x8c\xe2\x94\x80 Incoming transfer \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80%s\n", C_YEL, C_RST);
    us_print("%s  \xe2\x94\x82%s From    : %s%s%s (%s)\n", C_YEL, C_RST, C_BOLD, sender, C_RST, ip);
    us_print("%s  \xe2\x94\x82%s Sending : %s%s%s\n", C_YEL, C_RST, C_BOLD, label, C_RST);
    us_print("%s  \xe2\x94\x82%s Size    : %u file(s), %s\n", C_YEL, C_RST, nfiles, sz);
    us_print("%s  \xe2\x94\x82%s Security: PIN %s   Encryption %s\n", C_YEL, C_RST,
             pin_protected ? "\xe2\x9c\x94" : "off", encrypted ? "\xe2\x9c\x94 (ChaCha20)" : "off");
    us_print("%s  \xe2\x94\x94\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80%s\n", C_YEL, C_RST);
    us_print("  Accept? %s[y]%ses / %s[n]%so / %s[a]%slways trust this device: ", C_GRN, C_RST, C_RED, C_RST, C_CYN, C_RST);
    int res = 0;
    if (read_line_timeout(ans, sizeof ans, ACCEPT_WAIT) == 0) {
        if (ans[0] == 'y' || ans[0] == 'Y') res = 1;
        else if (ans[0] == 'a' || ans[0] == 'A') res = 2;
    } else us_print("\n  (no answer - rejected)\n");
    pthread_mutex_unlock(&ask_lock);
    return res;
}

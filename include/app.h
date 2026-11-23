/* app.h — shared start-up and the four commands behind the UniSync binary */
#ifndef APP_H
#define APP_H
void app_init(void);                       /* logging + history + paired list */
int  run_receive(int argc, char **argv);   /* UniSync receive ...  */
int  run_send(int argc, char **argv);      /* UniSync send ...     */
int  run_discover(void);                   /* UniSync devices      */
#endif

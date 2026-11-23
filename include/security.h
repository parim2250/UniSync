/*
 * security.h — device pairing, block list and "resume" trust
 *   paired  devices skip the Accept/Reject question (saved in ~/.unisync/paired.txt)
 *   blocked devices are always refused           (saved in ~/.unisync/blocked.txt)
 */
#ifndef SECURITY_H
#define SECURITY_H

void security_init(void);                       /* load lists from disk */
int  is_paired(const char *name);
int  is_blocked(const char *name);
void pair_device(const char *name);
void unpair_device(const char *name);
void block_device(const char *name);
void unblock_device(const char *name);
void security_print(void);                      /* show paired + blocked devices */

/* A sender that was interrupted may reconnect to resume without being asked again (10 min). */
void remember_resume(const char *sender, const char *label);
int  resume_match(const char *sender, const char *label);

#endif

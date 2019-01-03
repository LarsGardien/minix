#ifndef _SS_PROTO_H
#define _SS_PROTO_H

/* Function prototypes. */

/* main.c */
int main(int argc, char **argv);

/* sync.c */
int do_add_alphabet(message *m_ptr);
int do_update_sensitivity(message *m_ptr);
int do_synchronize_action(message *m_ptr);
int sef_cb_init_fresh(int type, sef_init_info_t *info);

#endif

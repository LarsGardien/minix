#ifndef _SS_PROTO_H
#define _SS_PROTO_H

/* Function prototypes. */

/* main.c */
int main(int argc, char **argv);

/* sync.c */
int do_add_alphabet(message *m_ptr);
int do_update_sensitivities(message *m_ptr);
int do_synchronise_transition(message *m_ptr);
int do_delete_process(message *m_ptr);
int do_print_ss();
int sef_cb_init_fresh(int type, sef_init_info_t *info);

#endif

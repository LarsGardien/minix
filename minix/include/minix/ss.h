#ifndef _MINIX_SS_H
#define _MINIX_SS_H

#include <stdbool.h>

int ss_add_alphabet(char *prefix, char *actions, size_t actions_blklen, int nr_actions);
int ss_update_sensitivities(int *sensitivities, int nr_sensitivities);
int ss_synchronise_transition(int transition_index);
int ss_delete_process();
#endif /* _MINIX_SS_H */

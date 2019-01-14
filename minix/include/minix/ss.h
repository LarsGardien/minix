#ifndef _MINIX_SS_H
#define _MINIX_SS_H

#include <stdbool.h>

int ss_add_alphabet(char *prefix, char *action, int *transition_index);
int ss_update_sensitivity(int transition_index, int sensitive);
int ss_synchronise_transition(int transition_index);

#endif /* _MINIX_SS_H */

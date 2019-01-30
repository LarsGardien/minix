#ifndef _DATA_STRUCTURES_H
#define _DATA_STRUCTURES_H

typedef struct FsmTransition
{
    int transition_index;
    int next_state_index;
} FsmTransition;

typedef struct FsmState
{
    int number_of_transitions;
    FsmTransition *transition_list;
} FsmState;

typedef struct FsmProcess
{
    FsmState *state_list;
    int current_state;
    int number_of_states;
	
	char *alphabet_strings;			/*All null-terminated alphabet actions (eg. do1\0do2\0do3\0)*/
	int alphabet_strings_blklen;    /*Length of all null-terminated alphabet actions*/
	int *alphabet_transitions; 		/*Holds the Synchronisation Server transition_indices*/
	int number_of_alphabet; 		/*Total number of actions in alphabet*/
} FsmProcess;

#endif
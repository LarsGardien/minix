#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "fsm_parser.h"
#include "data_structures.h"

static int
fsm_find_or_create_alphabet_index(FsmProcess *process, char *input)
{
	int i;
	size_t input_len;
	
	char *alphabet_string = process->alphabet_strings; /*first string*/
	if(NULL != alphabet_string){
		for(i = 0; i < process->number_of_alphabet; ++i)
		{	/*check if input is already in alphabet*/
			if(strcmp(alphabet_string, input) == 0){
				return i; /*return Fsm index*/
			}
			
			while(*alphabet_string++ != '\0'); /*skip to next string*/
		}
	}
	/*Add to alphabet. first  reallocate, then copy*/
	input_len = strlen(input);
	process->alphabet_strings_blklen += input_len+1;
	process->alphabet_strings = realloc(process->alphabet_strings, process->alphabet_strings_blklen);
	if(NULL == process->alphabet_strings){
		fprintf(stderr, "Alphabet reallocation failed.\n");
		return -1;
	}
	
	memcpy(process->alphabet_strings + (process->alphabet_strings_blklen - (input_len+1)), input, input_len+1);
	
	return process->number_of_alphabet++; /*return fsm index*/
}

int
fsm_parse(char *input, FsmProcess *process)
{
    FsmTransition *transition_write;
    int i;
    int number_of_transitions = 0;

    int parsed_state_index = 0;
    int parsed_next_state_index = 0;
    int parsed_string_index = 0;
	char *parsed_string = NULL;
	
    char *substr = NULL;
    if(NULL == (substr = strchr(input, ','))){ /*first line first comma*/
        fprintf(stderr, "fsm_parse: Invalid FSM input: number of transitions not found.\n");
        return -1;
    }
    substr += 1; /*skip comma*/
    number_of_transitions = atoi(substr); /*parse int*/

    if(NULL == (substr = strchr(substr, ','))){ /*first line second comma*/
        fprintf(stderr, "fsm_parse: Invalid FSM input: number of states not found.\n");
        return -1;
    }
    substr += 1; /*skip comma*/
    process->number_of_states = atoi(substr); /*parse int*/

	/*Allocate for states*/
    process->state_list = (FsmState *) calloc(process->number_of_states, sizeof(FsmState));
    if(NULL == process->state_list){
        fprintf(stderr, "fsm_parse: Could not allocate enough memory for states.\n");
        return -1;
    }

	/*Allocate for all transitions at once. Divide over states on the fly*/
    transition_write = (FsmTransition *) calloc(number_of_transitions, sizeof(FsmTransition));
    if(NULL == process->state_list){
        fprintf(stderr, "fsm_parse: Could not allocate enough memory for transitions.\n");
        return -1;
    }

    for(i = 0; i < number_of_transitions; ++i){ /*Iterate all transitions in input*/
        if(NULL == (substr = strchr(substr, '('))){ /*read until next line*/
            fprintf(stderr, "fsm_parse: Invalid FSM input: number_of_transitions %d\n", i);
            return -1;
        }
        substr += 1; /*skip '('*/
        parsed_state_index = atoi(substr); /*parse int*/

        if(NULL == (substr = strchr(substr, ','))){ /*first comma*/
            printf("Invalid FSM input: number_of_transitions %d\n", i);
            return -1;
        }
        substr += 1; /*skip comma*/
        parsed_string = substr; /*remember pointer to action string*/

        if(NULL == (substr = strchr(substr, ','))){ /*second comma*/
            fprintf(stderr, "fsm_parse: Invalid FSM input: number_of_transitions %d\n", i);
            return -1;
        }
        *substr = '\0'; /*action string null terminated instead of comma terminated*/
        substr += 1; /*now skip comman / '\0'*/
        parsed_next_state_index = atoi(substr); /*parse int*/

		/*add string to process->alphabet_strings, save alphabet_string index*/
		parsed_string_index = fsm_find_or_create_alphabet_index(process, parsed_string);
		
        transition_write->transition_index = parsed_string_index;
        transition_write->next_state_index = parsed_next_state_index;
        if(NULL == process->state_list[parsed_state_index].transition_list){ /*first in this state*/
            process->state_list[parsed_state_index].transition_list = transition_write;
        }
        process->state_list[parsed_state_index].number_of_transitions += 1;

        transition_write += 1; /*next write should be in next block*/
    }
	
	/*Allocate room for SS transition indices*/
	process->alphabet_transitions = malloc(process->number_of_alphabet * sizeof(int));
	if(NULL == process->alphabet_transitions){
		perror("malloc:");
		return -1;
	}

    return 0;
}

int
fsm_print(const FsmProcess *process)
{
    int i, j;
    FsmState *temp_current_state = NULL;
    FsmTransition *current_transition = NULL;

    for(i = 0; i < process->number_of_states; ++i){ /*iterate states*/
        temp_current_state = &process->state_list[i];
        for(j = 0; j < temp_current_state->number_of_transitions; ++j){ /*iterate state transitions*/
            current_transition = &temp_current_state->transition_list[j];
            printf("(%d, %d, %d)\n", i,
                   current_transition->transition_index,
                   current_transition->next_state_index);
        }
    }
	
	printf("\n\n");
	/*print mapping alphabet_strings index to SS index*/
	for(i = 0; i < process->number_of_alphabet; ++i){
		printf("%d => %d\n", i, process->alphabet_transitions[i]);
	}
	
    return 0;
}

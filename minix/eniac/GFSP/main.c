#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <minix/ipc.h>
#include <minix/endpoint.h>
#include <minix/ss.h>

#include "data_structures.h"
#include "fsm_parser.h"

int init_process(FsmProcess **process, char *file_path, char *prefix);
int change_state(FsmProcess *process, int action_index);
int update_sensitivites(FsmProcess *process);
int run(FsmProcess *process);

int fifo_fd;
char fifo_filename[21];

/*SIG handler performs cleanup upon exit*/
void handle_sigint(int sig) 
{
	int r;
	if(sig == SIGINT || sig == SIGTERM || sig == SIGQUIT) {
		r = ss_delete_process();
		if(r != 0){
			printf("delete_process failed!\n");
		}
		r = close(fifo_fd);
		if(r != 0){
			printf("fd close failed.\n");
		}
		if(fifo_filename[0] != '\0'){
			unlink(fifo_filename);
		}
		exit(0);	
	}
} 

int
main(int argc, char *argv[])
{
	/*Register sig handlers*/
	if(signal(SIGINT, handle_sigint) == SIG_ERR){
		fprintf(stderr, "Failed to add handler for SIGINT\n");
	}
	if(signal(SIGTERM, handle_sigint) == SIG_ERR){
		fprintf(stderr, "Failed to add handler for SIGTERM\n");
	}
	if(signal(SIGQUIT, handle_sigint) == SIG_ERR){
		fprintf(stderr, "Failed to add handler for SIGQUIT\n");
	}
	if(signal(SIGHUP, handle_sigint) == SIG_ERR){
		fprintf(stderr, "Failed to add handler for SIGHUP\n");
	}
	
    FsmProcess *process;

    if(argc < 3)
    {
        fprintf(stderr,"Usage: <application> <AUT file> <prefix>\n");
        return -1;
    }

    if(init_process(&process, argv[1], argv[2]) != 0)
    {
        fprintf(stderr, "main: Could not initialize a process.\n");
        return -1;
    }

    if(run(process) != 0)
    {
        fprintf(stderr, "main: Unrecognized message received.\n");
        return -1;
    }
	handle_sigint(SIGQUIT);
	return 0;
}

int
run(FsmProcess *process)
{
	int transition_index;

	while(1){
		/*Open named pipe*/
		fifo_fd = open(fifo_filename, O_RDONLY);
		if(fifo_fd == -1){
			fprintf(stderr, "fifo open failed\n");
			return -1;
		}
		/*Block on pipe read until SS sends an int that needs to be synchronized.*/
		if (read(fifo_fd, &transition_index, sizeof(transition_index)) != sizeof(transition_index)){
			fprintf(stderr, "fifo read failed\n");
			return -1;
		}
		printf("transition %d synced.\n", transition_index);	
		
		change_state(process, transition_index); /*change process current_state*/
		update_sensitivites(process);			 /*send new sensitivities to SS*/
		
		if(close(fifo_fd) != 0){				/*Close named pipe*/
			fprintf(stderr, "fifo close failed\n");
			return -1;
		}
	}
	return 0;
}

int
init_process(FsmProcess **process, char *file_path, char *prefix)
{
    long file_size;
    char *file_buffer;

    FILE *fp = fopen(file_path, "r"); /*open input file read-only*/
    if(fp == NULL){
        perror("fopen: ");
        return -1;
    }

    fseek(fp, 0L, SEEK_END);    /* Seek to the end of the file */
    file_size = ftell(fp);      /* Determine the amount of bytes until the end of the file */
    rewind(fp);                 /* Go to the start of the file */

    file_buffer = (char *) malloc(file_size + 1); /*Allocate buffer for reading full file*/
    if(file_buffer == NULL){
        perror("calloc: ");
        fclose(fp);
        return -1;
    }

    if(fread(file_buffer, file_size, 1, fp) == 0){ /*read full file into buffer*/
        perror("fread: ");
        fclose(fp);
        free(file_buffer);
        return -1;
    }
    fclose(fp);
	
    *process = calloc(1, sizeof(FsmProcess)); /*Allocate process*/
    if(*process == NULL){
        perror("calloc: ");
        free(file_buffer);
        return -1;
    }
	
    if(fsm_parse(file_buffer, *process) != 0){ /*Fill process using input (fills alphabet, states, transitions)*/
        fprintf(stderr, "init_process: Could not parse .AUT file.\n");	
        goto error_exit;
    }
	
	if(sprintf(fifo_filename, "/tmp/%d.fifo", getpid()) < 0){/*unique filename for new named pipe*/
		fprintf(stderr, "init_process: Could not write filename.\n");
        goto error_exit;
	}
	
	if(mkfifo(fifo_filename, 0666) == -1){ /*Create new named pipe*/
		fprintf(stderr, "mkfifo failed\n");
        goto error_exit;
	}
	
	if(ss_add_alphabet(/*Tell SS our alphabet and named pipe FIFO*/
			prefix,
			(*process)->alphabet_strings,
			(*process)->alphabet_strings_blklen,
			(*process)->number_of_alphabet,
			(*process)->alphabet_transitions,
			fifo_filename) != 0)
	{
		fprintf(stderr, "init_process: Could not add process to alphabet.\n");
        goto error_exit;
	}
		
	if(update_sensitivites(*process) != 0){ /*Tell SS initial state*/
		fprintf(stderr, "init_process: Could not update process sensitivities.\n");
		goto error_exit;
	}
	
    if(fsm_print(*process) != 0){
        fprintf(stderr, "init_process: Could not print fsm.\n");
        goto error_exit;
    }

	free(file_buffer); /*Input is copied into process, no longer need file*/
    return 0;
	
error_exit:
	/*free everything alloced in this function*/
	free(file_buffer);
	free(*process);
	return -1;
}

int
change_state(FsmProcess *process, int transition_index)
{
	int fsm_transition_index = -1;
    FsmState *state = &process->state_list[process->current_state]; /*Get current state*/

	/* translate SS TransitionIndex to GFSP TransitionIndex */
	for(int i = 0; i < process->number_of_alphabet; ++i)
	{
		if(process->alphabet_transitions[i] == transition_index){
			fsm_transition_index = i;
			break;
		}
	}
	if(fsm_transition_index == -1){ /*NOT FOUND!*/
		fprintf(stderr, "Transition %d not found in process alphabet transitions", transition_index);
		return -1;
	}
	
	/*find the current state's transition and set the new state accordingly*/
	for(int i = 0; i < state->number_of_transitions; ++i){
		if(state->transition_list[i].transition_index == fsm_transition_index){
            process->current_state = state->transition_list[i].next_state_index;
		}
	}
	
	return 0;
}

int
update_sensitivites(FsmProcess *process)
{
    FsmState *state = &process->state_list[process->current_state]; /*Get current state*/
	int *sensitivities = calloc(process->number_of_alphabet, sizeof(int));
	
	/*set all sensitivities for the current state*/
	for(int i = 0; i < state->number_of_transitions; ++i){
		sensitivities[state->transition_list[i].transition_index] = 1;
	}
	
	/*Tell SS about the sensitivities*/
	if(ss_update_sensitivities(sensitivities, process->number_of_alphabet)!= 0){
		fprintf(stderr, "update_sensitivites failed.");
		return -1;
	}
	
	free(sensitivities);
	return 0;
}
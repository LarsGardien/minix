#ifndef _SS_SYNC_H_
#define _SS_SYNC_H_

#define SS_EXIST 1
#define SS_INSERT 2

#include <minix/type.h>

typedef struct TransitionStringItem TransitionStringItem;
typedef struct ProcessItem ProcessItem;
typedef struct TransitionItem TransitionItem;
typedef struct SensitivityItem SensitivityItem;

/*TransitionItem is used to map action and prefix
strings to int indices to avoid strcmp.*/
struct TransitionStringItem {
	struct TransitionStringItem *next_item;

	int index;
	char *action;
	char *prefix;
};

struct ProcessItem{
	struct ProcessItem *next_item;
	endpoint_t ep;
	int nr_sensitivities;
	SensitivityItem *process_sensitivities; /*iterate sensitivities for proc*/
};

struct TransitionItem{
	struct TransitionItem *next_item;
	int transition_index;
	SensitivityItem *transition_sensitivities; /*iterate sensitivities for transition*/
} ;

struct SensitivityItem {
  struct SensitivityItem *prev_transition_item;
	struct SensitivityItem *next_transition_item; /*used by sync transition*/
	TransitionItem *transition;
	ProcessItem *process;
  int process_transition_index;
	int sensitive;
};

#endif /* _SS_SYNC_H_ */

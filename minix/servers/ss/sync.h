#ifndef _SS_SYNC_H_
#define _SS_SYNC_H_

#define SS_EXIST 1
#define SS_INSERT 2

#include <minix/type.h>
#include <stdbool.h>

/*TransitionItem is used to map action and prefix
strings to int indices to avoid strcmp.*/
typedef struct TransitionStringItem {
	struct TransitionStringItem *next_item;

	int index;
	char *action;
	char *prefix;
} TransitionStringItem;

typedef struct SensitivityItem {
	struct SensitivityItem *next_proc_item;	/*used by update sensitivity*/
	struct SensitivityItem *next_transition_item; /*used by sync transition*/
	endpoint_t ep;
	int transition_index;
	bool sensitive;
} SensitivityItem;

typedef struct ProcessItem{
	struct ProcessItem *next_item;
	endpoint_t ep;
	SensitivityItem *process_sensitivities; /*iterate sensitivities for proc*/
} ProcessItem;

typedef struct TransitionItem{
	struct TransitionItem *next_item;
	int transition_index;
	SensitivityItem *transition_sensitivities; /*iterate sensitivities for transition*/
} TransitionItem;

#endif /* _SS_SYNC_H_ */

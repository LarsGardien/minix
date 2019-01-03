#ifndef _SS_SYNC_H_
#define _SS_SYNC_H_

/*TransitionItem is used to map action and prefix
strings to int indices to avoid strcmp.*/
typedef struct TransitionItem {
	struct TransitionItem *next_item;

	int index;
	char *action;
	char *prefix;
} TransitionItem;

#endif /* _SS_SYNC_H_ */

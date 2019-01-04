#include "inc.h"

static TransitionItem *reserved_transitions;

/***
*Checks if a given prefix-action combination is already known. if known: sets p:transition_index
*to the known index and returns 0 (get). if not known. Creates a new TransitionItem sets the 
*p:transition_index to the prev + 1 and returns 1 (insert).
*/
static int
get_or_insert_transition(char *prefix, char *action, int *transition_index)
{
  TransitionItem *item = reserved_transitions;
  TransitionItem *prev_item = NULL;

  while(item){
    if(strcmp(item->prefix, prefix) == 0 && strcmp(item->action, action) == 0){
      *transition_index = item->index;
	  return 0;
    }
    prev_item = item;
    item = item->next_item;
  }
	
  TransitionItem *new_item = (TransitionItem *)malloc(sizeof(TransitionItem));
  if(NULL == new_item){
	  panic("SS: TransitionItem malloc failed.\n");
  }
  new_item->next_item = NULL;
  new_item->prefix = prefix;
  new_item->action = action;

  if(!item && !prev_item){ /*empty list*/
	new_item->index = 0;
	reserved_transitions = new_item;
  }
  else if(!item && prev_item){ /*add to tail*/
	new_item->index = prev_item->index + 1;
	prev_item->next_item = new_item;
  }
  *transition_index = new_item->index;
  
  return 1;
}

/***
*System call that adds a single prefix-action combination (aka a transition) to the list
*of known transitions and assigns it an index. When a transition is already known, return
*the known transition index. Use the transition index to refer to the transition in any 
*future correspondence. This way the prefix+action strings only have to be safecopied once.
*TODO: Bulk add alphabet: feasible? performance gain?
*/
int
do_add_alphabet(message *m_ptr)
{
	/*maloc m_ss_action_length*/
	int r, transition_index;
	char *prefix, *action;
	prefix = (char *)malloc(m_ptr->m_ss_req.prefix_strlen);
	if(NULL == prefix){
		panic("SS: prefix malloc failed.\n");
	}
	
	action = (char *)malloc(m_ptr->m_ss_req.action_strlen);
	if(NULL == action){
		panic("SS: action malloc failed.\n");
	}
	
	/* Copy the memory ranges. */
	r = sys_safecopyfrom(m_ptr->m_source, m_ptr->m_ss_req.prefix_grant,
	        0, (vir_bytes) prefix, m_ptr->m_ss_req.prefix_strlen);
	if(r != OK){
		panic("SS: prefix safecopy failed.\n");
	}
	r = sys_safecopyfrom(m_ptr->m_source, m_ptr->m_ss_req.action_grant,
	        0, (vir_bytes) action, m_ptr->m_ss_req.action_strlen);
	if(r != OK){
		panic("SS: prefix safecopy failed.\n");
	}
	
	/*get or insert the transition insert:r=1 get:r=0 error:r<0*/
	r = get_or_insert_transition(prefix, action, &transition_index);
	if(r < 0){
		printf("SS: failed to insert %s.%s", prefix, action);
		return r;
	}
	else if(r == 0){ /*r=0 means transition already existed, preform cleanup*/
		free(prefix);
		free(action);
	}
	m_ptr->m_ss_reply.transition_index = transition_index;
	
	return OK;
}

/**/
int
do_update_sensitivity(message *m_ptr)
{
	
}

int
do_synchronize_action(message *m_ptr)
{
  Transition transition;
  transition.prefix_index = m_ptr->m_ss_req.prefixindex;
  transition.action_index = m_ptr->m_ss_req.actionindex;
}

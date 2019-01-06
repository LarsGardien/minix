#include "inc.h"

static TransitionStringItem *transition_strings;
static ProcessItem *processes;
static TransitionItem *transitions;

/***
*Checks if a given prefix-action combination is already known. if known: sets p:transition_index
*to the known index and returns SS_EXIST. If not known: creates a new TransitionItem sets the
*p:transition_index to the prev + 1 and returns SS_INSERT.
*/
static int
insert_transition_string(char *prefix, char *action, int *transition_index)
{
  TransitionStringItem *item = transition_strings;
  TransitionStringItem *prev_item = NULL;

  while(item){
    if(strcmp(item->prefix, prefix) == 0 && strcmp(item->action, action) == 0){
      *transition_index = item->index;
	     return SS_EXIST;
    }
    prev_item = item;
    item = item->next_item;
  }

  TransitionStringItem *new_item = (TransitionStringItem *)malloc(sizeof(TransitionStringItem));
  if(NULL == new_item){
	  panic("SS: insert_transition_string: TransitionItem malloc failed.\n");
  }
  new_item->next_item = NULL;
  new_item->prefix = prefix;
  new_item->action = action;

  if(!item && !prev_item){ /*empty list*/
	new_item->index = 0;
	transition_strings = new_item;
  }
  else if(!item && prev_item){ /*add to tail*/
	new_item->index = prev_item->index + 1;
	prev_item->next_item = new_item;
  }
  *transition_index = new_item->index;

  return SS_INSERT;
}

/***
*Inserts a SensitivityItem into the process sensitivity list.
*Iterates process list until it finds the process. Then iterates
*the process_sensitivities to check the transition wasn't added
*already. Adds the sensitivity to the linked list.
*/
static int
insert_process_sensitivity(endpoint_t ep, SensitivityItem *sensitivity)
{
  SensitivityItem *sp, *prev_sp;
  ProcessItem *xp = processes;
  /*Iterate processes until ep found.*/
  while(xp && xp->ep != ep && xp = xp->next_item);
  if(!xp){
    printf("SS: insert_process_sensitivity: Could not find ep:%d in processes.\n");
    return -1;
  }

  /*iterate ep's sensitivities until tail found*/
  sp = xp->process_sensitivities;
  while(sp && sp->transition_index != sensitivity->transition_index){
    prev_sp = sp;
    sp = sp->next_proc_item;
  }
  if(sp->transition_index == sensitivity->transition_index){
    printf("SS: insert_process_sensitivity: TransitionIndex already exists.\n");
    return -1;
  }
  if(!prev_sp){ /*insert empty list*/
    xp->process_sensitivities = sensitivity;
  } else if(prev_sp){ /*insert tail*/
    prev_sp->next_proc_item = sensitivity;
  }

  return OK;
}

/***
*Inserts a SensitivityItem into the transition sensitivity list.
*Iterates transition list until it finds the transition. Adds the sensitivity
*to the transition_sensitivities linked list.
*/
static int
insert_transition_sensitivity(SensitivityItem *sensitivity)
{
  SensitivityItem *sp, *prev_sp;
  TransitionItem *tp = transitions;
  /*iterate transitions until correct index found.*/
  while(tp && tp->transition_index != sensitivity->transition_index &&
        tp = tp->next_item);
  if(!tp){
    printf("SS: insert_transition_sensitivity: Could not find transition_index:%d in transitions.\n");
    return -1;
  }

  /*iterate transition_sensitivities until tail*/
  sp = xp->transition_sensitivities;
  while(sp){
    prev_sp = sp;
    sp = sp->next_proc_item;
  }

  if(!prev_sp){ /*insert empty list*/
    xp->transition_sensitivities = sensitivity;
  } else if(prev_sp){ /*insert tail*/
    prev_sp->next_proc_item = sensitivity;
  }

  return OK;
}

/***
*Adds a transition to a process alphabet. First reserves transition string. Might
*be reserved already. Then creates a sensitivity and inserts it into the process
sensitivities and transition sensitivities linked lists.
*/
static int
add_alphabet(endpoint_t proc, char *prefix, char *action, int *transition_index)
{
  /*Check if transition is already reserved*/
  r = insert_transition_string(prefix, action, transition_index);
	if(r < 0){
		printf("SS: failed to insert %s.%s into transition_strings.\n", prefix, action);
		return r;
	}
	else if(r == SS_EXIST){ /*r=0 means transition already existed, preform cleanup*/
		free(prefix);
		free(action);
	}

  /*Create new SensitivityItem*/
  SensitivityItem *new_sensitivity = (SensitivityItem *)malloc(sizeof(SensitivityItem));
  if(NULL == new_sensitivity) {
    panic("SS: SensitivityItem malloc failed.\n");
  }
  new_sensitivity->next_proc_item = NULL;
  new_sensitivity->next_transition_item = NULL;
  new_sensitivity->ep = proc;
  new_sensitivity->transition_index = *transition_index;
  new_sensitivity->sensitive = false;

  /*Insert into correct process_sensitivities*/
  r = insert_process_sensitivity(m_ptr->m_source, new_sensitivity);
  if(r != OK){
    printf("SS: failed to insert %s.%s into process_sensitivities\n", prefix, action);
    return r;
  }

  /*Insert into correct transition_sensitivities*/
  r = insert_transition_sensitivity(m_ptr->m_source, new_sensitivity);
  if(r != OK){
    printf("SS: failed to insert %s.%s into transition_sensitivities\n", prefix, action);
    return r;
  }

	return OK;
}

/***
*updates the transition sensitivity for a process. First searches the process
*in processes. Then searches the process_sensitivities for the corresponding
*transition. Finally it updates the transition sensitivity.
*/
static int
update_sensitivity(endpoint_t ep, int transition_index, bool sensitive)
{
  SensitivityItem *sp, *prev_sp = NULL;
  /*find the process in processses*/
  ProcessItem *xp = processes;
  while(xp && xp->ep != ep && xp = xp->next_item);
  if(!xp){
    printf("SS: update_sensitivity: could not find ep:%d in processes.\n");
    return -1;
  }

  /*find the transition in process_sensitivities.*/
  sp = xp->process_sensitivities;
  while(sp && sp->transition_index != transition_index){
    prev_sp = sp;
    sp = sp->next_proc_item;
  }
  if(!prev_sp){
    printf("SS: update_sensitivity: could not find transition_index:%d for ep:%d.\n",
            transition_index, ep);
    return -1;
  }
  /*update the sensitivity*/
  prev_sp->sensitive = sensitive;
}

/***
*performs a transition if all relavant processes are sensitive. First searches
*transitions. Then checks whether all transition_sensitivities are sensitive for
*the transition. Finalloy, notifies all relevant processes.
*/
static int
synchronize_transition(int transition_index)
{
  SensitivityItem *sp;
  /*Find the correct transition in transitions.*/
  TransitionItem *tp = transitions;
  while(tp && tp->transition_index != transition_index && tp = tp->next_item);
  if(!tp){
    printf("SS: synchronize_transition: could not find transition_index:%d.\n",
      transition_index);
      return -1;
  }

  /*check if all processes are sensitive for this transition.*/
  sp = tp->transition_sensitivities;
  while(sp && sp->sensitive && sp = sp->next_transition_item);
  if(sp){
    printf("SS: synchronize_transition: transition:%d was not sensitive everywhere.\n");
    return -1;
  }

  /*notify all relevant processes.*/
  sp = tp->transition_sensitivities;
  do{
    ipc_send(sp->ep, &m);
  }
  while(sp = sp->next_transition_item);
}

/***
*System call that adds a single prefix-action combination (aka a transition) to the list
*of known transition strings and assigns it an index. When a transition is already known, return
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

  r = add_alphabet(m_ptr->m_source, prefix, action, &transition_index);
  if(r != OK){
    printf("SS: add_alphabet failed.\n");
    return r;
  }

	m_ptr->m_ss_reply.transition_index = transition_index;
  return OK;
}

/***
*System call that updates the supplied sensitivity to the supplied value for the
*calling process.
*/
int
do_update_sensitivity(message *m_ptr)
{
  int r;
  r = update_sensitivity(m_ptr->m_source, m_ptr->m_ss_req.transition_index,
              m_ptr->m_ss_req.sensitive);
  if(r != OK){
    printf("SS: update_sensitivity failed.\n");
    return r;
  }next_transition_item;

  return OK;
}

/***
*System call that executes a single trasition. It only executes the transition
*when all the relevant processes are sensitive. Notifies all relevant processes
*when the transition can be performed.
*/
int
do_synchronize_transition(message *m_ptr)
{
  int r;
  r = synchronize_transition(m_ptr->m_ss_req.transition_index);
  if(r != OK){
    printf("SS: synchronize_transition failed.\n");
    return r;
  }

  return OK;
}

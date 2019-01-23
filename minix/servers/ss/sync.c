#include "inc.h"
#include "sync.h"

int do_add_alphabet(message *m_ptr);
int do_update_sensitivities(message *m_ptr);
int do_synchronise_transition(message *m_ptr);
int do_delete_process(message *m_ptr);
int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *info);

static TransitionStringItem *transition_strings;
static ProcessItem *processes;
static TransitionItem *transitions;
static int waiting_for_update;

/***
*Checks if a given prefix-action combination is already known. if known: sets p:transition_index
*to the known index and returns SS_EXIST. If not known: creates a new TransitionItem sets the
*p:transition_index to the prev + 1 and returns SS_INSERT.
*/
static int
insert_transition_string(char *prefix, char *action, int *transition_index)
{
  TransitionStringItem *item = NULL, *prev_item = NULL;

  for(item = transition_strings; item; prev_item = item, item = item->next_item){
    if(strcmp(item->prefix, prefix) == 0 && strcmp(item->action, action) == 0){
      *transition_index = item->index;
	     return OK;
    }
  }

  printf("Adding %s.%s to stringitems.\n", prefix, action);

  size_t prefix_len = strlen(prefix);
  size_t action_len = strlen(action);
  TransitionStringItem *new_item =
                          malloc(sizeof *new_item + prefix_len+1 + action_len+1);
  if(NULL == new_item){
	  panic("SS: insert_transition_string: TransitionItem malloc failed.\n");
  }
  new_item->next_item = NULL;
  new_item->prefix = (char *)((char *)new_item + sizeof *new_item);
  new_item->action = (char *)((char *)new_item + sizeof *new_item + prefix_len+1);
  memcpy(new_item->prefix, prefix, prefix_len+1); /*copy + '\0'*/
  memcpy(new_item->action, action, action_len+1); /*copy + '\0'*/

  if(!prev_item){ /*empty list*/
    new_item->index = 0;
    transition_strings = new_item;
  }
  else if(prev_item){ /*add to tail*/
    new_item->index = prev_item->index + 1;
    prev_item->next_item = new_item;
  }
  *transition_index = new_item->index;

  return OK;
}

static ProcessItem *
find_or_create_process(endpoint_t ep)
{
  ProcessItem *xp = NULL, *prev_xp = NULL, *new_process = NULL;
  /*Iterate  until ep found.*/
  for(xp = processes; xp && xp->ep != ep; prev_xp = xp, xp = xp->next_item);
  if(!xp){ /*Process not found. Create and add to processes.*/
    new_process = malloc(sizeof *new_process);
    if(NULL == new_process) {
      panic("SS: ProcessItem malloc failed.\n");
    }
    new_process->ep = ep;
    new_process->next_item = NULL;
    new_process->process_sensitivities = NULL;
    if(!prev_xp){ /*insert empty list*/
      processes = new_process;
    } else{ /*insert after prev*/
      prev_xp->next_item = new_process;
    }
    return new_process;
  }

  return xp;
}

static TransitionItem *
find_or_create_transition(int transition_index)
{
  TransitionItem *tp = NULL, *prev_tp = NULL, *new_transition = NULL;
  /*Iterate until index found or end*/
  for(tp = transitions; tp && tp->transition_index != transition_index;
        prev_tp = tp, tp = tp->next_item);
  if(!tp){
    new_transition = malloc(sizeof *new_transition);
    if(NULL == new_transition){
      panic("SS: TransitionItem malloc failed\n");
    }
    new_transition->transition_index = transition_index;
    new_transition->next_item = NULL;
    new_transition->transition_sensitivities = NULL;
    if(!prev_tp){ /*insert empty list*/
      transitions = new_transition;
    } else{ /*insert after prev*/
      prev_tp->next_item = new_transition;
    }
    return new_transition;
  }
  return tp;
}

/***
*Inserts a SensitivityItem into the transition sensitivity list.
*Searches tail and adds the sensitivity to the transition_sensitivities linked list.
*/
static int
insert_transition_sensitivity(TransitionItem *transition, SensitivityItem *sensitivity)
{
  SensitivityItem *sp = NULL, *prev_sp = NULL;

  /*iterate transition_sensitivities until tail*/
  for(sp = transition->transition_sensitivities; sp && sp->process->ep
      != sensitivity->process->ep; prev_sp = sp, sp = sp->next_transition_item);

  if(sp && sp->process->ep == sensitivity->process->ep){
    printf("SS: insert_transition_sensitivity: process already exists.\n");
    return -1;
  }
  if(!prev_sp){ /*insert empty list*/
    transition->transition_sensitivities = sensitivity;
  } else if(prev_sp){ /*insert tail*/
    prev_sp->next_transition_item = sensitivity;
    sensitivity->prev_transition_item = prev_sp;
  }

  return OK;
}

static void
remove_transition_sensitivity(SensitivityItem *sensitivity)
{
  /*If removing the first item in transition_sensitivities, fix pointer*/
  if(sensitivity == sensitivity->transition->transition_sensitivities){
    sensitivity->transition->transition_sensitivities
            = sensitivity->next_transition_item;
  }

  if(sensitivity->prev_transition_item){
    sensitivity->prev_transition_item->next_transition_item =
            sensitivity->next_transition_item;
  }

  if(sensitivity->next_transition_item){
    sensitivity->next_transition_item->prev_transition_item =
            sensitivity->prev_transition_item;
  }
}

/***
*Adds all given transitions to a process alphabet. First reservers all
*SensitivityItems. Then per item: -reserves transition string, Might be reserved
*already -inserts Sensitivity into the process sensitivities and transition
* sensitivities linked lists.
*/
static int
add_alphabet(
  endpoint_t ep,
  char *prefix,
  char *action_strings,
  int nr_actions,
  int *transition_indices,
  char *fifo_filename)
{
  int i = 0, r = 0, transition_index = 0;;
  TransitionItem *transition = NULL;
  ProcessItem *process = NULL;

  /*Allocate mem for all sensitivities*/
  SensitivityItem *sensitivity_items =
                      calloc(nr_actions, sizeof *sensitivity_items);
  if(NULL == sensitivity_items){
    panic("SS: SensitivityItems malloc failed.\n");
  }
  /*Assign the contiguous Sensitivity memory block to correct process.*/
  process = find_or_create_process(ep); /*Assume created*/
  process->nr_sensitivities = nr_actions;
  process->process_sensitivities = sensitivity_items;
  process->waiting_for_update = 0;
  process->fifo_filename = fifo_filename;

  for(i = 0; i < nr_actions; ++i){
    /*Check if transition is already reserved*/
    r = insert_transition_string(prefix, action_strings, &transition_index);
  	if(r != OK){
  		printf("SS: failed to insert %s.%s into transition_strings.\n",
        prefix, action_strings);
  		return r;
  	}
    transition_indices[i] = transition_index; /*return to process.*/
    transition = find_or_create_transition(transition_index);

    sensitivity_items[i].transition = transition;
    sensitivity_items[i].process = process;
    sensitivity_items[i].sensitive = 0;
    /*Insert into correct transition_sensitivities*/
    r = insert_transition_sensitivity(transition, &sensitivity_items[i]);
    if(r != OK){
      printf("SS: failed to insert %s.%s into transition_sensitivities.\n",
        prefix, action_strings);
      return r;
    }

    while(*action_strings++ != '\0'); /*Skip to next string*/
  }

  return OK;
}

/***
*updates the transition sensitivity for a process. First searches the process
*in processes. Then searches the process_sensitivities for the corresponding
*transition. Finally it updates the transition sensitivity.
*/
static int
update_sensitivities(
  endpoint_t ep,
  int *sensitivities,
  int nr_sensitivities)
{
  SensitivityItem *sp = NULL, *prev_sp = NULL;
  ProcessItem *xp = NULL;
  int i = 0;

  /*find the process in processses*/
  for(xp = processes; xp && xp->ep != ep; xp = xp->next_item);
  if(!xp){
    printf("SS: update_sensitivity: could not find ep:%d in processes.\n", ep);
    return -1;
  }

  for(i = 0; i < xp->nr_sensitivities; ++i){
    xp->process_sensitivities[i].sensitive = sensitivities[i];
    if(xp->waiting_for_update){
      xp->waiting_for_update = 0;
      --waiting_for_update;
    }
  }

  return OK;
}

/***
*performs a transition if all relavant processes are sensitive. First searches
*transitions. Then checks whether all transition_sensitivities are sensitive for
*the transition. Finalloy, notifies all relevant processes.
*/
static int
synchronise_transition(int transition_index)
{
  int r, fd;
  message m;
  SensitivityItem *sp = NULL;
  TransitionItem *tp = NULL;

  if(waiting_for_update != 0){
    printf("SS: could not sync, still waiting for %d updates.\n", waiting_for_update);
    return -1;
  }

  /*Find the correct transition in transitions.*/
  for(tp = transitions; tp && tp->transition_index != transition_index;
        tp = tp->next_item);
  if(!tp){
    printf("SS: synchronize_transition: could not find transition_index:%d.\n",
      transition_index);
      return -1;
  }

  /*check if all processes are sensitive for this transition.*/
  for(sp = tp->transition_sensitivities; sp && sp->sensitive;
        sp = sp->next_transition_item);
  if(sp){
    printf("SS: synchronize_transition: transition:%d was not "\
      "sensitive everywhere.\n", transition_index);
    return -1;
  }

  /*notify all relevant processes.*/
  for(sp = tp->transition_sensitivities; sp; sp = sp->next_transition_item){
    sp->process->waiting_for_update = 1;
    ++waiting_for_update;

    fd = open(sp->process->fifo_filename, O_WRONLY);
    if(fd == -1){
      printf("SS: synchronize_transition: failed to open FIFO for %d.\n",
            sp->process->ep);
      return -1;
    }
    r = write(fd, &transition_index, sizeof(transition_index));
    if(r != sizeof(transition_index)){
      printf("SS: synchronize_transition: could not notify %d\n", sp->process->ep);
      return -1;
    }
    r = close(fd);
    if(r != OK){
      printf("SS: synchronize_transition: failed to close FIFO for %d.\n",
            sp->process->ep);
      return -1;
    }

  }
  return OK;
}

static int
delete_process(endpoint_t proc)
{
  ProcessItem *xp = NULL, *prev_xp = NULL;
  int r = 0, i = 0;

  for(xp = processes; xp && xp->ep != proc; prev_xp = xp, xp = xp->next_item);
  if(!xp){
    printf("SS: delete_process: process:%d was not found.\n", proc);
    return -1;
  }

  for(i = 0; i < xp->nr_sensitivities; ++i){
     remove_transition_sensitivity(&xp->process_sensitivities[i]);
  }

  if(prev_xp){
    prev_xp->next_item = xp->next_item;
  }
  else{
    processes = xp->next_item;
  }

  if(xp->waiting_for_update){
    --waiting_for_update;
  }

  free(xp->fifo_filename);
  free(xp->process_sensitivities);
  free(xp);

  return OK;
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
  int *transition_indices;
	char *prefix, *actions, *fifo_filename;
	prefix = malloc(m_ptr->m_ss_add_req.prefix_strlen+1);
	if(NULL == prefix){
		panic("SS: prefix malloc failed.\n");
	}

	actions = malloc(m_ptr->m_ss_add_req.actions_blklen);
	if(NULL == actions){
		panic("SS: actions malloc failed.\n");
	}

  fifo_filename = malloc(m_ptr->m_ss_add_req.fifo_filename_strlen+1);
  if(NULL == fifo_filename){
    panic("SS: fifo_filename malloc failed.\n");
  }

  transition_indices = malloc(
              m_ptr->m_ss_add_req.nr_actions * sizeof *transition_indices);

  r = sys_datacopy(m_ptr->m_source, m_ptr->m_ss_add_req.prefix,
      SELF, (vir_bytes)prefix, m_ptr->m_ss_add_req.prefix_strlen);
	if (r != OK){
		panic("SS: do_add_alphabet: sys_datacopy failed: %d", r);
  }
  r = sys_datacopy(m_ptr->m_source, m_ptr->m_ss_add_req.actions,
      SELF, (vir_bytes)actions, m_ptr->m_ss_add_req.actions_blklen);
	if (r != OK){
		panic("SS: do_add_alphabet: sys_datacopy failed: %d", r);
  }
  prefix[m_ptr->m_ss_add_req.prefix_strlen] = '\0';
  r = sys_datacopy(m_ptr->m_source, m_ptr->m_ss_add_req.fifo_filename,
      SELF, (vir_bytes)fifo_filename, m_ptr->m_ss_add_req.fifo_filename_strlen);
  if(r != OK){
    panic("SS: do_add_alphabet: sys_datacopy failed: %d", r);
  }
  fifo_filename[m_ptr->m_ss_add_req.fifo_filename_strlen] = '\0';

  r = add_alphabet(m_ptr->m_source, prefix, actions,
    m_ptr->m_ss_add_req.nr_actions, transition_indices, fifo_filename);
  if(r != OK){
    printf("SS: add_alphabet failed.\n");
    goto exit;
  }

  r = sys_datacopy(SELF, (vir_bytes)transition_indices, m_ptr->m_source,
    m_ptr->m_ss_add_req.transition_indices, m_ptr->m_ss_add_req.nr_actions
    * sizeof *transition_indices);
  if(r != OK){
    panic("SS: add_alphabet: failed to transfer indices.\n");
  }

exit:
  free(transition_indices);
  free(prefix);
  free(actions);

  return r;
}

/***
*System call that updates the supplied sensitivity to the supplied value for the
*calling process.
*/
int
do_update_sensitivities(message *m_ptr)
{
  int r;
  int *sensitivities = NULL;
  sensitivities = malloc(m_ptr->m_ss_update_req.nr_sensitivities * sizeof(int));
  if(NULL == sensitivities){
    panic("SS: sensitivities malloc failed.\n");
  }

  r = sys_datacopy(m_ptr->m_source, m_ptr->m_ss_update_req.sensitivities,
    		SELF, (vir_bytes)sensitivities,
        m_ptr->m_ss_update_req.nr_sensitivities * sizeof(int));
	if (r != OK){
		panic("SS: do_add_alphabet: sys_datacopy failed: %d", r);
  }

  r = update_sensitivities(m_ptr->m_source, sensitivities,
              m_ptr->m_ss_update_req.nr_sensitivities);
  if(r != OK){
    printf("SS: update_sensitivity failed.\n");
    return r;
  }

  return OK;
}

/***
*System call that executes a single trasition. It only executes the transition
*when all the relevant processes are sensitive. Notifies all relevant processes
*when the transition can be performed.
*/
int
do_synchronise_transition(message *m_ptr)
{
  int r;
  r = synchronise_transition(m_ptr->m_ss_sync_req.transition_index);
  if(r != OK){
    printf("SS: synchronize_transition failed.\n");
    return r;
  }

  return OK;
}

int
do_delete_process(message *m_ptr)
{
  int r;
  r = delete_process(m_ptr->m_source);
  if(r != OK){
    printf("SS: delete_process failed.\n");
    return r;
  }

  return OK;
}

int do_print_ss()
{
  ProcessItem *xp = NULL;
  TransitionItem *tp = NULL;
  SensitivityItem *sp = NULL;
  int i = 0;

  for(xp = processes; xp; xp = xp->next_item){
    printf("process: ep: %d. nr_sens: %d\n", xp->ep, xp->nr_sensitivities);
    for(i = 0; i < xp->nr_sensitivities; ++i){
      printf("%p - %p %d %d %d\n",
          xp->process_sensitivities[i].prev_transition_item,
          xp->process_sensitivities[i].next_transition_item,
          xp->process_sensitivities[i].transition->transition_index,
          xp->process_sensitivities[i].process->ep,
          xp->process_sensitivities[i].sensitive
        );
    }
  }
  printf("\n");

  for(tp = transitions; tp; tp = tp->next_item){
    printf("transition: tidx: %d\n", tp->transition_index);
    for(sp = tp->transition_sensitivities; sp; sp = sp->next_transition_item){
      printf("%p - %p %d %d %d\n",
          sp->prev_transition_item,
          sp->next_transition_item,
          sp->transition->transition_index,
          sp->process->ep,
          sp->sensitive
        );
    }
  }
  return OK;
}


int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *info)
{
	return(OK);
}

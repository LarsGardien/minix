#include "inc.h"

static TransitionItem *reserved_transitions;

static int
get_or_insert_transition(char *prefix, char *action)
{
  TransitionItem *item = reserved_transitions;
  TransitionItem *prev_item = NULL;

  while(item){
    if(strcmp(item->prefix, prefix) == 0 && strcmp(item->action, action) == 0){
      return item->index;
    }
    prev_item = item;
    item = item->next_item;
  }

  if(!item && !prev_item){ /*empty list*/

  }
  else if(){

  }
}

int
do_add_alphabet(message *m_ptr)
{

}

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

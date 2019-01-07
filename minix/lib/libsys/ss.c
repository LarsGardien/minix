#include <string.h>
#include <stdbool.h>

#include "syslib.h"

int
ss_add_alphabet(char *prefix, char *action, int *transition_index)
{
  message m;
  cp_grant_id_t prefix_grant, action_grant;
	size_t prefix_strlen, action_strlen;
	int access, r;

	/* Grant for prefix. */
  prefix_strlen = strlen(prefix);
	prefix_grant = cpf_grant_direct(SS_PROC_NR, (vir_bytes) prefix,
		prefix_strlen, CPF_READ);
	if(!GRANT_VALID(prefix_grant))
		return ENOMEM;

  /* Grant for action. */
  action_strlen = strlen(action);
  action_grant = cpf_grant_direct(SS_PROC_NR, (vir_bytes) action,
    action_strlen, CPF_READ);
  if(!GRANT_VALID(action_grant))
    return ENOMEM;

	m->m_ss_req.prefix_grant = prefix_grant;
  m->m_ss_req.prefix_strlen = prefix_strlen;
	m->m_ds_req.action_grant = action_grant;
  m->m_ds_req.action_strlen = action_strlen;

	r = _taskcall(SS_PROC_NR, SS_ALPHABET, &m);
  if(r == OK){
    *transition_index = m_ss_reply.transition_index;
  }

	cpf_revoke(prefix_grant);
  cpf_revoke(action_grant);
	return r;
}

int
ss_update_sensitivity(int transition_index, bool sensitive)
{
  message m;
  m.m_ss_req.transition_index = transition_index;
  m.m_ss_req.sensitive = sensitive;

	r = _taskcall(SS_PROC_NR, SS_SENSITIVITY, &m);
  return r;
}

int
ss_synchronise_transition(int transition_index)
{
  message m;
  m.m_ss_req.transition_index = transition_index;

	r = _taskcall(SS_PROC_NR, SS_SYNCHRONISE, &m);
  return r;
}

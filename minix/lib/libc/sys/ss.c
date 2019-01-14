#include <string.h>

#include <lib.h>
#include <minix/ss.h>
#include <minix/syslib.h>

#define OK 0

int
ss_add_alphabet(char *prefix, char *action, int *transition_index)
{
  message m;
	size_t prefix_strlen, action_strlen;
	int r;

  prefix_strlen = strlen(prefix);
  action_strlen = strlen(action);

	m.m_ss_req.prefix = (vir_bytes)prefix;
  m.m_ss_req.prefix_strlen = prefix_strlen;
	m.m_ss_req.action = (vir_bytes)action;
  m.m_ss_req.action_strlen = action_strlen;

	r = _syscall(SS_PROC_NR, SS_ALPHABET, &m);
  if(r == OK){
    *transition_index = m.m_ss_reply.transition_index;
  }
	return m.m_type;
}

int
ss_update_sensitivity(int transition_index, int sensitive)
{
  message m;
  int r;
  m.m_ss_req.transition_index = transition_index;
  m.m_ss_req.sensitive = sensitive;

  r = _syscall(SS_PROC_NR, SS_SENSITIVITY, &m);
  return r;
}

int
ss_synchronise_transition(int transition_index)
{
  message m;
  int r;
  m.m_ss_req.transition_index = transition_index;

	r = _syscall(SS_PROC_NR, SS_SYNCHRONISE, &m);
  return r;
}

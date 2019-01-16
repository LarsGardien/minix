#include <string.h>

#include <lib.h>
#include <minix/ss.h>
#include <minix/syslib.h>

#define OK 0

int
ss_add_alphabet(char *prefix, char *actions, size_t actions_blklen, int nr_actions)
{
  message m;
	size_t prefix_strlen;
	int r;

  prefix_strlen = strlen(prefix);

	m.m_ss_add_req.prefix = (vir_bytes)prefix;
  m.m_ss_add_req.prefix_strlen = prefix_strlen;
	m.m_ss_add_req.actions = (vir_bytes)actions;
  m.m_ss_add_req.actions_blklen = actions_blklen;
  m.m_ss_add_req.nr_actions = nr_actions;

	r = _syscall(SS_PROC_NR, SS_ALPHABET, &m);
	return r;
}

int
ss_update_sensitivities(int *sensitivities, int nr_sensitivities)
{
  message m;
  int r;
  m.m_ss_update_req.sensitivities = (vir_bytes) sensitivities;
  m.m_ss_update_req.nr_sensitivities = nr_sensitivities;

  r = _syscall(SS_PROC_NR, SS_SENSITIVITY, &m);
  return r;
}

int
ss_synchronise_transition(int transition_index)
{
  message m;
  int r;
  m.m_ss_sync_req.transition_index = transition_index;

	r = _syscall(SS_PROC_NR, SS_SYNCHRONISE, &m);
  return r;
}

#include <string.h>

#include <lib.h>
#include <minix/ss.h>
#include <minix/syslib.h>

#define OK 0

int
ss_add_alphabet(char *prefix, char *actions, size_t actions_blklen, int nr_actions,
  int *transition_indices, char *fifo_filename)
{
  message m;
	size_t prefix_strlen, fifo_filename_strlen;
	int r;

  prefix_strlen = strlen(prefix);
  fifo_filename_strlen = strlen(fifo_filename);

  memset(&m, 0, sizeof(m));
	m.m_ss_add_req.prefix = (vir_bytes)prefix;
  m.m_ss_add_req.prefix_strlen = prefix_strlen;
	m.m_ss_add_req.actions = (vir_bytes)actions;
  m.m_ss_add_req.actions_blklen = actions_blklen;
  m.m_ss_add_req.nr_actions = nr_actions;
  m.m_ss_add_req.transition_indices = (vir_bytes)transition_indices;
  m.m_ss_add_req.fifo_filename = (vir_bytes)fifo_filename;
  m.m_ss_add_req.fifo_filename_strlen = fifo_filename_strlen;

	r = _syscall(SS_PROC_NR, SS_ALPHABET, &m);
	return r;
}

int
ss_update_sensitivities(int *sensitivities, int nr_sensitivities)
{
  message m;
  int r;
  memset(&m, 0, sizeof(m));
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
  memset(&m, 0, sizeof(m));
  m.m_ss_sync_req.transition_index = transition_index;

	r = _syscall(SS_PROC_NR, SS_SYNCHRONISE, &m);
  return r;
}

int
ss_delete_process()
{
  message m;
  int r;
  memset(&m, 0, sizeof(m));

  r = _syscall(SS_PROC_NR, SS_DELETE, &m);
  return r;
}

/* Synchronisation Server.
 *
 * Created:
 *   Dec 12, 2018	by Lars Gardien
 */

#include "inc.h"	/* include master header file */
#include <minix/endpoint.h>

/* Allocate space for the global variables. */
static endpoint_t who_e;	/* caller's proc number */
static int callnr;		/* system call number */

/* Declare some local functions. */
static void get_work(message *m_ptr);
static void reply(endpoint_t whom, message *m_ptr);

/* SEF functions and variables. */
static void sef_local_startup(void);

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
int main(int argc, char **argv)
{
/* This is the main routine of this service. The main loop consists of
 * three major activities: getting new work, processing the work, and
 * sending the reply. The loop never terminates, unless a panic occurs.
 */
  message m;
  int result;

  /* SEF local startup. */
  env_setargs(argc, argv);
  sef_local_startup();

  /* Main loop - get work and do it, forever. */
  while (TRUE) {

      /* Wait for incoming message, sets 'callnr' and 'who'. */
      get_work(&m);

      if (is_notify(callnr)) {
          printf("SS: warning, got illegal notify from: %d\n", m.m_source);
          result = EINVAL;
          goto send_reply;
      }

      switch (callnr) {
      case SS_ALPHABET:
          result = do_add_alphabet(&m);
          break;
      case SS_SENSITIVITY:
          result = do_update_sensitivity(&m);
          break;
      case SS_SYNCHRONISE:
          result = do_synchronise_transition(&m);
          break;
      case SS_DELETE:
          result = do_delete_process(&m);
          break;
      default:
          printf("SS: warning, got illegal request from %d\n", m.m_source);
          result = EINVAL;
          break;
      }

send_reply:
      /* Finally send reply message, unless disabled. */
      if (result != EDONTREPLY) {
          m.m_type = result;  		/* build reply message */
          reply(who_e, &m);		/* send it away */
      }
  }
  return(OK);				/* shouldn't come here */
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(SEF_CB_INIT_RESTART_STATEFUL);

  /* Register state transfer callbacks. */
  sef_llvm_ds_st_init();

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *				get_work                                     *
 *===========================================================================*/
static void get_work(
  message *m_ptr			/* message buffer */
)
{
    int status = sef_receive(ANY, m_ptr);   /* blocks until message arrives */
    if (OK != status)
        panic("failed to receive message!: %d", status);
    who_e = m_ptr->m_source;        /* message arrived! set sender */
    callnr = m_ptr->m_type;       /* set function call number */
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
static void reply(
  endpoint_t who_e,			/* destination */
  message *m_ptr			/* message buffer */
)
{
    int s = ipc_send(who_e, m_ptr);    /* send the message */
    if (OK != s)
        printf("SS: unable to send reply to %d: %d\n", who_e, s);
}

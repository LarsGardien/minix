/*
 * mux_i2c - software driver for multiplexed I2C interface. Forwards messages
 * to the parent i2c bus driver and stores device reservations.
 */

/* kernel headers */
#include <minix/chardriver.h>
#include <minix/drivers.h>
#include <minix/ds.h>
#include <minix/i2cdriver.h>
#include <minix/i2c.h>
#include <minix/log.h>
#include <minix/type.h>
#include <minix/board.h>

/* system headers */
#include <sys/mman.h>

/* usr headers */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* i2c slave addresses can be up to 10 bits */
#define NR_I2CDEV (0x3ff)

/* local function prototypes */
static int check_reservation(endpoint_t endpt, int slave_addr,
					uint8_t is_mux_reserve);
static void update_reservation(endpoint_t endpt, char *key);
static void ds_event(void);

static int sef_cb_lu_state_save(int UNUSED(result), int UNUSED(flags));

static int do_mux_i2c_reserve(message *m);
static int do_mux_i2c_ioctl_exec(endpoint_t caller, cp_grant_id_t grant_nr,
			uint8_t is_mux_exec);

static int env_parse_muxinstance(void);

/* libchardriver callbacks */
static int mux_i2c_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int flags, endpoint_t user_endpt, cdev_id_t id);
static void mux_i2c_other(message * m, int ipc_status);

static endpoint_t mux_endpoint = NONE;
static endpoint_t parent_bus_endpoint = NONE;
static uint8_t channel = 0;
static uint32_t i2c_bus_id;

/* Table of i2c device reservations. */
static struct i2cdev
{
	uint8_t inuse;
	int mux_inuse; /*Count how often address is reserved by child muxes*/
	endpoint_t endpt;
	char key[DS_MAX_KEYLEN];
} i2cdev[NR_I2CDEV];

/* logging - use with log_warn(), log_info(), log_debug(), log_trace() */
static struct log log = {
	.name = "mux_i2c",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* Entry points to the i2c driver from libchardriver.
 * Only i2c_ioctl() and i2c_other() are implemented. The rest are no-op.
 */
static struct chardriver i2c_tab = {
	.cdr_ioctl	= mux_i2c_ioctl,
	.cdr_other	= mux_i2c_other
};

/*
 * Claim an unclaimed device for exclusive use by endpt. This function can
 * also be used to update the endpt if the endpt's label matches the label
 * already associated with the slave address. This is useful if a driver
 * shuts down unexpectedly and starts up with a new endpt and wants to reserve
 * the same device it reserved before.
 */
static int
do_mux_i2c_reserve(message *m)
{
	int r;
	message m_parent;
	char key[DS_MAX_KEYLEN];
	char label[DS_MAX_KEYLEN];

	int slave_addr = m->m_li2cdriver_i2c_busc_i2c_reserve.addr;

	if (slave_addr < 0 || slave_addr >= NR_I2CDEV) {
		log_warn(&log,
		    "slave address must be positive & no more than 10 bits\n");
		return EINVAL;
	}

	/*Forward as mux reserve*/
	memset(&m_parent, 0, sizeof(m_parent));
	m_parent.m_type = BUSC_I2C_RESERVE;
	m_parent.m_li2cdriver_i2c_busc_i2c_reserve.addr = slave_addr;
	m_parent.m_li2cdriver_i2c_busc_i2c_reserve.mux_reserve = 1;
	printf("mux_i2c: reserve on parent.\n");
	r = ipc_sendrec(parent_bus_endpoint, &m_parent);
	if(r != OK){
		log_warn(&log, "could not reserve address on parent bus\n");
		return EBUSY;
	}
	printf("mux_i2c: reserve parent returned. %d\n", m_parent.m_type);
	printf("mux_i2c: reserve: %d - %d\n", m->m_source, m->m_li2cdriver_i2c_busc_i2c_reserve.mux_reserve);

	if(m->m_li2cdriver_i2c_busc_i2c_reserve.mux_reserve){
		/*Check if device is in use by another driver*/
		if(i2cdev[slave_addr].inuse != 0){
			log_warn(&log, "address in use by '%s'/0x%x\n",
			    i2cdev[slave_addr].key, i2cdev[slave_addr].endpt);
				return EBUSY;
		}
		/*Reserve device for a multiplexed driver*/
		i2cdev[slave_addr].mux_inuse += 1;
	} else {
		/* find the label for the endpoint */
		r = ds_retrieve_label_name(label, m->m_source);
		if (r != OK) {
			log_warn(&log, "Couldn't find label for endpt='0x%x'\n",
					m->m_source);
			return r;
		}
		/* construct the key i2cdriver_announce published (saves an IPC call) */
		snprintf(key, DS_MAX_KEYLEN, "drv.i2c.%d.%s", i2c_bus_id + 1, label);

		/*Check if device is in use by another driver */
		if (i2cdev[slave_addr].inuse != 0
		    && strncmp(i2cdev[slave_addr].key, key, DS_MAX_KEYLEN) != 0) {
			log_warn(&log, "address in use by '%s'/0x%x\n",
			    i2cdev[slave_addr].key, i2cdev[slave_addr].endpt);
			return EBUSY;
		}
		/*Check if device is in use by a multiplexed driver*/
		else if(i2cdev[slave_addr].mux_inuse > 0){
			log_warn(&log, "address in use by %d multiplexed interfaces.\n",
			    i2cdev[slave_addr].mux_inuse);
			return EBUSY;
		}

		/* device is free or already owned by us, claim it */
		i2cdev[slave_addr].inuse = 1;
		i2cdev[slave_addr].endpt = m->m_source;
		memcpy(i2cdev[slave_addr].key, key, DS_MAX_KEYLEN);
	}

	sef_cb_lu_state_save(0, 0);	/* save reservations */

	log_debug(&log, "Device 0x%x claimed by 0x%x key='%s'\n",
	    slave_addr, m->m_source, key);

	return OK;
}

/*
 * All drivers must reserve their device(s) before doing operations on them
 * (read/write, etc). ioctl()'s from VFS (i.e. user programs) can only use
 * devices that haven't been reserved. A driver isn't allowed to access a
 * device that another driver has reserved (not even other instances of the
 * same driver).
 */
static int
check_reservation(endpoint_t endpt, int slave_addr, uint8_t is_mux_reserve)
{
	if (slave_addr < 0 || slave_addr >= NR_I2CDEV) {
		log_debug(&log,
		    "slave address must be positive & no more than 10 bits\n");
		return EINVAL;
	}

	if (endpt == VFS_PROC_NR && i2cdev[slave_addr].inuse == 0) {
		log_debug(&log,
		    "allowing ioctl() from VFS to access unclaimed device\n");
		return OK;
	}
	if(is_mux_reserve && i2cdev[slave_addr].mux_inuse > 0){
		log_debug(&log, "allowing access to multiplexed device\n");
		return OK;
	}	else if (i2cdev[slave_addr].inuse && i2cdev[slave_addr].endpt != endpt) {
		log_warn(&log, "device reserved by another endpoint\n");
		return EBUSY;
	} else if (i2cdev[slave_addr].inuse == 0) {
		log_warn(&log,
		    "all drivers sending messages directly to this driver must reserve\n");
		return EPERM;
	} else {
		log_debug(&log, "allowing access to registered device\n");
		return OK;
	}
}

/*
 * i2c listens to updates from ds about i2c device drivers starting up.
 * When a driver comes back up with the same label, the endpt associated
 * with the reservation needs to be updated. This function does the updating.
 */
static void
update_reservation(endpoint_t endpt, char *key)
{
	int i;
	message m;

	log_debug(&log, "Updating reservation for '%s' endpt=0x%x\n", key,
	    endpt);

	for (i = 0; i < NR_I2CDEV; i++) {

		/* find devices in use that the driver owns */
		if (i2cdev[i].inuse != 0
		    && strncmp(i2cdev[i].key, key, DS_MAX_KEYLEN) == 0) {
			/* update reservation with new endpoint */
			memset(&m, 0, sizeof(m));
			m.m_li2cdriver_i2c_busc_i2c_reserve.addr = i;
			m.m_li2cdriver_i2c_busc_i2c_reserve.mux_reserve = 0;
			m.m_source = endpt;
			do_mux_i2c_reserve(&m);
			log_debug(&log, "Found device to update 0x%x\n", i);
		}
	}
}

static int
do_mux_i2c_ioctl_exec(endpoint_t caller, cp_grant_id_t grant_nr,
					uint8_t is_mux_reserve)
{
	int r;
	message m;
	cp_grant_id_t indir_grant;

	/* permission check */
	/*r = check_reservation(caller, ioctl_exec.iie_addr, is_mux_reserve);
	if (r != OK) {
		log_warn(&log, "check_reservation() denied the request\n");
		goto deselect;
	}*/

	r = i2cdriver_mux_select(mux_endpoint, channel);
	if(r != OK) {
		log_warn(&log, "Failed i2cdriver_mux_select()\n");
		goto deselect;
	}

	indir_grant = cpf_grant_indirect(parent_bus_endpoint, caller, grant_nr);
	if (!GRANT_VALID(indir_grant)) {
		log_warn(&log, "Failed cpf_grant_indirect()\n");
		r = -1;
		goto deselect;
	}

	memset(&m, '\0', sizeof(message));
	m.m_type = BUSC_I2C_EXEC;
	m.m_li2cdriver_i2c_busc_i2c_exec.grant = indir_grant;
	m.m_li2cdriver_i2c_busc_i2c_exec.mux_exec = 1;

	r = ipc_sendrec(parent_bus_endpoint, &m);
	cpf_revoke(indir_grant);
	if (r != OK) {
		log_warn(&log, " Failed ipc_sendrec()\n");
		goto deselect;
	}

deselect:
	r = i2cdriver_mux_deselect(mux_endpoint, channel);
	if(r != OK) {
		log_warn(&log, "Failed i2cdriver_mux_deselect()\n");
		return r;
	}

	return r;
}

static int
mux_i2c_ioctl(devminor_t UNUSED(minor), unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int UNUSED(flags), endpoint_t UNUSED(user_endpt),
	cdev_id_t UNUSED(id))
{
	int r;

	switch (request) {
	case MINIX_I2C_IOCTL_EXEC:
		r = do_mux_i2c_ioctl_exec(endpt, grant, 0);
		break;
	default:
		log_warn(&log, "Invalid ioctl() 0x%x\n", request);
		r = ENOTTY;
		break;
	}

	return r;
}

static void
mux_i2c_other(message * m, int ipc_status)
{
	message m_reply;
	int r;

	if (is_ipc_notify(ipc_status)) {
		/* handle notifications about drivers changing state */
		if (m->m_source == DS_PROC_NR) {
			ds_event();
		}
		return;
	}

	switch (m->m_type) {
	case BUSC_I2C_RESERVE:
		/* reserve a device on the bus for exclusive access */
		printf("mux_i2c: R: %d - %d - %d\n", m->m_source, m->m_li2cdriver_i2c_busc_i2c_reserve.addr, m->m_li2cdriver_i2c_busc_i2c_reserve.mux_reserve);
		r = do_mux_i2c_reserve(m);
		break;
	case BUSC_I2C_EXEC:
		/* handle request from another driver */
		printf("mux_i2c: E: %d - %d\n", m->m_source, m->m_li2cdriver_i2c_busc_i2c_exec.mux_exec);
		r = do_mux_i2c_ioctl_exec(m->m_source, m->m_li2cdriver_i2c_busc_i2c_exec.grant,
									m->m_li2cdriver_i2c_busc_i2c_exec.mux_exec);
		break;
	default:
		/*log_warn(&log, "Invalid message type (0x%x)\n", m->m_type);*/
		r = EINVAL;
		break;
	}

	log_trace(&log, "mux_i2c_other() returning r=%d\n", r);

	/* Send a reply. */
	memset(&m_reply, 0, sizeof(m_reply));
	m_reply.m_type = r;

	if ((r = ipc_send(m->m_source, &m_reply)) != OK)
		log_warn(&log, "ipc_send() to %d failed: %d\n", m->m_source, r);
}

/*
 * The bus drivers are subscribed to DS events about device drivers on their
 * bus. When the device drivers restart, DS sends a notification and this
 * function updates the reservation table with the device driver's new
 * endpoint.
 */
static void
ds_event(void)
{
	char key[DS_MAX_KEYLEN];
	u32_t value;
	int type;
	endpoint_t owner_endpoint;
	int r;

	/* check for pending events */
	while ((r = ds_check(key, &type, &owner_endpoint)) == OK) {

		r = ds_retrieve_u32(key, &value);
		if (r != OK) {
			log_warn(&log, "ds_retrieve_u32() failed r=%d\n", r);
			return;
		}

		log_debug(&log, "key='%s' owner_endpoint=0x%x\n", key,
		    owner_endpoint);

		if (value == DS_DRIVER_UP) {
			/* clean up any old reservations the driver had */
			log_debug(&log, "DS_DRIVER_UP\n");
			update_reservation(owner_endpoint, key);
		}
	}
}

static int
sef_cb_lu_state_save(int UNUSED(result), int UNUSED(flags))
{
	int r;
	char key[DS_MAX_KEYLEN];

	memset(key, '\0', DS_MAX_KEYLEN);
	snprintf(key, DS_MAX_KEYLEN, "i2c.%d.i2cdev", i2c_bus_id + 1);
	r = ds_publish_mem(key, i2cdev, sizeof(i2cdev), DSF_OVERWRITE);
	if (r != OK) {
		log_warn(&log, "ds_publish_mem(%s) failed (r=%d)\n", key, r);
		return r;
	}

	log_debug(&log, "State Saved\n");

	return OK;
}

static int
sef_cb_lu_state_restore(void)
{
	int r;
	char key[DS_MAX_KEYLEN];
	size_t size = sizeof(i2cdev);

	memset(key, '\0', DS_MAX_KEYLEN);
	snprintf(key, DS_MAX_KEYLEN, "i2c.%d.i2cdev", i2c_bus_id + 1);

	r = ds_retrieve_mem(key, (char *) i2cdev, &size);
	if (r != OK) {
		log_warn(&log, "ds_retrieve_mem(%s) failed (r=%d)\n", key, r);
		return r;
	}

	log_debug(&log, "State Restored\n");

	return OK;
}

static int
sef_cb_init(int type, sef_init_info_t * UNUSED(info))
{
	int r;
	char regex[DS_MAX_KEYLEN];
	r = env_parse_muxinstance();
	if(r != OK){
		log_warn(&log, "env_parse_muxinstance failed\n");
		return r;
	}

	if (type != SEF_INIT_FRESH) {
		/* Restore a prior state. */
		sef_cb_lu_state_restore();
	}

	/* Announce we are up when necessary. */
	if (type != SEF_INIT_LU) {

		/* only capture events for this particular bus */
		snprintf(regex, DS_MAX_KEYLEN, "drv\\.i2c\\.%d\\..*",
		    i2c_bus_id + 1);

		/* Subscribe to driver events for i2c drivers */
		r = ds_subscribe(regex, DSF_INITIAL | DSF_OVERWRITE);
		if (r != OK) {
			log_warn(&log, "ds_subscribe() failed\n");
			return r;
		}

		chardriver_announce();
	}

	/* Save state */
	sef_cb_lu_state_save(0, 0);

	/* Initialization completed successfully. */
	return OK;
}

static void
sef_local_startup()
{
	/* Register init callbacks. */
	sef_setcb_init_fresh(sef_cb_init);
	sef_setcb_init_lu(sef_cb_init);
	sef_setcb_init_restart(sef_cb_init);

	/* Register live update callbacks */
	sef_setcb_lu_state_save(sef_cb_lu_state_save);

	/* Let SEF perform startup. */
	sef_startup();
}

static int
env_parse_muxinstance(void)
{
	int r;
	long instance, chan, parent_bus;
	char mux_label[DS_MAX_KEYLEN];

	/* Parse the instance number passed to service */
	instance = 0;
	r = env_parse("instance", "d", 0, &instance, 1, 11);
	if (r == -1) {
		log_warn(&log,
				"Expecting '-arg instance=N' argument.\n");
		return EXIT_FAILURE;
	}
	/* Device files count from 1, hardware starts counting from 0 */
	i2c_bus_id = instance - 1;

	parent_bus = 0;
	r = env_parse("parent-instance", "d", 0, &parent_bus, 1, 11);
	if (r == -1) {
		log_warn(&log, "Expecting '-arg parent-instance=N' argument.\n");
		return EXIT_FAILURE;
	}
	parent_bus_endpoint = i2cdriver_bus_endpoint(parent_bus);
	if (parent_bus_endpoint == 0) {
		log_warn(&log, "Couldn't find bus driver.\n");
		return EXIT_FAILURE;
	}

	/* Parse the instance number passed to service */
	r = env_parse("channel", "d", 0, &chan, 0, 7);
	if (r == -1) {
		log_warn(&log, "Expecting '-arg channel=N' argument (N=0..7).\n");
		return EXIT_FAILURE;
	}
	channel = chan;

	r = env_get_param("mux", mux_label, sizeof(mux_label));
	if (r == -1) {
		log_warn(&log, "Expecting '-arg mux=<label>' argument.\n");
		return EXIT_FAILURE;
	}
	r = ds_retrieve_label_endpt(mux_label, &mux_endpoint);
	if (r != OK) {
		log_warn(&log, "Couldn't find mux driver.\n");
		return EXIT_FAILURE;
	}

	return OK;
}

int
main(int argc, char *argv[])
{
	int r;

	env_setargs(argc, argv);

	memset(i2cdev, '\0', sizeof(i2cdev));
	sef_local_startup();

	chardriver_task(&i2c_tab);

	return OK;
}

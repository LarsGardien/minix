/* Driver for the TCA9548A I2C multiplexer */

#include <minix/ds.h>
#include <minix/drivers.h>
#include <minix/i2c.h>
#include <minix/i2cdriver.h>
#include <minix/chardriver.h>
#include <minix/log.h>
#include <minix/type.h>
#include <minix/spin.h>
#include <minix/com.h>

/* logging - use with log_warn(), log_info(), log_debug(), log_trace(), etc */
static struct log log = {
	.name = "tca9548a",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* The slave address is hardwired to 0x39 and cannot be changed. */
static i2c_addr_t valid_addrs[9] = {
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x00
};

/* the bus that this device is on (counting starting at 1) */
static uint32_t bus;

/* slave address of the device */
static i2c_addr_t address;

/* endpoint for the driver for the bus itself. */
static endpoint_t bus_endpoint;

static uint8_t channel_mask = 0x00;

/* main driver functions */
static int tca9548a_init(void);
static int tca9548a_select(uint8_t channel);
static int tca9548a_deselect(uint8_t channel);

/* libchardriver callbacks */
static void tca9548a_other(message * m, int ipc_status);

/* Entry points to this driver from libchardriver. */
static struct chardriver tca9548a_tab = {
	.cdr_other	= tca9548a_other
};

static int
tca9548a_init(void)
{
	int r;
	channel_mask = 0x00;
	r = i2creg_raw_write8(bus_endpoint, address, channel_mask);
	if (r != OK) {
		log_warn(&log, "Channel mask reset failed.\n");
		return -1;
	}
	return OK;
}

static int
tca9548a_select(uint8_t channel)
{
	int r;
	channel_mask |= (1 << channel);
	r = i2creg_raw_write8(bus_endpoint, address, channel_mask);
	if (r != OK) {
		log_warn(&log, "Channel mask select failed.\n");
		return -1;
	}
	return OK;
}

static int
tca9548a_deselect(uint8_t channel)
{
	int r;

	channel_mask &= ~(1 << channel);
	r = i2creg_raw_write8(bus_endpoint, address, channel_mask);
	if (r != OK) {
		log_warn(&log, "Channel mask deselect failed.\n");
		return -1;
	}
	return OK;
}

static void
tca9548a_other(message * m, int ipc_status)
{
	message m_reply;
	int r;

	if (is_ipc_notify(ipc_status)) {
		if (m->m_source == DS_PROC_NR) {
			log_debug(&log,
			    "bus driver changed state, update endpoint\n");
			i2cdriver_handle_bus_update(&bus_endpoint, bus,
			    address);
		}
		return;
	}

	switch (m->m_type) {
	case BUSC_I2C_MUX_SELECT:
	  r = tca9548a_select(m->m_li2cdriver_i2c_busc_i2c_mux.channel);
		break;
	case BUSC_I2C_MUX_DESELECT:
		r = tca9548a_deselect(m->m_li2cdriver_i2c_busc_i2c_mux.channel);
		break;
	default:
		log_warn(&log, "Invalid message type (0x%x)\n", m->m_type);
		r = EINVAL;
		break;
	}

	log_trace(&log, "i2c_other() returning r=%d\n", r);

	/* Send a reply. */
	memset(&m_reply, 0, sizeof(m_reply));
	m_reply.m_type = r;

	if ((r = ipc_send(m->m_source, &m_reply)) != OK)
		log_warn(&log, "ipc_send() to %d failed: %d\n", m->m_source, r);
}

static int
sef_cb_lu_state_save(int UNUSED(result), int UNUSED(flags))
{
	ds_publish_u32("bus", bus, DSF_OVERWRITE);
	ds_publish_u32("address", address, DSF_OVERWRITE);
	return OK;
}

static int
lu_state_restore(void)
{
	/* Restore the state. */
	u32_t value;

	ds_retrieve_u32("bus", &value);
	ds_delete_u32("bus");
	bus = (int) value;

	ds_retrieve_u32("address", &value);
	ds_delete_u32("address");
	address = (int) value;

	return OK;
}

static int
sef_cb_init(int type, sef_init_info_t * UNUSED(info))
{
	int r;

	if (type == SEF_INIT_LU) {
		/* Restore the state. */
		lu_state_restore();
	}

	/* look-up the endpoint for the bus driver */
	bus_endpoint = i2cdriver_bus_endpoint(bus);
	if (bus_endpoint == 0) {
		log_warn(&log, "Couldn't find bus driver.\n");
		return EXIT_FAILURE;
	}

	/* claim the device */
	r = i2cdriver_reserve_device(bus_endpoint, address);
	if (r != OK) {
		log_warn(&log, "Couldn't reserve device 0x%x (r=%d)\n",
		    address, r);
		return EXIT_FAILURE;
	}

	r = tca9548a_init();
	if (r != OK) {
		log_warn(&log, "Device Init Failed\n");
		return EXIT_FAILURE;
	}

	if (type != SEF_INIT_LU) {

		/* sign up for updates about the i2c bus going down/up */
		r = i2cdriver_subscribe_bus_updates(bus);
		if (r != OK) {
			log_warn(&log, "Couldn't subscribe to bus updates\n");
			return EXIT_FAILURE;
		}

		i2cdriver_announce(bus);
		log_debug(&log, "announced\n");
	}

	return OK;
}

static void
sef_local_startup(void)
{
	/*
	 * Register init callbacks. Use the same function for all event types
	 */
	sef_setcb_init_fresh(sef_cb_init);
	sef_setcb_init_lu(sef_cb_init);
	sef_setcb_init_restart(sef_cb_init);

	/*
	 * Register live update callbacks.
	 */
	sef_setcb_lu_state_save(sef_cb_lu_state_save);

	/* Let SEF perform startup. */
	sef_startup();
}

int
main(int argc, char *argv[])
{
	int r;

	env_setargs(argc, argv);

	r = i2cdriver_env_parse(&bus, &address, valid_addrs);
	if (r < 0) {
		log_warn(&log, "Expecting -args 'bus=X address=0xYY'\n");
		log_warn(&log, "Example -args 'bus=1 address=0x70'\n");
		return EXIT_FAILURE;
	} else if (r > 0) {
		log_warn(&log,
		    "Invalid slave address for device, expecting 0x70\n");
		return EXIT_FAILURE;
	}

	sef_local_startup();

	chardriver_task(&tca9548a_tab);

	return 0;
}

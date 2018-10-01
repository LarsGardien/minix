/* Driver for the tcs34725 Color Sensor */

#include <minix/ds.h>
#include <minix/drivers.h>
#include <minix/i2c.h>
#include <minix/i2cdriver.h>
#include <minix/chardriver.h>
#include <minix/log.h>
#include <minix/type.h>

#include "tcs34725.h"

/* logging - use with log_warn(), log_info(), log_debug(), log_trace(), etc */
static struct log log = {
	.name = "tcs34725",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* The slave address is hardwired to 0x39 and cannot be changed. */
static i2c_addr_t valid_addrs[2] = {
	0x29, 0x00
};

/* the bus that this device is on (counting starting at 1) */
static uint32_t bus;

/* slave address of the device */
static i2c_addr_t address;

/* endpoint for the driver for the bus itself. */
static endpoint_t bus_endpoint;

/* main driver functions */
static int tcs34725_init(void);
static int tcs34725_read8(uint16_t address, uint8_t *value);
static int tcs34725_write8(uint16_t address, uint8_t value);

/* libchardriver callbacks */
static ssize_t tcs34725_read_hook(devminor_t minor, u64_t position, endpoint_t endpt,
    cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static void tcs34725_other_hook(message * m, int ipc_status);

/* Entry points to this driver from libchardriver. */
static struct chardriver tcs34725_tab = {
	.cdr_read		= tcs34725_read_hook,
	.cdr_other	= tcs34725_other_hook
};

static int
tcs34725_init(void)
{
	int r;
	uint8_t val;

	/* Power on the device */
	printf("tcs34725 init. 1937\n");
	if((r = tcs34725_read8(TCS34725_ID, &val)) != OK)
	{
		log_warn(&log, "Failed to read device ID.\n");
		return -1;
	}
	if((val != 0x44) && (val != 0x10))
	{
		log_warn(&log, "Failed to find device.\n");
		return -1;
	}

	if((r = tcs34725_write8(TCS34725_ATIME, TCS34725_INTEGRATIONTIME_2_4MS)) != OK)
	{
		log_warn(&log, "Failed to set initial integration time.\n");
		return -1;
	}

	if((r = tcs34725_write8(TCS34725_CONTROL, TCS34725_GAIN_1X)) != OK)
	{
		log_warn(&log, "Failed to set initial gain.\n");
		return -1;
	}

	if((r = tcs34725_write8(TCS34725_ENABLE,
		TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN)) != OK)
	{
		log_warn(&log, "Failed to power on device.\n");
		return -1;
	}

	return OK;
}

static int
tcs34725_read8(uint16_t reg, uint8_t *value)
{
	int r;
	uint8_t command[] = {TCS34725_COMMAND_BIT | reg};
	if((r = i2creg_read(bus_endpoint, address, command, 1, value, 1)) != OK)
	{
		log_warn(&log, "Failed to read value from device.\n");
		return -1;
	}
	return OK;
}

static int
tcs34725_write8(uint16_t reg, uint8_t value)
{
	int r;
	if((r = i2creg_write8(bus_endpoint, address,
				TCS34725_COMMAND_BIT | reg, value)) != OK)
	{
		log_warn(&log, "Failed to write command to device.\n");
		return -1;
	}
	return OK;
}

static ssize_t
tcs34725_read_hook(devminor_t UNUSED(minor), u64_t position, endpoint_t endpt,
    cp_grant_id_t grant, size_t size, int UNUSED(flags), cdev_id_t UNUSED(id))
{
	int ret;
	uint16_t c = 0, r = 0, g = 0, b = 0;
	printf("tcs34725 read\n");

	uint8_t rgbc[8];
	uint8_t command[] = {TCS34725_COMMAND_BIT | TCS34725_CDATAL};
	if((ret = i2creg_read(bus_endpoint, address, command, 1, rgbc, 8)) != OK)
	{
		log_warn(&log, "Failed to read from device.\n");
		return -1;
	}
	c = (rgbc[1] << 8) | rgbc[0];
	r = (rgbc[3] << 8) | rgbc[2];
	g = (rgbc[5] << 8) | rgbc[4];
	b = (rgbc[7] << 8) | rgbc[6];

	printf("R %d; G %d; B %d; C %d;\n", r, g, b, c);
	return OK;
}

static void
tcs34725_other_hook(message * m, int ipc_status)
{
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

	log_warn(&log, "Invalid message type (0x%x)\n", m->m_type);
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

	r = tcs34725_init();
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
		log_warn(&log, "Example -args 'bus=1 address=0x39'\n");
		return EXIT_FAILURE;
	} else if (r > 0) {
		log_warn(&log,
		    "Invalid slave address for device, expecting 0x39\n");
		return EXIT_FAILURE;
	}

	sef_local_startup();

	chardriver_task(&tcs34725_tab);

	return 0;
}

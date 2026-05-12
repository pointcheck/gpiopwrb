/*
* pwrb.c - kernel module for Power Button state change
*
* It monitors given pin on GPIO port using standard gpio framework.
* Once state is changed, an _EVT or PWRB event is raised to ACPI subsystem.
*
*/
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/kenv.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <machine/bus.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>
#include <dev/gpio/gpiobusvar.h>

#include "gpiobus_if.h"

#define	GPIO_POLL_TIMEO		(1000/4)	// Poll GPIO every 1/4 second
#define	GPIO_PWRB_PIN_NUM	0		// Pin number to monitor: pin 0
#define	ACPI_PATH_SIZE		32
#define	GPIO_PWRB_ACPI_PATH	"\\_SB.GPIO._EVT"	// Default ACPI path to call

#define	GPIOPWRB_DEBUG	1

#ifdef GPIOPWRB_DEBUG
#define dprintf printf
#else
#define dprintf(x, arg...)
#endif

struct gpiopwrb_softc {
	device_t dev;		/* this device */
	device_t gpiobus_dev;	/* gpiobus parent */
	device_t child_dev;	/* gpiobus child dev */
	gpio_pin_t gpio_pin;
	ACPI_HANDLE acpi_handle;
	struct mtx thread_mtx;
	int thread_terminate;
	int pin_num;
	char acpi_path[ACPI_PATH_SIZE];
} gpiopwrb_softc_sc; /* we use global softc because we need it in MOD_LOAD */

static struct thread *gpiopwrb_daemon_thread;


static void gpiopwrb_call_acpi(struct gpiopwrb_softc* sc)
{
	ACPI_STATUS status;
	ACPI_OBJECT args[2];
	ACPI_OBJECT_LIST arglist;

	memset(args, 0, sizeof(args));
	memset(&arglist, 0, sizeof(arglist));
	
	/* ACPI event \_SB.GPIO._EVT usually takes two arguments:
	 * arg0 - event source number (GPIO pin number)
	 * arg1 - 0x80 for status change
	 *
	 * ACPI event \_SB.PWRB.PKG2 takes two args that are returned as package
	 */

	args[0].Type = ACPI_TYPE_INTEGER;
	args[0].Integer.Value = (ACPI_INTEGER) (INT64)0;	// event source number
	args[1].Type = ACPI_TYPE_INTEGER;
	args[1].Integer.Value = (ACPI_INTEGER) (INT64)0x80;	// pin status change
	arglist.Count = 2;
	arglist.Pointer = args;

	status = AcpiEvaluateObject(sc->acpi_handle, NULL, &arglist, NULL);

	if(ACPI_FAILURE(status))
		printf("pwrb: failed to call AcpiEvaluateObject: %s\n",
			AcpiFormatException(status));
}


static void gpiopwrb_daemon(void* arg)
{
	struct gpiopwrb_softc* sc = (struct gpiopwrb_softc*) arg;

	if (sc == NULL) {
		printf("pwrb: bogus softc, thread stopped!\n");
		goto stop_daemon;
	}
	
	dprintf("pwrb: kthread %p started, sc = %p\n", gpiopwrb_daemon_thread, sc);

	if (!sc->gpio_pin) {
		printf("pwrb: gpio pin %d has not been acquired during attach!\n",
			sc->pin_num);
		goto stop_daemon;
	}

	bool gpio_state, gpio_state_prev;

	gpio_pin_is_active(sc->gpio_pin, &gpio_state);
	gpio_state_prev = gpio_state;
	
	dprintf("pwrb: gpio pin %d state = %d\n", sc->pin_num, gpio_state);

	while (1) {
		int terminate;

		mtx_lock(&sc->thread_mtx);
		terminate = sc->thread_terminate;
		mtx_unlock(&sc->thread_mtx);

		if (terminate > 0)
			break;

		gpio_pin_is_active(sc->gpio_pin, &gpio_state);

		bool changed = (gpio_state != gpio_state_prev);

		//dprintf("pwrb: poll... gpio pin %d state = %d, changed = %s\n",
		//	sc->pin_num, gpio_state, changed ? "YES" : "no");

		if (changed) {
			printf("pwrb: GPIO pin %d changed, emmiting ACPI event %s\n",
				sc->pin_num, sc->acpi_path);
			gpiopwrb_call_acpi(sc);
		}

		gpio_state_prev = gpio_state;

		pause_sig("pwrb sleep", GPIO_POLL_TIMEO);
	}

	stop_daemon:

	dprintf("pwrb: kthread %p exited\n", gpiopwrb_daemon_thread);
	
	mtx_lock(&sc->thread_mtx);
	sc->thread_terminate++;
	mtx_unlock(&sc->thread_mtx);

	kthread_exit();
}

static void gpiopwrb_identify(driver_t *driver, device_t parent)
{
	dprintf("pwrb: identify\n");

	struct gpiopwrb_softc* sc = &gpiopwrb_softc_sc;

	if (sc->child_dev) {
		dprintf("pwrb: child_dev = %p already set for parrent = %p\n",
			sc->child_dev, parent);
		return;
	}

	sc->child_dev = BUS_ADD_CHILD(parent, 0, "gpiopwrb", -1);
	sc->gpiobus_dev = parent;

	/* Fetch GPIO pin number from Hints */

	char hint_str[32];
	char *hint_value;

	snprintf(hint_str, 32, "hint.%s.%d.pin_num",
	    device_get_name(sc->child_dev),
	    device_get_unit(sc->child_dev));

	if ((hint_value = kern_getenv(hint_str)) != NULL)
		sc->pin_num = strtol(hint_value, NULL, 0);
	else
		sc->pin_num = GPIO_PWRB_PIN_NUM;

	dprintf("pwrb: new child_dev = %p for parrent = %p, pin_num = %d, %s = %s\n",
		sc->child_dev, parent, sc->pin_num, hint_str, hint_value);

	/* Fetch ACPI path from Hints */

	snprintf(hint_str, 32, "hint.%s.%d.acpi_path",
	    device_get_name(sc->child_dev),
	    device_get_unit(sc->child_dev));

	if ((hint_value = kern_getenv(hint_str)) != NULL)
		strncpy(sc->acpi_path, hint_value, ACPI_PATH_SIZE); 
	else
		snprintf(sc->acpi_path, ACPI_PATH_SIZE, GPIO_PWRB_ACPI_PATH);

	dprintf("pwrb: acpi path hint = %s, value = %s\n", hint_str, sc->acpi_path);

	device_set_softc(sc->child_dev, sc);
}

static int gpiopwrb_probe(device_t dev)
{
	if (acpi_disabled("gpiopwrb")) {
		dprintf("pwrb: ACPI is disabled\n");
		return (ENXIO);
	}

	device_set_desc(dev, "GPIO Power Button Monitor");

	dprintf("pwrb: probed\n");

	return (BUS_PROBE_DEFAULT);
}


static int gpiopwrb_attach(device_t dev)
{
	int error;
	struct gpiopwrb_softc *sc = device_get_softc(dev);

	sc->dev = dev;
	sc->gpiobus_dev = device_get_parent(dev);

	ACPI_STATUS acpi_status = AcpiGetHandle(NULL, sc->acpi_path, &sc->acpi_handle);
	if(ACPI_FAILURE(acpi_status)) {
		printf("pwrb: failed to get ACPI handle for %s: %s\n", sc->acpi_path, AcpiFormatException(acpi_status));
		return (EINVAL);
	}

	dprintf("pwrb: gpiobus = %s, child = %s\n",
		device_get_name(sc->gpiobus_dev), device_get_name(sc->child_dev));

	error = gpio_pin_get_by_bus_pinnum(sc->gpiobus_dev, sc->pin_num, &sc->gpio_pin);

	dprintf("pwrb: gpio_pin_get_by_bus_pin_num error = %d, gpio_pin->pin = %d, "
		"gpio_pin->flags = %x, gpio_pin->dev = %p, devname = %s\n",
			error,
			sc->gpio_pin->pin, sc->gpio_pin->flags, sc->gpio_pin->dev,
			device_get_name(sc->gpio_pin->dev));

	if (error) {
		printf("pwrb: failed to get GPIO pin %d value, gpiobus = %p, "
			"child = %p, error = %d\n",
			sc->pin_num, sc->gpiobus_dev, sc->child_dev, error);
		return (ENXIO);
	}

	sc->thread_terminate = 0;

	mtx_init(&sc->thread_mtx, "pwrb thread mtx", NULL, MTX_DEF);

	error = kthread_add(gpiopwrb_daemon, sc, NULL, &gpiopwrb_daemon_thread,
		0, 0, "pwrb_daemon");

	if (error) {
		printf("pwrb: failed to start kthread, error = %d\n", error);
		return (error);
	}

	device_set_softc(dev,sc);

	printf("pwrb: attached to gpio pin %d\n", sc->pin_num);

	return (0);
}

static int gpiopwrb_detach(device_t dev)
{
	struct gpiopwrb_softc *sc = device_get_softc(dev);

	if (!sc)
		return (0);

	kthread_resume(gpiopwrb_daemon_thread); // wakeup thread if it's sleeping

	sc->thread_terminate++; // signal thread to terminate

	dprintf("pwrb: waiting thread termination...\n");

	while (1) {
		mtx_lock(&sc->thread_mtx);
		int tmp = sc->thread_terminate; 
		mtx_unlock(&sc->thread_mtx);

		if (tmp > 1)
			break;

		pause("wait thread", hz);
	}

	dprintf("pwrb: detached from gpio pin %d\n", sc->pin_num);

	return (0);
}

static device_method_t gpiopwrb_methods[] = {
	DEVMETHOD(device_identify, gpiopwrb_identify),
	DEVMETHOD(device_probe, gpiopwrb_probe),
	DEVMETHOD(device_attach, gpiopwrb_attach),
	DEVMETHOD(device_detach, gpiopwrb_detach),
	DEVMETHOD_END
};

DEFINE_CLASS_0(gpiopwrb, gpiopwrb_driver, gpiopwrb_methods, sizeof(struct gpiopwrb_softc));
DRIVER_MODULE(gpiopwrb, gpiobus, gpiopwrb_driver, 0, 0);	
MODULE_DEPEND(gpiopwrb, acpi, 1, 1, 1);
MODULE_DEPEND(gpiopwrb, gpiobus, 1, 1, 1);
MODULE_DEPEND(gpiopwrb, amdgpio, 1, 1, 1);
MODULE_VERSION(gpiopwrb, 1);

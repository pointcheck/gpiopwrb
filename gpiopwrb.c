/*
* pwrb.c - kernel module for Power Button state change
*
* It monitors given pin on GPIO port using standard gpio framework.
* Once state is changed, an PWRB event is raised to ACPI subsystem.
*
*/
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <machine/bus.h>
//#include <machine/resource.h>

#include <dev/gpio/gpiobusvar.h>
//#include <dev/gpio/gpiobus.h>
//#include <dev/gpio/gpioc.h>

//#define	GPIO_POLL_TIMEO	(1000/4)	// Poll GPIO every 1/4 second
#define	GPIO_POLL_TIMEO	(3000)

struct gpiopwrb_softc {
	device_t dev;
	device_t gpio_bus; /* gpiobus parent */
	int pin;
};

static struct thread *gpiopwrb_daemon_thread;

static int gpiopwrb_daemon_thread_terminate;
static struct mtx gpiopwrb_daemon_thread_mtx;

static void gpiopwrb_daemon(void* arg)
{
	printf("pwrb: kthread %p started\n", gpiopwrb_daemon_thread);

	while(!gpiopwrb_daemon_thread_terminate) {
		printf("pwrb: poll...\n");

		int error = pause_sig("pwrb sleep", GPIO_POLL_TIMEO);

		printf("pwrb: error = %d\n", error);
	}

	printf("pwrb: kthread %p exited\n", gpiopwrb_daemon_thread);
	
	mtx_lock(&gpiopwrb_daemon_thread_mtx);
	gpiopwrb_daemon_thread_terminate = 2;
	mtx_unlock(&gpiopwrb_daemon_thread_mtx);

	kthread_exit();
}

/*
* Event handler - called when the module is loaded/unloaded
*/
static int gpiopwrb_event_handler(module_t mod, int cmd, void *arg)
{
	int error = 0;

	switch (cmd) {

	case MOD_LOAD:
		mtx_init(&gpiopwrb_daemon_thread_mtx, "pwrb thread mtx", NULL, MTX_DEF);
		gpiopwrb_daemon_thread_terminate = 0;
     		error = kthread_add(gpiopwrb_daemon, NULL, NULL, &gpiopwrb_daemon_thread,
			0, 0, "pwrb_daemon");
		if(!error)
			printf("pwrb: loaded.\n");
		break;

	case MOD_UNLOAD:
		gpiopwrb_daemon_thread_terminate = 1;
		kthread_resume(gpiopwrb_daemon_thread); // wakeup thread if it's sleeping

		printf("pwrb: waiting thread for terminating...\n");

		while(1) {
			mtx_lock(&gpiopwrb_daemon_thread_mtx);
			int tmp = gpiopwrb_daemon_thread_terminate; 
			mtx_unlock(&gpiopwrb_daemon_thread_mtx);

			if(tmp == 2)
				break;

			pause("wait thread", hz);
		}
		printf("pwrb: unloaded.\n");
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}


static int gpiopwrb_probe(device_t dev)
{
	device_set_desc(dev, "GPIO Power Button monitor");

	printf("pwrb: probed\n");

	return BUS_PROBE_DEFAULT;
}


static int gpiopwrb_attach(device_t dev)
{
	struct gpiopwrb_softc *sc = device_get_softc(dev);

	sc->dev = dev;
	sc->pin = 0; /* change to your GPIO pin number */

	printf("pwrb: attached\n");

	return 0;
}

static int gpiopwrb_detach(device_t dev)
{
	//struct gpiopwrb_softc *sc = device_get_softc(dev);
	printf("pwrb: detached\n");
	return 0;
}


static device_method_t gpiopwrb_methods[] = {
	DEVMETHOD(device_probe, gpiopwrb_probe),
	DEVMETHOD(device_attach, gpiopwrb_attach),
	DEVMETHOD(device_detach, gpiopwrb_detach),
	DEVMETHOD_END
};



static driver_t gpiopwrb_driver = {
	"gpiopwrb",
	gpiopwrb_methods,
	sizeof(struct gpiopwrb_softc),
};

static devclass_t gpiopwrb_devclass;

DRIVER_MODULE(gpiopwrb, gpiobus, gpiopwrb_driver, gpiopwrb_devclass, gpiopwrb_event_handler, 0);
MODULE_DEPEND(gpiopwrb, acpi, 1, 1, 1);
MODULE_DEPEND(gpiopwrb, gpiobus, 1, 1, 1);
MODULE_VERSION(gpiopwrb, 1);


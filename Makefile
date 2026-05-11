# Build script for GPIO Power Button driver 

KMOD= gpiopwrb
SRCS= gpiopwrb.c device_if.h bus_if.h gpio_if.h gpiobus_if.h acpi_if.h opt_platform.h opt_acpi.h

.include <bsd.kmod.mk>


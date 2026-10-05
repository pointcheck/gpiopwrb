# Build script for GPIO Power Button driver module

KMOD=	gpiopwrb
SRCS=	gpiopwrb.c device_if.h bus_if.h gpio_if.h acpi_if.h opt_acpi.h opt_platform.h

.if defined(DEBUG)
CFLAGS+=	-DGPIOPWRB_DEBUG=1
.endif

.include <bsd.kmod.mk>

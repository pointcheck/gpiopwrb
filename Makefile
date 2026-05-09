# Build script for GPIO Power Button driver 

KMOD= gpiopwrb
SRCS= gpiopwrb.c device_if.h bus_if.h gpio_if.h opt_platform.h

.include <bsd.kmod.mk>


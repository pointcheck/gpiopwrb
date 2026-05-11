# Kernel Module for monitoring state of Power Button connected to AMD GPIO

On many Hardware-Reduced PCs equipped with AMD's CPUs, mostly modern laptops,
Power Button is wired to AMD GPIO controller. FreeBSD has support for AMD GPIO
since 2018 implemented by amdgpio.ko module (sys/dev/amdgpio/amdgpio.c).
Unfortunately, **amdgpio** module does not support interrupts on amd64 platform,
it's simply not implemented. This **gpiopwrb** module provides a quick and dirty
hack to allow Power Buttons to work on such PCs by periodically polling state of
given GPIO pin and raising ACPI event \_SB_.PWRB.

## How to Build and Install

```
sudo make clean
sudo make
sudo make install
```

## Configuring gpiopwrb module

Simply add below to your **/boot/loader.conf** file:

```
hint.gpiopwrb.pin_num=0 # <-- set GPIO pin number on your machine, default is zero.
gpiopwrb_load="YES"
```

## Documentation to read

```
man 4 gpiobus
man 4 acpi
```

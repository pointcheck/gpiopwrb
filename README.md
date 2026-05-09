# Kernel Module for monitoring state of Power Button connected to AMD GPIO

On may Hardware-Reduced PCs equipped with AMD's CPUS, mostely modern laptops,
Power Button is wired to AMD GPIO controller. FreeBSD has support for AMD GPIO
since 2018 implemented by amdgpio.ko module (sys/dev/amdgpio/amdgpio.c).
Unfortunately, this amdgpio module does not support interrupts on amd64 platform,
it's simply not implemented. This module provides a quick and dirty hack to
allow Power Buttons to work on such PCs by periodically polling stage of given
GPIO pin and raising ACPI event \_SB_.PWRB.

## How to Build and Install

```
sudo make clean
sudo make
sudo make install
```

## Documentation to read

```
man 9 module
```

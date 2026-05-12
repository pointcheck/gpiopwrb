# FreeBSD Kernel Module for monitoring state of Power Button connected to AMD GPIO

On many PCs with Hardware-Reduced ACPI equipped with AMD Ryzen CPUs, which are mostly modern laptops,
Power Button is wired to AMD GPIO controller. FreeBSD has support for AMD GPIO
since 2018 implemented by **amdgpio** module (sys/dev/amdgpio/amdgpio.c).
Unfortunately, **amdgpio** module does not support interrupts on amd64 platform,
it's simply not implemented because of lack of INTRNG support in upstream **gpiobus** module.
Hence, pressing Power Button will never trigger poweroff or sleep event.

This **gpiopwrb** module provides a quick and dirty hack to allow Power Buttons
to work on such PCs by periodically polling state of given GPIO pin and calling ACPI method
like \\_SB.GPIO._EVT or \\_SB.PWRB to raise event.

This module was tested on and developped for HP 15e laptop with the following firmware:
```
[000h 0000 004h]                   Signature : "FACP"    [Fixed ACPI Description Table (FADT)]
[004h 0004 004h]                Table Length : 00000114
[008h 0008 001h]                    Revision : 06
[009h 0009 001h]                    Checksum : FF
[00Ah 0010 006h]                      Oem ID : "HPQOEM"
[010h 0016 008h]                Oem Table ID : "SLIC-MPC"
[018h 0024 004h]                Oem Revision : 01072009
[01Ch 0028 004h]             Asl Compiler ID : "HP  "
[020h 0032 004h]       Asl Compiler Revision : 00010013
```

## How to Build and Install

```
make clean
make
sudo cp gpiopwrb.ko /boot/kernel/ 
```

## Configuring gpiopwrb module

1. Add below to your **/boot/loader.conf** file:

```
hint.gpiopwrb.0.pin_num=0 # <-- set GPIO pin number on your machine, default is zero.
hint.gpiopwrb.0.acpi_path="\\_SB.GPIO._EVT" # <-- normally GPIO should trigger _EVT ACPI event
# or
hint.gpiopwrb.0.acpi_path="\\_SB.PWRB.PKG2" # <--- on some PCs _EVT is not supported, call to PWRB instead.
```

Preserve the double backslash!!!

2. Add **gpiopwrb** module to **kld_list** variable in your /etc/rc.conf:

```
# sysrc kld_list+=gpiopwrb
```

3. Reboot.

Notes:

 - Loading **gpiopwrb** module from /boot/loader.conf will not work because it depends on a number of other
modules which are not provided during boot time.

 - By default, when no hints provided, **gpiopwrb** module uses pin_num = 0 and calls \\_SB.GPIO._EVT ACPI method to trigger event.

 - For testing, you can use kenv(1) command to temporarily set different hints for **gpiopwrb** module:

 - The exact behaviour of your system upon poweroff/sleep event is regulated by **hw.acpi.power_button_state** sysctl variable. 

```
# kenv hint.gpiopwrb.0.pin_num=0
# kenv hint.gpiopwrb.0.acpi_path='\_SB.GPIO._EVT'
```

Use single backslash and single quote character while in shell!

Use ```kldunload gpiopwrb || kldload gpiopwrb``` to reload module with new hints.

## More reads 

A general description of ACPI subsystem can be found in following system man pages:

```
man 4 gpiobus
man 4 acpi
man 8 iasl
```

In some rare cases you might be needing to decompile DSDT table/firmware provided by BIOS on your machine.
You can easily do that with acpidumpt(8) and iasl(8) tools provided by **acpica-tools** package.
Basically what you need to know is how Power Button is wired (what GPIO is used) and what ACPI method should
be called to raise power button event. Look for PWRB method in DSDT and in other AML tables.

## Debugging

1. There's also **sysutils/acpi_call** port that provides kernel module and tool that allows sending
arbitrary ACPI calls and view returning results, it can be useful for debugging ACPI issues.
To call \\_SB.PWRB.PKG2 event with acpi_call, use the following syntax:

```
# acpi_call -v -p '\_SB.GPIO._EVT' -i 0 -i 0x80
```
or
```
# acpi_call -v -p '\_SB.PWRB.PKG2' -i 0 -i 0x80
```

Note: if that works, your system will go immediate poweroff/sleep after issuing above commands.

2. It's possible to build **gpiopwrb** module with quite verbose debugging output. For that, define GPIOPWRB_DEBUG macro in source file gpiopwrb.c, recompile and reinstall.

## Links

1. https://en.wikipedia.org/wiki/ACPI
2. https://wiki.freebsd.org/GPIO



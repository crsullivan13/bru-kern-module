# Per-Bank Bandwidth Regulator Kernel Module

This repository contains a kernel module for interfacing with the per-bank bandwidth regulator and is designed for use in the **FireSim/Chipyard** environment with **FireMarshal**.
[`Regulator`](https://github.com/crsullivan13/BwReg)

## Building and Installing the Kernel Module

### 1. Update Kernel Source Path
Modify the `KERNEL_SRC` variable in the `Makefile` to point to the path of the kernel source in your environment.

### 2. Build the Module
Ensure you have the RISC-V toolchain sourced, then run:
```sh
make
```

### 3. Copy the Module to the Workload Directory
Once built, copy the kernel module to your FireMarshal workload directory:

### 4. Insert and Remove the Kernel Module
To insert the module:
```sh
insmod bru_driver.ko
```
To remove the module:
```sh
rmmod bru_driver
```

## DebugFS Interface

This module creates a **debugfs** interface at:
```sh
/sys/kernel/debug/bru
```
If this directory is not visible after inserting the module, mount **debugfs** using:
```sh
mount -t debugfs none /sys/kernel/debug
```

The following files are created under `/sys/kernel/debug/bru/`:

```sh
/sys/kernel/debug/bru/client_control   # Enables/disables regulation per client and assigns domains
/sys/kernel/debug/bru/domain_reads     # Sets the read access budget per domain
/sys/kernel/debug/bru/global_enable    # Globally enables/disables the regulator
/sys/kernel/debug/bru/global_period    # Sets the regulation period (in cycles)
```

## Configuring the Regulator

### 1. Enable/Disable Regulation Per Client & Assign Domains
Each client (typically a core) can be assigned a domain and enabled/disabled.  
To configure, use:
```sh
echo <CLIENT_0_ENABLE> <CLIENT_0_DOMAIN> ... <CLIENT_N-1_ENABLE> <CLIENT_N-1_DOMAIN> > /sys/kernel/debug/bru/client_control
```
Example (dual-core system, both cores enabled in domain 0):
```sh
echo 1 0 1 0 > /sys/kernel/debug/bru/client_control
```

### 2. Set Read Access Budget Per Domain
To configure the number of allowed read accesses per domain in a period:
```sh
echo <DOMAIN_0_READS> ... <DOMAIN_N-1_READS> > /sys/kernel/debug/bru/domain_reads
```
Example (Domain 0: 1 access, Domain 1: 8 accesses):
```sh
echo 1 8 > /sys/kernel/debug/bru/domain_reads
```

### 3. Enable/Disable the Regulator Globally
The regulator must be globally enabled for regulation to occur:
```sh
echo <VALUE> > /sys/kernel/debug/bru/global_enable
```
- `0` → Off
- `1` → On

Example (Enable regulation):
```sh
echo 1 > /sys/kernel/debug/bru/global_enable
```

### 4. Set the Regulation Period
The regulation period is defined in clock cycles:
```sh
echo <VALUE> > /sys/kernel/debug/bru/global_period
```
Example (Set period to 400 cycles):
```sh
echo 400 > /sys/kernel/debug/bru/global_period
```

This repo has a kernel module for interfacing with the per-bank regulator https://github.com/crsullivan13/BwReg (which is based on https://github.com/farzadfch/rocket-chip/blob/bf45db0dae0925ba86482d6ab8fa5bc37b158a93/src/main/scala/subsystem/BwRegulator.scala)

It is assumed you are using it in FireSim/Chipyard environment with Firemarshal.

Build the module by running 'make'

client_control  domain_reads  global_enable  global_period

mount -t debugfs none /sys/kernel/debug

insmod bru_driver.ko

/sys/kernel/debug/bru/

# ENV-COMBO I2C Sensor Driver

This Linux kernel module implements a fictional I2C environmental sensor driver supporting temperature and humidity readings via the IIO subsystem.

## Build Instructions

### Yocto Setup & Kernel Configuration

1. Install Yocto as described in the official ARM guide:
   [Yocto QEMU Learning Path](https://learn.arm.com/learning-paths/embedded-and-microcontrollers/yocto_qemu/yocto_build/)

2. Checkout Yocto Branch:
   
   $ git checkout tags/yocto-4.0.26 -b yocto-4.0.26-local

4. Initialize Build Environment:

   $ source oe-init-build-env build-qemu-arm64

5. Edit Configuration:
   Open `conf/local.conf` and ensure the following lines are added:

   MACHINE = "qemuarm64"
   
   CORE_IMAGE_EXTRA_INSTALL += "openssh"
   
   TOOLS += "gcc make kernel-dev"
   
   IMAGE_INSTALL:append = " kernel-devsrc"

6. Enable IIO Subsystem:
   
   $ bitbake -c menuconfig virtual/kernel
   
   Then in the menu:
   
   -> Device Drivers
   
     -> Industrial I/O support (IIO)
   
       [*] Industrial I/O support
   
       [*] Enable buffer support within IIO and submodules
   
       [*] Enable IIO configuration via configfs
   
       [*] Enable triggered sampling support
   
       [*] Maximum number of consumers per trigger
   
       [*] Enable triggered events support
   
7. Enable I2C

   $ bitbake -c menuconfig virtual/kernel
   
   Go to Device Drivers → I2C support
   Then in the menu:

   -> Device Drivers
   
   	->Enable
        ->I2C support (Press Y)
   
	Save and exit.


9. Rebuild the Kernel:

   $ bitbake virtual/kernel

10. Build Image with Kernel Headers:

    $ bitbake core-image-minimal

### Out-of-Tree Module Build

To compile the driver out-of-tree using the appropriate cross-compiler, follow these steps:

1. Open the QEMU devshell to use the correct Yocto environmen
   
   $ bitbake -c devshell virtual/kernel

3. Inside the devshell, navigate to the module source directory and run:

   $ make KDIR=/path/to/linux-qemuarm64-standard-build

4. To clean the build:

   $ make -C /path/to/linux-qemuarm64-standard-build M=$PWD clean

#### Example Makefile

This driver is built as an out-of-tree kernel module. Below is the `Makefile` used:

```makefile
obj-m := env-combo.o

all:
	make -C $(KDIR) \
		M=$(PWD) \
		ARCH=$(ARCH) \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		modules

clean:
	make -C $(KDIR) M=$(PWD) clean
```

## Running QEMU Environment

After building the Yocto image with the configured kernel and IIO support, you can boot into QEMU as follows:

$ runqemu qemuarm64

Once booted, you can log in (usually as `root`, no password needed), and proceed to transfer and test the kernel modules as explained below.

## Testing the Driver
There are two available methods for testing the ENV-COMBO Linux kernel driver:

### 1. Manual Command Testing:
### Copy Modules

Copy the following to the QEMU target via SSH:

* env-combo.ko
  
* i2c-envcombo-sim.ko
  
### Load Modules

$ insmod i2c-envcombo-sim.ko   # I2C simulator

$ insmod env-combo.ko	       # I2C Driver

### Create I2C Device (Optional for simulated environments)

$ echo env-combo 0x39 > /sys/bus/i2c/devices/i2c-0/new_device

### Verify sysfs Entries

Sensor sysfs entries should appear under:

/sys/bus/iio/devices/iio:device0/

* in_temp0_raw
* in_humidityrelative1_raw

### Sysfs Paths to Read Values

$ cat /sys/bus/iio/devices/iio:device0/in_temp0_raw

$ cat /sys/bus/iio/devices/iio:device0/in_humidityrelative1_raw

### 2. scripted Testing with test_env-combo.sh:
### Transfer & Run Test Script

Copy the following to the QEMU target via SSH:

* test_env-combo.sh
  
Run the test script:

$ chmod +x test_env-combo.sh

$ ./test_env-combo.sh

This script will:
* Load both modules
* Verify sysfs entries
* Read and print the implemented functionality
* Check dmesg for logs and errors
* Cleanly unload the modules
  
## Additional Considerations

* Ensure kernel configuration changes for IIO are persistent by saving the `.config` file from menuconfig.
* Use Yocto devshell to ensure access to the correct cross-compiler toolchain.
* Be mindful that `bitbake virtual/kernel` may overwrite menuconfig changes unless saved appropriately.

Author: Sami Natshe
License: GPLv2

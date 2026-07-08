# Utopia

Utopia is a (probably good) x86_64 operating system kernel.

<div align="center">
    <img width="783" height="566" alt="image" src="https://github.com/user-attachments/assets/64c4e884-17e4-4a95-8d9e-82f8a8ce3256" />
</div>

## Dependencies

- a normal assembler: NASM
- a C compiler 
- GNU make (or any other program that reads Makefiles)

## System requirements

Have a working x86_64 machine (kinda).

Jokes aside but should work everywhere if you have an x86_64 processor. Tested on my machine or in QEMU and everything mostly behaves as it should. 

## Building and testing

To build the project, simply use `make`. Alternatively, you can only build the kernel using `make build_kernel`.

Testing is usually done in one of two ways:
- **flashing a pendrive and booting on real hardware**: use `dd` to flash an USB drive, then boot from it and test 
- **using an emulator**: run `make run` to open the kernel in QEMU

There are two bootloaders that are supported natively - Limine and GRUB (default is Limine, but you can change that with `BOOTLOADER=grub`). After tweaking the Makefile, you should be able to run the kernel on any bootloader that supports either Limine boot protocol or Multiboot1.

## Additional notes

- Contributions are welcome!
- Utopia is licensed under GNU GPL v3.0

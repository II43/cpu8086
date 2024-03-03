#ifndef BIOS_H
#define BIOS_H

void bios_interrupt_10h(uint8_t fcn);
void bios_interrupt_21h(uint8_t fcn);

void bios_interrupt(uint8_t interrupt);

#endif
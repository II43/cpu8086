#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include "cpu8086.h"

void bios_interrupt_10h(uint8_t fcn)
{
    syslog(LOG_DEBUG,"Interrupt 10h - function call 0x%02X\n" , fcn);
    switch(fcn) {
        case 0x0E:
            // 0Eh - Print a character
            syslog(LOG_DEBUG,"PRINTING: %c\n", AL);
            printf("%c", AL);
            break;
        default:
            syslog(LOG_DEBUG,"Not implemented interrupt routine 0x%02X\n", fcn);
    }
}

void bios_interrupt_21h(uint8_t fcn)
{
    uint16_t i = 0;
    syslog(LOG_DEBUG,"Interrupt 21h - function call 0x%02X\n" , fcn);
    switch(fcn) {
        case 0x09:
            // 09h - Write string (terminated by a $ character)to standard output.
            // DS:DX = segment:offset of string
            i = 0;
            syslog(LOG_DEBUG,"PRINTING (0x%04X %c):\n", 16*DS + 0*DX, memory[16*DS+DX+i]);
            // $ = 0x24 = 36
            while (memory[16*DS+DX+i] != '$') {
                printf("%c", memory[16*DS+DX+i]);
                syslog(LOG_INFO,"%c", memory[16*DS+DX+i]);
                i++;
            }
            break;
        case 0x4C:
            // 4Ch - Exit the program
            syslog(LOG_DEBUG,"EXITING 0x%02X!\n", AL);
            clean_up_and_exit(AL);
            break;
        default:
            syslog(LOG_DEBUG,"Not implemented interrupt routine 0x%02X\n", fcn);
    }
}

void bios_interrupt(uint8_t interrupt)
{
    syslog(LOG_DEBUG,"BIOS interrupt 0x%02X", interrupt);
    switch(interrupt) {
        case 0x10:
            bios_interrupt_10h(AH);
            break; 
        case 0x21:
            bios_interrupt_21h(AH);
            break;    
        default:
            syslog(LOG_DEBUG,"Not implemented bios interrupt 0x%02X\n", interrupt);
    }
}
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include "cpu8086.h"
#include "cpu8086_instructions.h"

int main(int argc, char **argv)
{
    if (argc == 1) {
        printf("Error: You must pass a binary program to run as first argument!\n");
        exit(1);
    }
    // Logging
    openlog("cpu8086", LOG_CONS, LOG_LOCAL0);
    // Self-Test
    GeneralPurposeRegister T;
    T.X = 0x1234;
    syslog(LOG_DEBUG,"Test of general purpose register\n");
    syslog(LOG_DEBUG,"X=0x%04X H=0x%02X L=0x%02X\n", T.X, T.H, T.L);
    // Main
    cpu_init();
    // load_program("yourhelp.com", 0x1FD0);
    load_program(argv[1], 0x1FD0);
    // 0x100+16*0x01ED = 0x100 + 0x1ED0
    printf("Running\n");
    cpu_run();
    printf("Done");
    closelog();
    return EXIT_SUCCESS;
}
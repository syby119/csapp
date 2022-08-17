#include <stdio.h>
#include <stdlib.h>
#include "phases.h"

FILE *infile;

int main(int argc, char *argv[]) {
    if (argc == 1) {
        infile = stdin;
    } else {
        if (argc == 2) {
            infile = fopen(argv[1], "r");
            if (!infile) {
                printf("%s: Error: Couldn't open %s\n", argv[0], argv[1]);
                exit(8);
            }
        }  else {
            printf("Usage: %s [<input_file>]\n", argv[0]);
            exit(8);
        }
    }

    initialize_bomb();

    puts("Welcome to my fiendish little bomb. You have 6 phases with");
    puts("which to blow yourself up. Have a nice day!");

    phase_1(read_line());
    phase_defused();
    puts("Phase 1 defused. How about the next one?");

    phase_2(read_line());
    phase_defused();
    puts("That's number 2.  Keep going!");

    phase_3(read_line());
    phase_defused();
    puts("Halfway there!");

    phase_4(read_line());
    phase_defused();
    puts("So you got that one.  Try this one.");

    phase_5(read_line());
    phase_defused();
    puts("Good work!  On to the next...");

    phase_6(read_line());
    phase_defused();

    return 0;
}

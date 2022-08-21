#ifdef __linux__
#include <unistd.h>
#include <signal.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "phases.h"

extern FILE *infile;

int num_input_strings = 0;

char input_strings[1000];

int string_length(const char* str) {
    int length;
    if (*str == '\0') {
        length = 0;
    } else {
        const char* p = str;
        do {
            p++;
            length = (int)(p - str);
        } while (*p != '\0'); 
    }

    return length;
}

int strings_not_equal(const char* str1, const char* str2) {
    const char *p = str1, *q = str2;
    int len1 = string_length(p);
    int len2 = string_length(q);

    if (len1 == len2) {
        if (*p == '\0') {
            return 0;
        }

        while (*p == *q) {
            ++p;
            ++q;
            if (*p != '\0') {
                continue;
            } else {
                return 0;
            }
        }
        return 1;
    } else {
        return 1;
    }
}

int blank_line(const char* str) {
    const char* p = str;
    char c = *p;

    if (c == '\0') {
        return 1;
    }

    while (c != '\0') {
        if (isspace(c)) {
            ++p;
            c = *p;
            if (c == '\0') {
                return 1;
            }
        } else  {
            return 0;
        }
    }
}

char* skip() {
    char *str;

    do {
        str = fgets(input_strings + 80 * num_input_strings, 80, infile);
        if (str == NULL) {
            break;
        }

        if (blank_line(str) == 0) {
            break;
        }
    } while(1);

    return str;
}

char* read_line() {
    if (skip() == NULL) {
        if (infile == stdin) {
            puts("Error: Premature EOF on stdin");
            exit(8);
        }

        if (getenv("GRADE_BOMB") != NULL) {
            exit(0);
        }

        infile = stdin;
        if (skip() == 0) {
            puts("Error: Premature EOF on stdin");
            exit(0);
        }
    }

    char* str = input_strings + 80 * num_input_strings;
    char* p = str;
    
    // mov    $0x0,%eax
    // mov    $0xffffffffffffffff,%rcx
    // repnz scas %es:(%rdi),%al
    // not    %rcx
    // sub    $0x1,%rcx
    // https://stackoverflow.com/questions/26783797/repnz-scas-assembly-instruction-specifics
    unsigned long length = -1;
    while (length != 0) {
        int ZF = ('\0' == *p);
        ++p;
        --length;
        if (ZF) break;
    }
    length = ~length;
    --length;

    if (length > 80) {
        puts("Error: Input line too long");
        int offset = num_input_strings++;
        offset *= 80;
        char info[16] = "***truncated***";
        *(long*)(input_strings + offset) = *(long*)info;
        *(long*)(input_strings + offset + 8) = *(long*)(info + 8);

        explode_bomb();
    }

    --length;
    input_strings[length + 80 * num_input_strings] = '\0';
    ++num_input_strings;

    return str;
}

void sig_handler() {
#ifdef __linux__
    puts("So you think you can stop the bomb with ctrl-c, do you?");
    sleep(3);
    printf("Well...");
    fflush(stdout);
    sleep(1);
    puts("OK. :-)");
#endif

    exit(16);
}

void initialize_bomb() {
#ifdef __linux__
    signal(SIGINT, sig_handler);
#endif
}

void explode_bomb() {
    puts("\nBOOM!!!");
    puts("The bomb has blown up.");
    exit(8);
}

void phase_1(const char* line) {
    const char* key = "Border relations with Canada have never been better.";
    if (strings_not_equal(line, key) == 0) {
        return;
    }

    explode_bomb();
}

void read_six_numbers(const char* buf, int* numbers) {
    int ret = sscanf(buf, "%d %d %d %d %d %d", 
                     numbers + 0, numbers + 1, numbers + 2,
                     numbers + 3, numbers + 4, numbers + 5);
    if (ret > 5) {
        return;
    }

    explode_bomb();
}

void phase_2(const char* line) {
    int numbers[6];
    read_six_numbers(line, numbers);
    if (numbers[0] == 1) {
        for (int i = 1; i < 6; ++i) {
            int current = numbers[i];
            int previous = numbers[i - 1];
            int twice_previous = previous + previous;
            if (twice_previous == current) {
                continue;
            }

            explode_bomb();
        }
    } else {
        explode_bomb();
    }
}

void phase_3(const char* line) {
    int num1, num2;
    int ret = sscanf(line, "%d %d", &num1, &num2);
    if (ret > 1) {
        if (num1 > 7) {
            explode_bomb();
        } else {
            int magic_number;
            switch(num1) {
                case 0: /* jmpq 0x400f7c */
                    magic_number = 0xcf;
                    break;
                case 1: /* jmpq 0x400fb9 */
                    magic_number = 0x137;
                    break;
                case 2: /* jmpq 0x400f83 */
                    magic_number = 0x2c3;
                    break;
                case 3: /* jmpq 0x400f8a */
                    magic_number = 0x100;
                    break;
                case 4: /* jmpq 0x400f91 */
                    magic_number = 0x185;
                    break;
                case 5: /* jmpq 0x400f98 */
                    magic_number = 0xce;
                    break;
                case 6: /* jmpq 0x400f9f */
                    magic_number = 0x2aa;
                    break;
                case 7: /* jmpq 0x400fa6 */
                    magic_number = 0x147;
                    break;
            }

            if (magic_number != num2) {
                explode_bomb();
            }
        }
    } else {
        explode_bomb();
    }
}

int func4(int n, int a, int b) {
    int x = b - a;
    x = (x + (x < 0 ? 1 : 0)) >> 1;
    int y =  x + a;

    if (y <= n) {
        if (y >= n) {
            return 0;
        } else {
            return 2 * func4(n, y + 1, b) + 1;
        }
    } else {
        return 2 * func4(n, a, y - 1);
    }
}

void phase_4(const char* line) {
    int num1, num2;
    int ret;

    ret = sscanf(line, "%d %d", &num1, &num2);
    if (ret != 2) {
        explode_bomb();
    }

    if (num1 > 14) {
        explode_bomb();
    }

    ret = func4(num1, 0, 14);
    if (ret != 0) {
        explode_bomb();
    }

    if (num2 != 0) {
        explode_bomb();
    }
}

void phase_5(const char* line) {
    const char* search_str = "maduiersnfotvbyl";
    char buf[8];

    int len = string_length(line);
    if (len == 6) {
        int i = 0;
        do {
            buf[i] = search_str[line[i] & 0xf];
            ++i;
        } while (i != 6);
        buf[6] = '\0';

        int ret = strings_not_equal(buf, "flyers");
        if (ret != 0) {
            explode_bomb();
        }
    } else {
        explode_bomb();
    }
}

struct Node {
    int data;
    int offset;
    struct Node* next;
};

struct Node node6 = { /* @0x603320 */
    0x1bb,
    6, 
    NULL
};

struct Node node5 = { /* @0x603310 */
    0x1dd, 
    5,
    &node6 /* 0x603320 */
};

struct Node node4 = { /* @0x603300 */
    0x2b3, 
    4,
    &node5 /* 0x603310 */
};

struct Node node3 = { /* @0x6032f0 */
    0x39c,
    3,
    &node4 /* 0x603300 */
};

struct Node node2 = { /* @0x6032e0 */
    0xa8,
    2,
    &node3 /* 0x6032f0 */
};

struct Node node1 = { /* @0x6032d0 */
    0x14c,
    1,
    &node2 /* 0x6032e0 */
};

void phase_6(const char* line) {
    int i, j, k;
    int numbers[6];

    struct Node* p;
    struct Node* nodes[6];

    read_six_numbers(line, numbers);

    /* @0x40110b */
    for (i = 0, j = 0; ; ++i) {
        if (numbers[i] - 1 > 5) {
            explode_bomb();
        }

        ++j;
        if (j == 6) {
            break;
        }

        k = j;
        do {
            if (numbers[k] == numbers[i]) {
                explode_bomb();
            }

            ++k;
        } while (k <= 5);
    }

    /* @0x401153 */
    i = 0;
    do {
        numbers[i] = 7 - numbers[i];
        ++i;
    } while (i != 6);

    /* @0x40116f */
    i = 0;
    do {
        if (numbers[i] <= 1) {
            p = &node1;
        } else {
            j = 1;
            p = &node1;

            do {
                p = p->next;
                j += 1;
            } while (j != numbers[i]);
        }

        nodes[i] = p;
        ++i;
    } while(i != 6);

    /* @0x4011ab */
    for (i = 0, j = 1; ; ) {
        nodes[i]->next = nodes[j];
        ++j;
        if (j == 6) {
            break;
        }
        ++i;
    };
    nodes[5]->next = NULL;

    /* @0x4011da */
    i = 5;
    p = nodes[0];
    do {
        if (p->data < p->next->data) {
            explode_bomb();
        }

        p = p->next;
        --i;
    } while (i != 0);
}

struct SecretNode {
    int data;
    struct SecretNode* left;
    struct SecretNode* right;
    struct SecretNode* padding;
};

struct SecretNode n45 = { /* @0x6031d0 */
    0x28,
    NULL,
    NULL,
    NULL
};

struct SecretNode n41 = { /* @0x6031f0 */
    0x01,
    NULL,
    NULL,
    NULL
};

struct SecretNode n47 = { /* @0x603210 */
    0x63,
    NULL,
    NULL,
    NULL
};

struct SecretNode n44 = { /* @0x603230 */
    0x23,
    NULL,
    NULL,
    NULL
};

struct SecretNode n42 = { /* @0x603250 */
    0x07,
    NULL,
    NULL,
    NULL
};

struct SecretNode n43 = { /* @0x603270 */
    0x14,
    NULL,
    NULL,
    NULL
};

struct SecretNode n46 = { /* @0x603290 */
    0x2f,
    NULL,
    NULL,
    NULL
};

struct SecretNode n48 = { /* @0x6032b0 */
    0xe9,
    NULL,
    NULL,
    NULL
};

struct SecretNode n32 = { /* @0x603150 */
    0x16,
    &n43, /*0x603270 */
    &n44, /*0x603230 */
    NULL
};

struct SecretNode n33 = { /* @0x603170 */
    0x2d,
    &n45, /* 0x6031d0 */
    &n46, /* 0x603290 */
    NULL
};

struct SecretNode n31 = { /* @0x603190 */
    0x06,
    &n41, /* 0x6031f0 */
    &n42, /* 0x603250 */
    NULL
};

struct SecretNode n34 = { /* @0x6031b0 */
    0x6b,
    &n47, /* 0x603210 */
    &n48, /* 0x6032b0 */
    NULL
};

struct SecretNode n21 = { /* @0x603110 */
    0x08,
    &n31, /* 0x603190 */
    &n32, /* 0x603150 */
    NULL
};

struct SecretNode n22 = { /* @0x603130 */
    0x32,
    &n33, /* 0x603170 */
    &n34, /* 0x6031b0 */
    NULL
};

struct SecretNode n1 = { /* @0x6030f0 */
    0x24, 
    &n21, /* 0x603110 */
    &n22, /* 0x603130*/
    NULL
};

int fun7(struct SecretNode* node, int target) {
    if (node == NULL) {
        return -1;
    }

    int data = node->data;
    if (data > target) {
        return 2 * fun7(node->left, target);
    }

    if (data == target) {
        return 0;
    }

    return 2 * fun7(node->right, target) + 1;
}

void secret_phase() {
    const char* line = read_line();
    long target = strtol(line, NULL, 10);
    if (target - 1 > 0x3e8) {
        explode_bomb(); 
    }

    int ret = fun7(&n1, target);
    if (ret != 2) {
        explode_bomb();
    }

    puts("Wow! You've defused the secret stage!");

    phase_defused();
}

void phase_defused() {
    int num1, num2;
    char s[20];
    if (num_input_strings != 6) {
        return;
    }

    char* buf = input_strings + 240;

    int ret = sscanf(input_strings + 240, "%d %d %s", &num1, &num2, s);

    if (ret == 3) {
        if (strings_not_equal(s, "DrEvil") == 0) {
            puts("Curses, you've found the secret phase!");
            puts("But finding it and solving it are quite different...");
            secret_phase();
        }
    }

    puts("Congratulations! You've defused the bomb!");
}
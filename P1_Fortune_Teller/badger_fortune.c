#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if(argc < 5) {
        fprintf(stdout, "USAGE: \n\tbadger-fortune -f <file> -n <number> (optionally: -o <output file>) \n\t\t OR \n\tbadger-fortune -f <file> -b <batch file> (optionally: -o <output file>)\n");
        exit(1);
    }

    int opt;
    FILE *fp = NULL;       // pointer to fortune file
    FILE *bp = NULL;       // pointer to batch file
    FILE *op = NULL;       // pointer to output file
    int n = -1;
    bool is_n = false;
    bool is_b = false;
    char c;                 // check empty file

    int invalid_flag_idx = -1;
    int curr_idx = 1;
    while(curr_idx < argc) {
        if((curr_idx % 2 == 1) && (argv[curr_idx][0] != '-')) {
            invalid_flag_idx = curr_idx;
            break;
        }
        curr_idx++;
    }

    while((opt = getopt (argc, argv, ":f:n:b:o:")) != -1) {
        if((invalid_flag_idx != -1) && (optind > invalid_flag_idx) && (opt != 'f')) {
            fprintf(stdout, "ERROR: Invalid Flag Types\n");
            if(fp != NULL) fclose(fp);
            if(bp != NULL) fclose(bp);
            if(op != NULL) fclose(op);
            exit(1);
        }

        switch(opt) {
            case 'f':
                fp = fopen(optarg, "r");
                if(fp == NULL) {
                    fprintf(stdout, "ERROR: Can't open fortune file\n");
                    if(fp != NULL) fclose(fp);
                    if(bp != NULL) fclose(bp);
                    if(op != NULL) fclose(op);
                    exit(1);
                }
                c = fgetc(fp);
                if(c == EOF) {
                    fprintf(stdout, "ERROR: Fortune File Empty\n");
                    if(fp != NULL) fclose(fp);
                    if(bp != NULL) fclose(bp);
                    if(op != NULL) fclose(op);
                    exit(1);
                } else {
                    ungetc(c, fp);
                }
                break;
            case 'n':
                is_n = true;
                if(is_b) {
                    fprintf(stdout, "ERROR: You can't specify a specific fortune number in conjunction with batch mode\n");
                    if(fp != NULL) fclose(fp);
                    if(bp != NULL) fclose(bp);
                    if(op != NULL) fclose(op);
                    exit(1);
                }
                n = atoi(optarg);
                if(n <= 0) {
                    fprintf(stdout, "ERROR: Invalid Fortune Number\n");
                    if(fp != NULL) fclose(fp);
                    if(bp != NULL) fclose(bp);
                    if(op != NULL) fclose(op);
                    exit(1);
                }
                break;
            case 'b':
                is_b = true;
                if(is_n) {
                    fprintf(stdout, "ERROR: You can't use batch mode when specifying a fortune number using -n\n");
                    if(fp != NULL) fclose(fp);
                    if(bp != NULL) fclose(bp);
                    if(op != NULL) fclose(op);
                    exit(1);
                }
                bp = fopen(optarg, "r");
                if(bp == NULL) {
                    fprintf(stdout, "ERROR: Can't open batch file\n");
                    if(fp != NULL) fclose(fp);
                    if(bp != NULL) fclose(bp);
                    if(op != NULL) fclose(op);
                    exit(1);
                }
                c = fgetc(bp);
                if(c == EOF) {
                    fprintf(stdout, "ERROR: Batch File Empty\n");
                    if(fp != NULL) fclose(fp);
                    if(bp != NULL) fclose(bp);
                    if(op != NULL) fclose(op);
                    exit(1);
                } else {
                    ungetc(c, bp);
                }
                break;
            case 'o':
                op = fopen(optarg, "w");
                break;
            case ':':
                if((optopt == 'b') && is_n) {
                    fprintf(stdout, "ERROR: You can't use batch mode when specifying a fortune number using -n\n");
                    if(fp != NULL) fclose(fp);
                    if(bp != NULL) fclose(bp);
                    if(op != NULL) fclose(op);
                    exit(1);
                }
                else if((optopt == 'n') && is_b) {
                    fprintf(stdout, "ERROR: You can't specify a specific fortune number in conjunction with batch mode\n");
                    if(fp != NULL) fclose(fp);
                    if(bp != NULL) fclose(bp);
                    if(op != NULL) fclose(op);
                    exit(1);
                }
                else {
                    fprintf(stdout, "ERROR: No fortune file was provided\n");
                    if(fp != NULL) fclose(fp);
                    if(bp != NULL) fclose(bp);
                    if(op != NULL) fclose(op);
                    exit(1);
                }
                break;
            case '?':
                fprintf(stdout, "ERROR: Invalid Flag Types\n");
                if(fp != NULL) fclose(fp);
                if(bp != NULL) fclose(bp);
                if(op != NULL) fclose(op);
                exit(1);
                break;
        }
    }

    if((invalid_flag_idx != -1) && (optind >= invalid_flag_idx)) {
        fprintf(stdout, "ERROR: Invalid Flag Types\n");
        if(fp != NULL) fclose(fp);
        if(bp != NULL) fclose(bp);
        if(op != NULL) fclose(op);
        exit(1);
    }
    if((argc == 6) && (strcmp(argv[argc - 1], "-f") != 0)) {
        fprintf(stdout, "ERROR: Invalid Flag Types\n");
        if(fp != NULL) fclose(fp);
        if(bp != NULL) fclose(bp);
        if(op != NULL) fclose(op);
        exit(1);
    }

    // -f not specified
    if(fp == NULL) {
        fprintf(stdout, "ERROR: No fortune file was provided\n");
        if(fp != NULL) fclose(fp);
        if(bp != NULL) fclose(bp);
        if(op != NULL) fclose(op);
        exit(1);
    }

    char int_buffer[20];
    fgets(int_buffer, 20, fp);
    int f_cnt = atoi(int_buffer);
    if(n > f_cnt) {
        fprintf(stdout, "ERROR: Invalid Fortune Number\n");
        if(fp != NULL) fclose(fp);
        if(bp != NULL) fclose(bp);
        if(op != NULL) fclose(op);
        exit(1);
    }

    // no more arg errors, allocate heap resources
    fgets(int_buffer, 20, fp);
    int f_max_size = atoi(int_buffer) + 1;          // +1 for the \0 character
    if(f_max_size < 3)
        f_max_size = 3;             // buffers need to be at least have 3 slots to hold %\n\0
    char** fortunes = malloc(f_cnt * sizeof(char*));
    for(int i = 0; i < f_cnt; i++)
        fortunes[i] = malloc(f_max_size * sizeof(char));

    char buffer[f_max_size];        // buffer for line reads
    char temp[f_max_size];          // buffer for a fortune
    temp[0] = '\0';
    int cnt = 0;
    while(fgets(buffer, f_max_size, fp) != NULL) {
        if(cnt > f_cnt)
            break;
        if((buffer[0] == '%') && (buffer[1] == '\n')) {
            if(cnt == 0) {          // skip first %
                cnt++;
                continue;
            }
            else {
                strcpy(fortunes[cnt - 1], temp);
                temp[0] = '\0';
                cnt++;
                continue;
            }
        }
        else {
            strcat(temp, buffer);
        }
    }
    // handle last fortune
    strcpy(fortunes[f_cnt - 1], temp);

    if(is_n) {        // number mode
        if(op != NULL)
            fprintf(op, "%s", fortunes[n - 1]);
        else
            fprintf(stdout, "%s", fortunes[n - 1]);
    } else {           // batch mode
        while(fgets(int_buffer, 20, bp) != NULL) {
            int num = atoi(int_buffer);
            if(num <= 0 || num > f_cnt)
                fprintf(stdout, "ERROR: Invalid Fortune Number\n\n");
            else {
                if(op != NULL)
                    fprintf(op, "%s\n\n", fortunes[num - 1]);
                else
                    fprintf(stdout, "%s\n\n", fortunes[num - 1]);
            }
        }
    }

    // free heap, close files
    for(int i = 0; i < f_cnt; i++)
        free(fortunes[i]);
    free(fortunes);
    if(fp != NULL) fclose(fp);
    if(bp != NULL) fclose(bp);
    if(op != NULL) fclose(op);
    exit(0);
}
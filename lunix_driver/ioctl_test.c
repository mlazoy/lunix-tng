#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include<inttypes.h>

#define RAW 60<<8 | 1
#define COOKED 60<<8 | 2

 int fd;
 char *mode, *filename;

void enter_raw_mode(){
    uint32_t reading_raw;
    fprintf(stdout, "entering raw mode\n");
    while(1){
        ioctl(fd,RAW);  //ioctl call
        read(fd, &reading_raw, sizeof(reading_raw));
        printf("%u . . . ",reading_raw);
    }
}
    
void enter_cooked_mode(){
    char reading_cooked[10];
    fprintf(stdout, "entering cooked mode\n");
    while(1){
        ioctl(fd,COOKED);  //ioctl call
        read(fd, reading_cooked, sizeof(reading_cooked));
        printf("%s . . . ",reading_cooked);
    }
}

int main (int argc, char **argv) {
    
    if (argc<3){
        fprintf(stderr, "Usage: ./ioct <filename> <mode>\n mode: RAW | COOKED");
        exit(1);
    }
    
    filename = argv[1];
    mode = argv[2];

    fd = open(filename, O_RDONLY);

    if (fd == -1) {
        fprintf(stderr, "Error in opening file \n");
        return 1;
    }
    
    if (mode == "RAW") enter_raw_mode();
    else if (mode =="COOKED") enter_cooked_mode();
    else fprintf(stderr, "No such mode\n mode: RAW | COOKED");
    
    close(fd);
    return 0;
}

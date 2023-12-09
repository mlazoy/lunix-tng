$ git clone https://github.com/mlazoy/lunix-tng
$ cd lunix_driver

$ su - 
# make
# insmod ./lunix.ko
# ./mk-lunix-devs.sh
# ./lunix-attach /dev/ttyUSB1

//script to test raw output mode
$ gcc -Wall ioctl.c -o io
$ ./io /dev/lunix0-temp RAW



* Install

      $ git clone https://github.com/mlazoy/lunix-tng
      $ cd lunix_driver

* Load module
  
      $ su - \n
      # make
      # insmod ./lunix.ko
      # ./mk-lunix-devs.sh
      # ./lunix-attach /dev/ttyUSB1

* Test

      $ gcc -Wall ioctl.c -o io
      $ ./io /dev/lunix0-temp RAW

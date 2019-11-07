sudo rmmod cdc_acm || :
sudo rmmod arduano || :
make all || :
sudo insmod arduano.ko || :
sudo chmod 666 /dev/arduano* || :
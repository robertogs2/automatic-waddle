sudo rmmod cdc_acm || :
sudo rmmod arduino || :
make local || :
sudo insmod arduino.ko || :
sudo chmod 666 /dev/arduino* || :
sudo make clean
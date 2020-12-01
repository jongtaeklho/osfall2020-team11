mount -o rw,remount /dev/mmcblk0p2 /
./rotd
./selector 74927492 &
sleep 1
./trial 0 &
sleep 1
./trial 1 &
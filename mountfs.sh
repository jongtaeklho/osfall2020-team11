mount -o rw,remount /dev/mmcblk0p2 /
mount -o loop -t ext2 /root/proj4.fs /root/proj4
./gpsupdate.o


touch proj4/file1
./file_loc.o proj4/file1


./build-rpi3-arm64.sh
arm-linux-gnueabi-gcc -I./include test/gpsupdate.c  -o gpsupdate.o
arm-linux-gnueabi-gcc -I./include test/file_loc.c -o file_loc.o
sudo mount ../tizen-image/rootfs.img mnt_dir
sudo mv gpsupdate.o file_loc.o mnt_dir/root
sudo cp proj4.fs mnt_dir/root
sudo umount mnt_dir

bash qemu.sh
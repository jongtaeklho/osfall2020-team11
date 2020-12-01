./build-rpi3-arm64.sh
arm-linux-gnueabi-gcc -I../include rotd.c -o rotd
arm-linux-gnueabi-gcc -I../include test/selector.c -o selector
arm-linux-gnueabi-gcc -I../include test/trial.c -o trial
echo "(a142857a)" | sudo -S su
mount ../tizen-image/rootfs.img mnt_dir
mv rotd selector trial mnt_dir/root
cp test.sh mnt_dir/root
umount mnt_dir

bash qemu.sh
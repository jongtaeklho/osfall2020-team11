cd e2fsprogs-1.45.6
./configure
make

cd ..
dd if=/dev/zero of=proj4.fs bs=1M count=1
sudo losetup /dev/loop15 proj4.fs
sudo ./e2fsprogs-1.45.6/misc/mke2fs -I 256 -L os.proj4 /dev/loop15
sudo losetup -d /dev/loop15
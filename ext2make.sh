sudo umount /dev/loop*
dd if=/dev/zero of=proj4.fs bs=1M count=1
sudo losetup /dev/loop1 proj4.fs
sudo ./e2fsprogs-1.45.6/misc/mke2fs -I 256 -L os.proj4 /dev/loop1
sudo losetup -d /dev/loop1
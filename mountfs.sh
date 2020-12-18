mount -o rw,remount /dev/mmcblk0p2 /
mount -o loop -t ext2 /root/proj4.fs /root/proj4
./gpsupdate.o

# 낙성대역 2번출구
# 37.476957, 126.963732
# 301동 -> 11067.209797585638 from statue of liberty
# acc 1000
# 37.450048, 126.952530
# statue of liberty
# acc 8000
# 40.689461, -74.044479
# hong kong -> 12957.040370823763 from statue of liberty, 2080.2564410418263 from 301동
# acc 3000
# 22.322813, 114.175757
touch proj4/file1
./file_loc.o proj4/file1


# Project 4: Geo-tagged File System
----------------------------------------------------------
## Adding gps information to ext2 file system 
### Adding gps related members on struct inode
ext2에서 사용하는 inode 구조체에 gps 정보를 받을 32비트 멤버를 만든다.

1. e2fsprogs/lib/ext2fs/ext2_fs.h -> `struct ext2_inode`
```C
struct ext2_inode {
    ...
	__s32	i_lat_integer;
	__s32	i_lat_fractional;
	__s32	i_lng_integer;
	__s32	i_lng_fractional;
	__s32	i_accuracy;
    ...
};
```
마운트할 파일 시스템의 디스크 inode에 gps 정보를 넣는다.

2. fs/ext2/ext2.h -> `struct ext2_inode`, `struct ext2_inode_info`
```C
struct ext2_inode {
    ...
	__s32	i_lat_integer;
	__s32	i_lat_fractional;
	__s32	i_lng_integer;
	__s32	i_lng_fractional;
	__s32	i_accuracy;
    ...
};
struct ext2_inode_info {
    ...
	__s32	i_lat_integer;
	__s32	i_lat_fractional;
	__s32	i_lng_integer;
	__s32	i_lng_fractional;
	__s32	i_accuracy;
    ...
};
```
본 파일 시스템의 메모리 및 디스크에서 사용하는 inode에 gps 정보를 넣는다.
### Adding/Modifying gps related functions on struct inode
1. include/linux/fs.h -> `struct inode_operations`
```C
struct inode_operations {
	...
	int (*set_gps_location)(struct inode *);
	int (*get_gps_location)(struct inode *, struct gps_location *);
    ...
};
```
ext2 파일 시스템에서 gps 정보를 관리하는 함수를 inode가 사용할 함수로 추가한다.

2. fs/ext2/ext2.h, fs/ext2/inode.c
fs/ext2/ext2.h에서 gps 관련 함수를 선언하고, inode.c에 구현한다.
```C
/* fs/ext2/ext2.h */
extern int ext2_set_gps_location(struct inode *node);
extern int ext2_get_gps_location(struct inode *, struct gps_location *);
extern int ext2_check_permission(struct inode *inode, int mask);
```
ext2 파일 시스템에서 gps 정보를 관리하는 함수를 inode가 사용할 함수로 추가한다.   
ext2_check_permission은 파일 접근 권한을 확인하는 함수로, 호출 시 현재 위치 및 접근할 파일의 accuracy의 합과 실제 거리를 비교해 접근 가능 여부를 확인한다.

```C
/* fs/ext2/inode.c */
static int __ext2_write_inode(struct inode *inode, int do_sync)
{
    ...
    raw_inode->i_lat_integer = cpu_to_le32(ei->i_lat_integer);
	raw_inode->i_lat_fractional = cpu_to_le32(ei->i_lat_fractional);
	raw_inode->i_lng_integer = cpu_to_le32(ei->i_lng_integer);
	raw_inode->i_lng_fractional = cpu_to_le32(ei->i_lng_fractional);
	raw_inode->i_accuracy = cpu_to_le32(ei->i_accuracy);
    ...
}
/* fs/ext2/namei.c */
static int ext2_create (struct inode * dir, struct dentry * dentry, umode_t mode, bool excl)
{
	...
	if(inode->i_op->set_gps_location != NULL){
		inode->i_op->set_gps_location(inode);
	}
	...
}
```
inode에 정보를 쓰거나 새로운 inode를 만들 때 gps location을 쓰도록 `ext2_write_inode`, `ext2_create` 함수를 변경한다.


### System calls for gps location
이전 프로젝트와 같이 syscall table에 새로운 번호로 시스템 콜을 추가하고, `kernel/gps.c`에 해당 시스템 콜을 구현한다.
`sys_set_gps_location`은 user space에서 gps 정보를 받아 커널에서 기억하는 gps 정보를 해당 정보로 최신화한다.
`sys_get_gps_location`은 입력된 파일의 경로로부터 파일을 찾아 해당 파일의 inode를 읽고, inode에서 gps 정보를 읽어 user space의 gps_location 구조체에 넣는다.


### Accessibility check
`kernel/gps.c`에 정의된 `not_accessible_loc` 함수는 inode 구조체를 입력받아 해당 inode가 가리키는 파일이 커널에 저장된 최근 위치에서 접근 가능한지 확인한다.  
이를 위해서는 두 gps 위치 사이의 거리를 계산해야 한다.  
계산에서는 아래의 haversine 공식을 이용하였다.  
![Screenshot from 2020-12-19 23-21-31](https://user-images.githubusercontent.com/48852336/102691604-021a6300-4251-11eb-9af5-6c79c2d3fb2e.png)

haversine을 이용해 두 점 사이 거리를 계산하려면 cos, arccos의 계산이 필요하다. 커널에서는 floating point 연산이 불가능하기 때문에, 원하는 계산을 위해서는 gps location에 대해 사칙 연산 및 cos, arccos를 계산하는 함수를 만들어야 한다. 이를 위해 덧셈, 뺄셈, 곱셈, cos, arccos를 계산하는 함수와 degree 단위를 radian으로 변경하는 함수를 `kernel/gps.c`에 추가하였다.
```C
/* gps location 연산 함수*/
int add_gps(int int1, int frac1, int int2, int frac2, int *frac);
int sub_gps(int int1, int frac1, int int2, int frac2, int *frac);
int mul_gps(int int1, int frac1, int int2, int frac2, int *frac);
int deg2rad_gps(int deg_i, int deg_f, int *frac);
int cos_gps(int x_i, int x_frac, int *ret_frac);
int acos_gps(int x_i, int x_frac, int *ret_frac);
```
이 때, cos 및 acos는 아래의 테일러 급수를 통해 계산하였다.
![Screenshot from 2020-12-19 23-21-46](https://user-images.githubusercontent.com/48852336/102691646-4b6ab280-4251-11eb-80e4-912ab24946a3.png)

### Test files
`test/gpsupdate.c`는 위도, 경도 및 정확도를 입력받아 커널의 최근 위치 정보에 넣는다. 커널에 위치 정보를 넣는 과정은 앞서 만든 `sys_set_gps_location` 시스템 콜을 이용한다.  
`test/file_loc.c`는 command line을 통해 파일 경로를 입력받고, 해당 경로의 파일을 확인하여 파일이 있으면 파일의 위도, 경도 및 해당 위치를 가리키는 Google Maps 링크를 출력한다.
이 때, 접근 불가능한 위치의 파일일 경우에는 파일의 정보와 Google Maps 링크를 출력하지 않는다.

--------------------------------------------------
## Lessons
+ 리눅스 파일 시스템(ext2)의 구조, inode의 작동 방식 및 관리 방법
+ 새로운 파일 시스템을 기존 시스템에 추가하는 방법
+ Kernel에서 작업 시 floating point 사용이 불가능하다는 점 등을 알게 되었다.

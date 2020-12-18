#ifndef GPS_H_
#define GPS_H_
struct gps_location {
  int lat_integer;
  int lat_fractional;
  int lng_integer;
  int lng_fractional;
  int accuracy;
};

int add_gps(int int1, int frac1, int int2, int frac2, int *frac);
int sub_gps(int int1, int frac1, int int2, int frac2, int *frac);
int mul_gps(int int1, int frac1, int int2, int frac2, int *frac);
int deg2rad_gps(int deg_i, int deg_f, int *frac);
int sin_gps(int x_i, int x_frac, int *ret_frac);
int cos_gps(int x_i, int x_frac, int *ret_frac);
int acos_gps(int x_i, int x_frac, int *ret_frac);
int not_accessible_loc(struct inode *inode);
long sys_set_gps_location(struct gps_location __user *loc);
long sys_get_gps_location(const char __user *pathname, struct gps_location __user *loc);

extern struct gps_location curr_loc;
extern spinlock_t geo_lock;
#endif
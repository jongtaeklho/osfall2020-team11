#include <linux/path.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/math64.h>
#include <linux/gps.h>

#define FRAC_MAX 1000000L
#define LAT_MAX 90
#define LAT_MIN -90
#define LNG_MAX 180
#define LNG_MIN -180
#define PATH_LENGTH_MAX 1024
#define EARTH_RADIUS 6371

DEFINE_SPINLOCK(geo_lock);

struct gps_location curr_loc = {
    .lat_integer = 0,
    .lat_fractional = 0,
    .lng_integer = 0,
    .lng_fractional = 0,
    .accuracy = 0,
};

int add_gps(int int1, int frac1, int int2, int frac2, int *frac)
{
    int ret;
    *frac = (frac1 + frac2) % FRAC_MAX;
    ret = int1 + int2 + (frac1 + frac2) / FRAC_MAX;
    // printk(KERN_INFO "%d.%06d + %d.%6d = %d.%06d", int1, frac1, int2, frac2, ret, *frac);
    return ret;
}

int sub_gps(int int1, int frac1, int int2, int frac2, int *frac)
{
    int ret;
    ret = int1 - int2;
    *frac = (frac1 - frac2) % FRAC_MAX;
    if (*frac < 0)
    {
        ret -= 1;
        *frac += FRAC_MAX;
    }
    // printk(KERN_INFO "%d.%06d - %d.%6d = %d.%06d", int1, frac1, int2, frac2, ret, *frac);
    return ret;
}

int mul_gps(int int1, int frac1, int int2, int frac2, int *frac)
{
    int ret;
    int hihi;
    __s64 hilo, lohi, lolo; // 10^6 * 10^6 > 2^32
    hihi = int1 * int2;
    hilo = (__s64)int1 * frac2;
    lohi = (__s64)frac1 * int2;
    lolo = (__s64)frac1 * (__s64)frac2;
    // printk(KERN_INFO "hihi: %d, hilo: %lld, lohi: %lld, lolo: %lld", hihi, hilo, lohi, lolo);
    
    *frac = (hilo + lohi) % FRAC_MAX + (lolo % FRAC_MAX >= FRAC_MAX / 2) + (lolo / FRAC_MAX);
    ret = hihi + (int)((hilo + lohi) / FRAC_MAX) + (*frac > FRAC_MAX);
    if(*frac < 0){
        ret--;
        *frac += FRAC_MAX;
    }
    
    // printk("%d.%06d * %d.%6d = %d.%06lld", int1, frac1, int2, frac2, ret, *frac);

    return ret;
}

int deg2rad_gps(int deg_i, int deg_f, int *frac)
{
    int ret;
    ret = mul_gps(deg_i, deg_f, 0, 17453, frac);
    // printk(KERN_INFO "%d.%06d (deg) = %d.%06d (rad)", deg_i, deg_f, ret, *frac);
    return ret;
}

int cos_gps(int x_i, int x_frac, int *ret_frac)
{
    int ret, rad_x_i, deg_x_i;
    int rad_x_frac, deg_x_frac;
    int add_term;
    int add_term_frac;
    int frac;
    int i, j;
    int divisor;
    int sign;
    ret = 0;
    divisor = 1;

    if (x_i < 0)
    {
        deg_x_i = sub_gps(0, 0, x_i, x_frac, &deg_x_frac);
    }
    else
    {
        deg_x_i = x_i;
        deg_x_frac = x_frac;
    }

    int rad_x_origin_i, rad_x_origin_frac;
    rad_x_origin_i = deg2rad_gps(deg_x_i, deg_x_frac, &rad_x_origin_frac);
    ret = 1;
    *ret_frac = 0;
    j = 1;
    sign = 1;

    rad_x_i = 1;
    rad_x_frac = 0;

    for (i = 2; i < 18; i += 2)
    {
        for (; j <= i; j++)
        {
            frac = rad_x_frac;
            divisor *= j;
            rad_x_i = mul_gps(rad_x_i, frac, rad_x_origin_i, rad_x_origin_frac, &rad_x_frac);
        }

        frac = rad_x_frac;
        // printk(KERN_INFO "divisor: %d\n", divisor);
        add_term = mul_gps(rad_x_i, frac, 0, FRAC_MAX / divisor, &add_term_frac);
        sign = -sign;
        frac = *ret_frac;
        if (sign > 0)
        {
            ret = add_gps(ret, frac, add_term, add_term_frac, ret_frac);
        }
        else
        {
            ret = sub_gps(ret, frac, add_term, add_term_frac, ret_frac);
        }
    }

    // printk(KERN_INFO "cos(%d.%06d) = %d.%06d", x_i, x_frac, ret, *ret_frac);
    return ret;
}

int acos_gps(int x_i, int x_frac, int *ret_frac)
{
    int pi_half_frac;
    // int dividends[6] = {-1, -3, -5, -35, 63};
    // int divisors[6] = {6, 40, 112, 1152, 2816};
    int coeffs[8] = {166667, 75000, 26786, 30382, 22372, 17352, 13867, 11551};
    int coeff_length = 8;
    pi_half_frac = 570796;

    int ret;
    int add_term;
    int add_term_frac;
    int frac;
    int i, j;
    int coeff;
    ret = 0;
    coeff = 1;

    if(x_i == 1){
        *ret_frac = 0;
        return 0;
    } else if(x_i == -1){
        *ret_frac = 141592;
        return 3;
    }


    ret = 1;
    *ret_frac = pi_half_frac;
    ret = sub_gps(ret, *ret_frac, x_i, x_frac, ret_frac);

    int x_origin_i, x_origin_frac;
    x_origin_i = x_i;
    x_origin_frac = x_frac;
    j = 1;
    for (i = 3; i < 19; i += 2)
    {
        for (; j < i; j++)
        {
            frac = x_frac;
            x_i = mul_gps(x_i, frac, x_origin_i, x_origin_frac, &x_frac);
        }
        coeff = coeffs[(i / 2) - 1];
        frac = x_frac;
        add_term = mul_gps(x_i, frac, 0, coeff, &add_term_frac);
        frac = *ret_frac;
        // printk(KERN_INFO "add_term_acos: %d, add_term_frac:%d,\n", add_term, add_term_frac);
        ret = sub_gps(ret, frac, add_term, add_term_frac, ret_frac);
    }
    // printk(KERN_INFO "acos(%d.%06d) = %d.%06d", x_origin_i, x_origin_frac, ret, *ret_frac);
    return ret;
}

int not_accessible_loc(struct inode *inode)
{
    struct gps_location *file_gps;
    file_gps = (struct gps_location *)kmalloc(sizeof(struct gps_location), GFP_KERNEL);
    if (!file_gps)
    {
        // printk(KERN_ALERT "is_accessible_loc: kmalloc failed");
        return -1;
    }
    if (!(inode->i_op->get_gps_location))
    {
        // printk(KERN_ALERT "is_accessible_loc: get_gps_location not defined\n");
        return -ENODEV;
    }
    if (inode->i_op->get_gps_location(inode, file_gps))
    {
        // printk(KERN_ALERT "is_accessible_loc: error from get_gps_location at inode");
        return -1;
    }

    if (curr_loc.accuracy == file_gps->accuracy && curr_loc.lng_integer == file_gps->lng_integer && curr_loc.lat_integer == file_gps->lat_integer &&
        curr_loc.lat_fractional == file_gps->lat_fractional && curr_loc.lng_fractional == file_gps->lng_fractional)
    {
        // printk(KERN_INFO "GPS location of the file is accessible.\n");
        return 0;
    }
    // calculate distance

    int long_diff_integer, long_diff_fractional;
    if (curr_loc.lng_integer > file_gps->lng_integer)
    {
        long_diff_integer = sub_gps(curr_loc.lng_integer, curr_loc.lng_fractional, file_gps->lng_integer, file_gps->lng_fractional, &long_diff_fractional);
    }
    else if (curr_loc.lng_integer < file_gps->lng_integer)
    {
        long_diff_integer = sub_gps(file_gps->lng_integer, file_gps->lng_fractional, curr_loc.lng_integer, curr_loc.lng_fractional, &long_diff_fractional);
    }
    else
    {
        long_diff_integer = (curr_loc.lng_fractional > file_gps->lng_fractional) ? sub_gps(curr_loc.lng_integer, curr_loc.lng_fractional, file_gps->lng_integer, file_gps->lng_fractional, &long_diff_fractional) : 
        sub_gps(file_gps->lat_integer, file_gps->lng_fractional, curr_loc.lat_integer, curr_loc.lng_fractional, &long_diff_fractional);
    }

    int lat_diff_integer, lat_diff_fractional;
    if (curr_loc.lat_integer > file_gps->lat_integer)
    {
        lat_diff_integer = sub_gps(curr_loc.lat_integer, curr_loc.lat_fractional, file_gps->lat_integer, file_gps->lat_fractional, &lat_diff_fractional);
    }
    else if (curr_loc.lat_integer < file_gps->lat_integer)
    {
        lat_diff_integer = sub_gps(file_gps->lat_integer, file_gps->lat_fractional, curr_loc.lat_integer, curr_loc.lat_fractional, &lat_diff_fractional);
    }
    else
    {
        lat_diff_integer = (curr_loc.lat_fractional > file_gps->lat_fractional) ? sub_gps(curr_loc.lat_integer, curr_loc.lat_fractional, file_gps->lat_integer, file_gps->lat_fractional, &long_diff_fractional) : 
        sub_gps(file_gps->lat_integer, file_gps->lat_fractional, curr_loc.lat_integer, curr_loc.lat_fractional, &lat_diff_fractional);
    }

    // printk(KERN_INFO "long_diff_integer: %d, long_diff_fractional: %d\n", long_diff_integer, long_diff_fractional);
    // printk(KERN_INFO "lat_diff_integer: %d, lat_diff_fractional: %d\n", lat_diff_integer, lat_diff_fractional);

    int cos_curr_loc_lat_integer, cos_curr_loc_lat_fractional;
    cos_curr_loc_lat_integer = cos_gps(curr_loc.lat_integer, curr_loc.lat_fractional, &cos_curr_loc_lat_fractional);
    // printk(KERN_INFO "cos_curr_loc_lat_integer: %d, cos_curr_loc_lat_fractional: %d\n", cos_curr_loc_lat_integer, cos_curr_loc_lat_fractional);

    int cos_file_lat_integer, cos_file_lat_fractional;
    cos_file_lat_integer = cos_gps(file_gps->lat_integer, file_gps->lat_fractional, &cos_file_lat_fractional);
    // printk(KERN_INFO "cos_file_lat_integer: %d, cos_file_lat_fractional: %d\n", cos_file_lat_integer, cos_file_lat_fractional);

    int cos_lat_diff_integer, cos_lat_diff_fractional;
    cos_lat_diff_integer = cos_gps(lat_diff_integer, lat_diff_fractional, &cos_lat_diff_fractional);
    // printk(KERN_INFO "cos_lat_diff_integer: %d, cos_lat_diff_fractional: %d\n", cos_lat_diff_integer, cos_lat_diff_fractional);

    int cos_long_diff_integer, cos_long_diff_fractional;
    cos_long_diff_integer = cos_gps(long_diff_integer, long_diff_fractional, &cos_long_diff_fractional);
    // printk(KERN_INFO "cos_long_diff_integer: %d, cos_long_diff_fractional: %d\n", cos_long_diff_integer, cos_long_diff_fractional);

    int cos_cos_integer, cos_cos_fractional;
    cos_cos_integer = mul_gps(cos_curr_loc_lat_integer, cos_curr_loc_lat_fractional, cos_file_lat_integer, cos_file_lat_fractional, &cos_cos_fractional);
    // printk(KERN_INFO "cos_cos_integer: %d, cos_cos_fractional: %d\n", cos_cos_integer, cos_cos_fractional);

    int haversine_lat_integer, haversine_lat_fractional;
    haversine_lat_integer = sub_gps(1, 0, cos_lat_diff_integer, cos_lat_diff_fractional, &haversine_lat_fractional);
    // printk(KERN_INFO "haversine_lat_integer: %d, haversine_lat_fractional: %d\n", haversine_lat_integer, haversine_lat_fractional);

    int haversine_long_integer, haversine_long_fractional;
    haversine_long_integer = sub_gps(1, 0, cos_long_diff_integer, cos_long_diff_fractional, &haversine_long_fractional);
    // printk(KERN_INFO "haversine_long_integer: %d, haversine_long_fractional: %d\n", haversine_long_integer, haversine_long_fractional);

    int cos_cos_hav_integer, cos_cos_hav_fractional;
    cos_cos_hav_integer = mul_gps(cos_cos_integer, cos_cos_fractional, haversine_long_integer, haversine_long_fractional, &cos_cos_hav_fractional);
    // printk(KERN_INFO "cos_cos_hav_integer: %d, cos_cos_hav_fractional: %d\n", cos_cos_hav_integer, cos_cos_hav_fractional);

    int haversine_all_integer, haversine_all_fractional;    //2y
    haversine_all_integer = add_gps(cos_cos_hav_integer, cos_cos_hav_fractional, haversine_lat_integer, haversine_lat_fractional, &haversine_all_fractional);
    // printk(KERN_INFO "haversine_all_integer: %d, haversine_all_fractional: %d\n", haversine_all_integer, haversine_all_fractional);

    int acos_arg_integer, acos_arg_fractional;    //1-2y
    acos_arg_integer = sub_gps(1, 0, haversine_all_integer, haversine_all_fractional, &acos_arg_fractional);
    // printk(KERN_INFO "acos_arg_integer: %d, acos_arg_fractional: %d\n", acos_arg_integer, acos_arg_fractional);

    int degree_dist_integer, degree_dist_fractional;
    degree_dist_integer = acos_gps(acos_arg_integer, acos_arg_fractional, &degree_dist_fractional);
    // printk(KERN_INFO "degree_dist_integer: %d, degree_dist_fractional: %d\n", degree_dist_integer, degree_dist_fractional);

    int dist_integer, dist_fractional;
    dist_integer = mul_gps(degree_dist_integer, degree_dist_fractional, EARTH_RADIUS, 0, &dist_fractional);
    // printk(KERN_INFO "dist_integer: %d, dist_fractional: %d\n", dist_integer, dist_fractional);

    int acc_curr, acc_file;
    acc_curr = curr_loc.accuracy;
    acc_file = file_gps->accuracy;
    // printk(KERN_INFO "accuracy of file: %d, accuracy of latest location: %d\n", acc_file, acc_curr);
    // printk(KERN_INFO "Calculated distance: %d.%06d\n", dist_integer, dist_fractional);
    if (acc_curr + acc_file > dist_integer)
    {
        printk(KERN_INFO "GPS location of the file is accessible.\n");
        return 0;
    }
    else if (acc_curr + acc_file == dist_integer && dist_fractional > 0)
    {
        printk(KERN_INFO "GPS location of the file is accessible.\n");
        return 0;
    }
    printk(KERN_INFO "GPS location of the file is NOT accessible.\n");
    return 1;
}

long sys_set_gps_location(struct gps_location __user *loc)
{
    struct gps_location *loc_kern = (struct gps_location *)kmalloc(sizeof(struct gps_location), GFP_KERNEL);
    if (!loc_kern)
    {
        printk(KERN_ALERT "sys_set_gps_location: kmalloc failed");
        return -1;
    }
    if (copy_from_user(loc_kern, loc, sizeof(struct gps_location)) != 0)
    {
        printk(KERN_ALERT "sys_set_gps_location: failed to copy from user\n");
        kfree(loc_kern);
        return -2;
    }
    if (loc_kern->lat_fractional > FRAC_MAX || loc_kern->lat_fractional < 0 || loc_kern->lng_fractional > FRAC_MAX || loc_kern->lng_fractional < 0 ||
        loc_kern->lat_integer > LAT_MAX || loc_kern->lat_integer < LAT_MIN || loc_kern->lng_integer > LNG_MAX || loc_kern->lng_integer < LNG_MIN ||
        loc_kern->accuracy < 0)
    {
        return -3;
    }
    if (loc_kern->lat_integer == LAT_MAX && loc_kern->lat_fractional)
    {
        return -4;
    }
    if (loc_kern->lng_integer == LNG_MAX && loc_kern->lng_fractional)
    {
        return -5;
    }
    spin_lock(&geo_lock);
    curr_loc.lat_integer = loc_kern->lat_integer;
    curr_loc.lat_fractional = loc_kern->lat_fractional;
    curr_loc.lng_integer = loc_kern->lng_integer;
    curr_loc.lng_fractional = loc_kern->lng_fractional;
    curr_loc.accuracy = loc_kern->accuracy;
    spin_unlock(&geo_lock);
    kfree(loc_kern);
    return 0;
}

long sys_get_gps_location(const char __user *pathname, struct gps_location __user *loc)
{
    int ret;
    char *buf_path;
    struct inode *inode;
    struct path path;
    struct gps_location *loc_kern;
    int path_len;
    path_len = (int)strnlen_user(pathname, PATH_LENGTH_MAX);
    if (pathname == NULL)
    {
        printk(KERN_ALERT "sys_get_gps_location: invalid path");
        return -1;
    }
    if (loc == NULL)
    {
        printk(KERN_ALERT "sys_get_gps_location: invalid gps_location");
        return -2;
    }
    if (path_len <= 0 || path_len > PATH_LENGTH_MAX)
    {
        printk(KERN_ALERT "sys_get_gps_location: failed to gen strnlen");
        return -3;
    }
    buf_path = (char *)kmalloc(sizeof(char) * (path_len + 1), GFP_KERNEL);
    if (!buf_path)
    {
        printk(KERN_ALERT "sys_get_gps_location: kmalloc failed");
        return -4;
    }
    if ((ret = strncpy_from_user(buf_path, pathname, path_len + 1) < 0))
    {
        kfree(buf_path);
        printk(KERN_ALERT "sys_get_gps_location: failed to do_strncpy_from_user\n");
        return -5;
    }
    buf_path[path_len] = '\0';

    if (kern_path(buf_path, LOOKUP_FOLLOW, &path))
    {
        kfree(buf_path);
        printk(KERN_ALERT "sys_get_gps_location: failed to kern_path\n");
        return -6;
    }
    kfree(buf_path);
    inode = path.dentry->d_inode;
    if (inode == NULL)
    {
        printk(KERN_ALERT "sys_get_gps_location: inode is null\n");
        return -7;
    }

    if (inode_permission(inode, MAY_READ))
    {
        printk(KERN_ALERT "sys_get_gps_location: no permission to read inode\n");
        return -EACCES;
    }
    loc_kern = (struct gps_location *)kmalloc(sizeof(struct gps_location), GFP_KERNEL);
    if (!loc_kern)
    {
        printk(KERN_ALERT "sys_get_gps_location: kmalloc failed");
        return -8;
    }
    if (copy_from_user(loc_kern, loc, sizeof(struct gps_location)))
    {
        printk(KERN_ALERT "sys_get_gps_location: failed to copy from user\n");
        kfree(loc_kern);
        return -9;
    }

    if (inode->i_op->get_gps_location == NULL)
    {
        printk(KERN_ALERT "sys_get_gps_location: get_gps_location not defined");
        kfree(loc_kern);
        return -ENODEV;
    }
    if (inode->i_op->get_gps_location(inode, loc_kern))
    {
        kfree(loc_kern);
        printk(KERN_ALERT "sys_get_gps_location: error from get_gps_location at inode");
        return -10;
    }
    if (copy_to_user(loc, loc_kern, sizeof(struct gps_location)))
    {
        kfree(loc_kern);
        printk(KERN_ALERT "sys_get_gps_location: copy_to_user failed\n");
        return -11;
    }
    kfree(loc_kern);
    return 0;
}

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

struct gps_location curr_loc = {
    .lat_integer = 0,
    .lat_fractional = 0,
    .lng_integer = 0,
    .lng_fractional = 0,
    .accuracy = 0,
};

int add_gps(int int1, int frac1, int int2, int frac2, int *frac)
{
    *frac = (frac1 + frac2) % FRAC_MAX;
    return int1 + int2 + (frac1 + frac2) / FRAC_MAX;
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
    return ret;
}

int mul_gps(int int1, int frac1, int int2, int frac2, int *frac)
{
    int ret;
    int hihi, hilo, lohi;
    s64 lolo; // 10^6 * 10^6 > 2^32
    hihi = int1 * int2;
    hilo = int1 * frac2;
    lohi = frac1 * int2;
    lolo = (s64)frac1 * frac2;
    *frac = hilo + lohi + (int)(lolo / FRAC_MAX);
    ret = lolo + (hilo + lohi) / FRAC_MAX;
    while (*frac > FRAC_MAX)
    {
        ret++;
        *frac -= FRAC_MAX;
    }
    while (*frac < 0)
    {
        ret--;
        *frac += FRAC_MAX;
    }

    return ret;
}

int deg2rad_gps(int deg_i, int deg_f, int *frac)
{

    return mul_gps(deg_i, deg_f, 0, 17453, frac);
}

int sin_gps(int x_i, int x_frac, int *ret_frac)
{
    int ret, rad_x_i, deg_x_i;
    int *rad_x_frac, *deg_x_frac;
    int add_term;
    int *add_term_frac;
    int frac;
    int i, j;
    int divisor;
    int sign;
    int is_negative;
    ret = 0;
    divisor = 1;

    if (x_i < 0)
    {
        is_negative = 1;
        deg_x_i = sub_gps(0, 0, x_i, x_frac, deg_x_frac);
    }
    else
    {
        is_negative = 0;
        deg_x_i = x_i;
        *deg_x_frac = x_frac;
    }

    if (x_i >= 90)
    {
        deg_x_i = sub_gps(180, 0, deg_x_i, *deg_x_frac, deg_x_frac);
    }
    rad_x_i = deg2rad_gps(deg_x_i, *deg_x_frac, rad_x_frac);
    ret = rad_x_i;
    *ret_frac = *rad_x_frac;

    j = 1;
    sign = 1;
    for (i = 3; i < 19; i += 2)
    {
        for (; j < i; j++)
        {
            frac = *rad_x_frac;
            divisor *= j;
            rad_x_i = mul_gps(rad_x_i, frac, rad_x_i, frac, rad_x_frac);
        }
        frac = *rad_x_frac;
        add_term = mul_gps(rad_x_i, frac, 0, FRAC_MAX / divisor, add_term_frac);
        sign = -sign;
        frac = *ret_frac;
        if (sign > 0)
        {
            ret = add_gps(ret, frac, add_term, *add_term_frac, ret_frac);
        }
        else
        {
            ret = sub_gps(ret, frac, add_term, *add_term_frac, ret_frac);
        }
    }
    if (is_negative)
    {
        frac = *ret_frac;
        ret = sub_gps(0, 0, ret, frac, ret_frac);
    }
    return ret;
}

int cos_gps(int x_i, int x_frac, int *ret_frac)
{
    int ret, rad_x_i, deg_x_i;
    int *rad_x_frac, *deg_x_frac;
    int add_term;
    int *add_term_frac;
    int frac;
    int i, j;
    int divisor;
    int sign;
    ret = 0;
    divisor = 1;

    if (x_i < 0)
    {
        deg_x_i = sub_gps(0, 0, x_i, x_frac, deg_x_frac);
    }
    else
    {
        deg_x_i = x_i;
        *deg_x_frac = x_frac;
    }

    rad_x_i = deg2rad_gps(deg_x_i, *deg_x_frac, rad_x_frac);
    ret = 1;
    *ret_frac = 0;

    j = 0;
    sign = 1;
    for (i = 2; i < 18; i += 2)
    {
        for (; j < i; j++)
        {
            frac = *rad_x_frac;
            divisor *= j;
            rad_x_i = mul_gps(rad_x_i, frac, rad_x_i, frac, rad_x_frac);
        }
        frac = *rad_x_frac;
        add_term = mul_gps(rad_x_i, frac, 0, FRAC_MAX / divisor, add_term_frac);
        sign = -sign;
        frac = *ret_frac;
        if (sign > 0)
        {
            ret = add_gps(ret, frac, add_term, *add_term_frac, ret_frac);
        }
        else
        {
            ret = sub_gps(ret, frac, add_term, *add_term_frac, ret_frac);
        }
    }

    return ret;
}

int acos_gps(int x_i, int x_frac, int *ret_frac)
{
    int pi_half_i;
    int pi_half_frac;
    // int dividends[6] = {-1, -3, -5, -35, 63};
    // int divisors[6] = {6, 40, 112, 1152, 2816};
    int coeffs[5] = {166667, 75000, 26786, 30382, 22372};
    int signs[5] = {-1, -1, -1, -1, 1};
    int coeff_length = 5;
    pi_half_i = 1;
    pi_half_frac = 570796;

    int ret, rad_x_i, deg_x_i;
    int *rad_x_frac, *deg_x_frac;
    int add_term;
    int *add_term_frac;
    int frac;
    int i, j;
    int coeff;
    int sign;
    ret = 0;
    coeff = 1;

    deg_x_i = x_i;
    *deg_x_frac = x_frac;

    rad_x_i = deg2rad_gps(deg_x_i, *deg_x_frac, rad_x_frac);
    ret = 1;
    *ret_frac = 570796;
    frac = *rad_x_frac;
    ret = sub_gps(ret, *ret_frac, rad_x_i, frac, ret_frac);

    j = 1;
    sign = 1;
    for (i = 3; i < 13; i += 2)
    {
        for (; j < i; j++)
        {
            frac = *rad_x_frac;
            rad_x_i = mul_gps(rad_x_i, frac, rad_x_i, frac, rad_x_frac);
        }
        coeff = coeffs[(i % 2) - 1];
        frac = *rad_x_frac;
        add_term = mul_gps(rad_x_i, frac, 0, coeff, add_term_frac);
        sign = signs[(i % 2) - 1];
        frac = *ret_frac;
        if (sign > 0)
        {
            ret = add_gps(ret, frac, add_term, *add_term_frac, ret_frac);
        }
        else
        {
            ret = sub_gps(ret, frac, add_term, *add_term_frac, ret_frac);
        }
    }
    return ret;
}

int is_accessible_loc(struct inode *inode)
{
    struct gps_location *file_gps;
    file_gps = (struct gps_location *)kmalloc(sizeof(struct gps_location), GFP_KERNEL);
    if (!file_gps)
    {
        printk(KERN_ALERT "is_accessible_loc: kmalloc failed");
        return -1;
    }
    if (!(inode->i_op->get_gps_location))
    {
        printk(KERN_ALERT "is_accessible_loc: get_gps_location not defined\n");
        return -ENODEV;
    }
    if (!(inode->i_op->get_gps_location(inode, file_gps)))
    {
        printk(KERN_ALERT "is_accessible_loc: error from get_gps_location at inode");
        return -1;
    }

    // calculate distance

    int theta_integer, theta_fractional;
    if(curr_loc.lng_integer > file_gps->lng_integer){
        theta_integer = sub_gps(curr_loc.lng_integer, curr_loc.lng_fractional, file_gps->lng_integer, file_gps->lng_fractional, &theta_fractional);
    } else if(curr_loc.lng_integer < file_gps->lng_integer){
        theta_integer = sub_gps(file_gps->lng_integer, file_gps->lng_fractional, curr_loc.lng_integer, curr_loc.lng_fractional, &theta_fractional);
    } else{
        theta_integer = (curr_loc.lng_fractional > file_gps->lng_fractional) ? 
            sub_gps(curr_loc.lng_integer, curr_loc.lng_fractional, file_gps->lng_integer, file_gps->lng_fractional, &theta_fractional): 
            sub_gps(file_gps->lng_integer, file_gps->lng_fractional, curr_loc.lng_integer, curr_loc.lng_fractional, &theta_fractional);
    }

    int cos_curr_loc_lat_integer, cos_curr_loc_lat_fractional;
    int cos_file_lat_integer, cos_file_lat_fractional;
    cos_curr_loc_lat_integer = cos_gps(curr_loc.lat_integer, curr_loc.lat_fractional, &cos_curr_loc_lat_fractional);
    cos_file_lat_integer = cos_gps(file_gps->lat_integer, file_gps->lat_fractional, &cos_file_lat_fractional);

    int sin_curr_loc_lat_integer, sin_curr_loc_lat_fractional;
    int sin_file_lat_integer, sin_file_lat_fractional;
    sin_curr_loc_lat_integer = sin_gps(curr_loc.lat_integer, curr_loc.lat_fractional, &sin_curr_loc_lat_fractional);
    sin_file_lat_integer = sin_gps(file_gps->lat_integer, file_gps->lat_fractional, &sin_file_lat_fractional);

    int cos_theta_integer, cos_theta_fractional;
    cos_theta_integer = cos_gps(theta_integer, theta_fractional, &cos_theta_fractional);

    int cos_cos_integer, cos_cos_fractional;
    cos_cos_integer = mul_gps(cos_curr_loc_lat_integer, cos_curr_loc_lat_fractional, cos_file_lat_integer, cos_file_lat_fractional, &cos_cos_fractional);

    int sin_sin_integer, sin_sin_fractional;
    sin_sin_integer = mul_gps(sin_curr_loc_lat_integer, sin_curr_loc_lat_fractional, sin_file_lat_integer, sin_file_lat_fractional, &sin_sin_fractional);

    int sin_sin_cos_integer, sin_sin_cos_fractional;
    sin_sin_cos_integer = mul_gps(sin_sin_integer, sin_sin_fractional, cos_theta_integer, cos_theta_fractional, &sin_sin_cos_fractional);

    int cos_dist_integer, cos_dist_fractional;
    cos_dist_integer = add_gps(cos_cos_integer, cos_cos_fractional, sin_sin_cos_integer, sin_sin_cos_fractional, &cos_dist_fractional);

    int degree_dist_integer, degree_dist_fractional;
    degree_dist_integer = acos_gps(cos_dist_integer, cos_dist_fractional, &degree_dist_fractional);

    int dist_integer, dist_fractional;
    dist_integer = mul_gps(degree_dist_integer, degree_dist_fractional, EARTH_RADIUS, 0, &dist_fractional);

    int acc_curr, acc_file;
    acc_curr = curr_loc.accuracy;
    acc_file = file_gps->accuracy;

    if(acc_curr + acc_file > dist_integer){
        printk(KERN_INFO "GPS location of the file is accessible.\n");
        return 1;
    } else if(acc_curr + acc_file == dist_integer && dist_fractional > 0){
        printk(KERN_INFO "GPS location of the file is accessible.\n");
        return 1;
    }
    printk(KERN_INFO "GPS location of the file is NOT accessible.\n");
    return 0;
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
        return -1;
    }
    if (loc_kern->lat_fractional > FRAC_MAX || loc_kern->lat_fractional < 0 || loc_kern->lng_fractional > FRAC_MAX || loc_kern->lng_fractional < 0 ||
        loc_kern->lat_integer > LAT_MAX || loc_kern->lat_integer < LAT_MIN || loc_kern->lng_integer > LNG_MAX || loc_kern->lng_integer < LNG_MIN || 
        loc_kern->accuracy < 0)
    {
        return -1;
    }
    if (loc_kern->lat_integer == LAT_MAX && loc_kern->lat_fractional)
    {
        return -1;
    }
    if (loc_kern->lng_integer == LNG_MAX && loc_kern->lng_fractional)
    {
        return -1;
    }

    curr_loc.lat_integer = loc_kern->lat_integer;
    curr_loc.lat_fractional = loc_kern->lat_fractional;
    curr_loc.lng_integer = loc_kern->lng_integer;
    curr_loc.lng_fractional = loc_kern->lng_fractional;
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
    if(loc == NULL){
        printk(KERN_ALERT "sys_get_gps_location: invalid gps_location");
        return -1;
    }
    if (path_len <= 0 || path_len > PATH_LENGTH_MAX)
    {
        printk(KERN_ALERT "sys_get_gps_location: failed to gen strnlen");
        return -1;
    }
    buf_path = (char *)kmalloc(sizeof(char) * (path_len + 1), GFP_KERNEL);
    if (!buf_path)
    {
        printk(KERN_ALERT "sys_get_gps_location: kmalloc failed");
        return -1;
    }
    if ((ret = strncpy_from_user(buf_path, pathname, path_len + 1) < 0))
    {
        kfree(buf_path);
        printk(KERN_ALERT "sys_get_gps_location: failed to do_strncpy_from_user\n");
        return -1;
    }
    buf_path[path_len] = '\0';

    if (kern_path(buf_path, LOOKUP_FOLLOW, &path))
    {
        kfree(buf_path);
        printk(KERN_ALERT "sys_get_gps_location: failed to kern_path\n");
        return -1;
    }
    kfree(buf_path);
    inode = path.dentry->d_inode;
    if (inode == NULL)
    {
        printk(KERN_ALERT "sys_get_gps_location: inode is null\n");
        return -1;
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
        return -1;
    }
    if (copy_from_user(loc_kern, loc, sizeof(struct gps_location)))
    {
        printk(KERN_ALERT "sys_get_gps_location: failed to copy from user\n");
        kfree(loc_kern);
        return -1;
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
        return -1;
    }
    if (copy_to_user(loc, loc_kern, sizeof(struct gps_location)))
    {
        kfree(loc_kern);
        printk(KERN_ALERT "sys_get_gps_location: copy_to_user failed\n");
        return -1;
    }
    kfree(loc_kern);
    return 0;
}

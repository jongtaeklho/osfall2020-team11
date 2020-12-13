#include <linux/gps.h>
#include <linux/slab.h>

#define FRAC_MAX 1000000L
#define LAT_MAX 90
#define LAT_MIN -90
#define LNG_MAX 180
#define LNG_MIN -180

static struct gps_location curr_loc = {
    .lat_integer = 0,
    .lat_fractional = 0,
    .lng_integer = 0,
    .lng_fractional = 0,
    .accuracy = 0,
    };

long sys_set_gps_location(struct gps_location __user *loc)
{
    struct gps_location *temp = (struct gps_location *)kmalloc(sizeof(struct gps_location), GFP_KERNEL);
    if (copy_from_user(temp, loc, sizeof(struct gps_location)) != 0)
    {
        printk(KERN_ALERT "set_gps_location: failed to copy from user\n");
        kfree(temp);
        return -1;
    }
    if (temp->lat_fractional > FRAC_MAX || temp->lat_fractional < 0 || temp->lng_fractional > FRAC_MAX || temp->lng_fractional < 0 ||
        temp->lat_integer > LAT_MAX || temp->lat_integer < LAT_MIN || temp->lng_integer > LNG_MAX || temp->lng_integer < LNG_MIN || temp->accuracy < 0)
    {
        return -1;
    }
    if (temp->lat_integer == LAT_MAX && temp->lat_fractional)
    {
        return -1;
    }
    if (temp->lng_integer == LNG_MAX && temp->lng_fractional)
    {
        return -1;
    }

    curr_loc.lat_integer = temp->lat_integer;
    curr_loc.lat_fractional = temp->lat_fractional;
    curr_loc.lng_integer = temp->lng_integer;
    curr_loc.lng_fractional = temp->lng_fractional;
    kfree(temp);
    return 0;
}

long sys_get_gps_location(const char __user *pathname, struct gps_location __user *loc)
{
    
}
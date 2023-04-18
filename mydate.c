#include "types.h"
#include "stat.h"
#include "user.h"
#include "date.h"

int main(int argc, char *argv[])
{
    struct rtcdate *dat = (struct rtcdate *)malloc(sizeof(struct rtcdate));
    if (date(dat))
    {
        printf(2, "Illegal pointer to rtcdate object");
        exit();
    }
    printf(1, "Year: %d\nMonth: %d\nDay: %d\nHour: %d\nMinute: %d\nSecond: %d\n", dat->year, dat->month, dat->day, dat->hour, dat->minute, dat->second);
    exit();
}
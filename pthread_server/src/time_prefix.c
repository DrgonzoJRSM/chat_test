#include "../headers/time_prefix.h"

void print_time_prefix(void) {
    time_t mytime = time(NULL);
    struct tm *now = localtime(&mytime);

    printf("[%02d:%02d:%02d]    ", now->tm_hour, now->tm_min, now->tm_sec);
}
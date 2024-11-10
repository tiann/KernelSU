#include <unistd.h>
#include <stdlib.h>
#include <sys/prctl.h>

// This is a simple example. If you want a full-featured "su", please use "/data/adb/ksud debug su".
int main(){
    int32_t result = 0;
    prctl(0xdeadbeef, 0, 0, 0, &result);
    system("/system/bin/sh");
    return 0;
}

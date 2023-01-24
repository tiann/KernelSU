//
// Created by Thom on 2019/3/8.
//

// Credits: https://github.com/brevent/genuine/blob/master/src/main/jni/apk-sign-v2.c
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

static bool isApkSigBlock42(const char *buffer) {
    // APK Sig Block 42
    return *buffer == 'A'
           && *++buffer == 'P'
           && *++buffer == 'K'
           && *++buffer == ' '
           && *++buffer == 'S'
           && *++buffer == 'i'
           && *++buffer == 'g'
           && *++buffer == ' '
           && *++buffer == 'B'
           && *++buffer == 'l'
           && *++buffer == 'o'
           && *++buffer == 'c'
           && *++buffer == 'k'
           && *++buffer == ' '
           && *++buffer == '4'
           && *++buffer == '2';
}

int get_signature(const char *path, unsigned int* out_size, unsigned int* out_hash) {
    unsigned char buffer[0x11] = {0};
    uint32_t size4;
    uint64_t size8, size_of_block;
    int sign = -1;
    int fd = (int) openat(AT_FDCWD, path, O_RDONLY);
    if (fd < 0) {
        return sign;
    }
    sign = 1;
    // https://en.wikipedia.org/wiki/Zip_(file_format)#End_of_central_directory_record_(EOCD)
    for (int i = 0;; ++i) {
        unsigned short n;
        lseek(fd, -i - 2, SEEK_END);
        read(fd, &n, 2);
        if (n == i) {
            lseek(fd, -22, SEEK_CUR);
            read(fd, &size4, 4);
            if ((size4 ^ 0xcafebabeu) == 0xccfbf1eeu) {
                if (i > 0) {
                    printf("warning: comment length is %d\n", i);
                }
                break;
            }
        }
        if (i == 0xffff) {
            printf("error: cannot find eocd\n");
            goto clean;
        }
    }

    lseek(fd, 12, SEEK_CUR);
    // offset
    read(fd, &size4, 0x4);
    lseek(fd, (off_t) (size4 - 0x18), SEEK_SET);

    read(fd, &size8, 0x8);
    read(fd, buffer, 0x10);
    if (!isApkSigBlock42((char *) buffer)) {
        goto clean;
    }

    lseek(fd, (off_t) (size4 - (size8 + 0x8)), SEEK_SET);
    read(fd, &size_of_block, 0x8);
    if (size_of_block != size8) {
        goto clean;
    }

    for (;;) {
        uint32_t id;
        uint32_t offset;
        read(fd, &size8, 0x8); // sequence length
        if (size8 == size_of_block) {
            break;
        }
        read(fd, &id, 0x4); // id
        offset = 4;
        if ((id ^ 0xdeadbeefu) == 0xafa439f5u || (id ^ 0xdeadbeefu) == 0x2efed62f) {
            read(fd, &size4, 0x4); // signer-sequence length
            read(fd, &size4, 0x4); // signer length
            read(fd, &size4, 0x4); // signed data length
            offset += 0x4 * 3;

            read(fd, &size4, 0x4); // digests-sequence length
            lseek(fd, (off_t) (size4), SEEK_CUR);// skip digests
            offset += 0x4 + size4;

            read(fd, &size4, 0x4); // certificates length
            read(fd, &size4, 0x4); // certificate length
            offset += 0x4 * 2;

            int hash = 1;
            signed char c;
            for (unsigned i = 0; i < size4; ++i) {
                read(fd, &c, 0x1);
                hash = 31 * hash + c;
            }
            offset += size4;
            // printf("    size: 0x%04x, hash: 0x%08x\n", size4, ((unsigned) hash) ^ 0x14131211u);
            printf("size = 0x%04x = %u\n", size4, size4);
            printf("hash = 0x%08x = %u\n", ((unsigned) hash) ^ 0x14131211u, ((unsigned) hash) ^ 0x14131211u);
            *out_size = size4;
            *out_hash = ((unsigned) hash) ^ 0x14131211u;

        }
        lseek(fd, (off_t) (size8 - offset), SEEK_CUR);
    }

clean:
    close(fd);

    return sign;
}

int set_kernel_param(unsigned int size, unsigned int hash){
    if (access("/sys/module/kernelsu", F_OK) != 0) {
        return 0;
    }
    if (size == 0 || hash == 0) {
        return 0;
    }
    printf("before:\n");
    system("printf 'ksu_expected_size: ';cat /sys/module/kernelsu/parameters/ksu_expected_size");
    system("printf 'ksu_expected_hash: ';cat /sys/module/kernelsu/parameters/ksu_expected_hash");
    char buf[128];
    snprintf(buf, 128, "echo %u > /sys/module/kernelsu/parameters/ksu_expected_size", size);
    system(buf);
    snprintf(buf, 128, "echo %u > /sys/module/kernelsu/parameters/ksu_expected_hash", hash);
    system(buf);
    printf("after:\n");
    system("printf 'ksu_expected_size: ';cat /sys/module/kernelsu/parameters/ksu_expected_size");
    system("printf 'ksu_expected_hash: ';cat /sys/module/kernelsu/parameters/ksu_expected_hash");
    return 1;
}
#include <stdint.h>
#include <syscalls.h>
#include <testUtil.h>
#include <userstdlib.h>

// Random
static uint32_t m_z = 362436069;
static uint32_t m_w = 521288629;

uint32_t
getUint() {
    m_z = 36969 * (m_z & 65535) + (m_z >> 16);
    m_w = 18000 * (m_w & 65535) + (m_w >> 16);
    return (m_z << 16) + m_w;
}

uint32_t
getUniform(uint32_t max) {
    uint32_t u = getUint();
    return (u + 1.0) * 2.328306435454494e-10 * max;
}

// Memory
uint8_t
memcheck(void *start, uint8_t value, uint32_t size) {
    uint8_t *p = (uint8_t *) start;
    uint32_t i;

    for (i = 0; i < size; i++, p++)
        if (*p != value) {
            fprintf(STDERR, "[memcheck]at %x found %d expected %d.", (unsigned int) (size_t) p, (int) *p, (int) value);
            return 0;
        }

    return 1;
}

// Parameters
int64_t
satoi(char *str) {
    uint64_t i = 0;
    int64_t res = 0;
    int8_t sign = 1;

    if (!str)
        return 0;

    if (str[i] == '-') {
        i++;
        sign = -1;
    }

    for (; str[i] != '\0'; ++i) {
        if (str[i] < '0' || str[i] > '9')
            return 0;
        res = res * 10 + str[i] - '0';
    }

    return res * sign;
}

// Dummies
void
bussyWait(uint64_t n) {
    uint64_t i;
    for (i = 0; i < n; i++)
        ;
}

void
endlessLoop(int argc, char *argv[]) {
    while (1)
        ;
}

void
endlessLoopPrint(int argc, char *argv[]) {
    Pid pid = sys_getpid();

    while (1) {
        printf("%d ", pid);
        bussyWait(9000000);
    }
}

void *
memsetTest(void *destiation, int32_t c, size_t length) {
    uint8_t chr = (uint8_t) c;
    char *dst = (char *) destiation;
    while (length--) {
        dst[length] = chr;
    }
    return destiation;
}
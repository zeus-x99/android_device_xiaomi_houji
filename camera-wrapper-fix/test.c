typedef unsigned long long u64;
typedef unsigned int u32;
extern void run_stub(void *);
static unsigned char buf[1024] __attribute__((aligned(16)));
static unsigned char expected[1024];

static int check(const u64 *values, u32 count) {
    for (u32 i = 0; i < sizeof(buf); ++i) buf[i] = expected[i] = 0xa5;
    *(u32 *)(buf + 0x10) = count;
    for (u32 i = 0; i < count; ++i) *(u64 *)(buf + 0x38 + i * 0x28) = values[i];
    for (u32 i = 0; i < sizeof(buf); ++i) expected[i] = buf[i];
    for (u32 i = 0; i < count; ++i) {
        for (u32 j = 0; j < i; ++j) {
            if (values[i] == values[j]) *(u64 *)(expected + 0x38 + i * 0x28) = 0;
        }
    }
    run_stub(buf);
    for (u32 i = 0; i < sizeof(buf); ++i) if (buf[i] != expected[i]) return 1;
    return 0;
}

void _start(void) {
    u64 values[16];
    int failed = 0;
    for (u32 count = 0; count <= 16; ++count) {
        for (u32 i = 0; i < 16; ++i) values[i] = 0;
        failed |= check(values, count);
        for (u32 i = 0; i < 16; ++i) values[i] = 0x1000 + i * 0x100;
        failed |= check(values, count);
        for (u32 i = 0; i < 16; ++i) values[i] = 0xb400000000001000ULL;
        failed |= check(values, count);
        for (u32 i = 0; i < 16; ++i) values[i] = (i % 4) * 0x1000;
        failed |= check(values, count);
    }
    register long code __asm__("x0") = failed;
    register long call __asm__("x8") = 93;
    __asm__ volatile("svc #0" : : "r"(code), "r"(call) : "memory");
    __builtin_unreachable();
}

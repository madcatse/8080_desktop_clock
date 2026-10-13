/* Run actual compiled 8080 instructions using the bundled CPU core.
   RTC test fixture + LCD write capture only; no BME280/1-Wire model. */
#include "i8080.h"
#include <stdlib.h>
#include <string.h>

static unsigned char memory[65536], expected[32768];
static unsigned short pixels[480 * 320];
static unsigned int rom_writes, rtc_reads, pixel_writes, bad_pixels;
static unsigned int cmd, index_, sx, ex, sy, ey, x, y, color;
static i8080 cpu;
static unsigned char rb(void *u, uint16_t a) { return memory[a]; }
static void wb(void *u, uint16_t a, uint8_t v) {
    if (a < 0x8000) rom_writes++; else memory[a] = v;
}
static unsigned char pi(void *u, uint8_t p) {
    /* 12 October 2026, Monday, 12:34:40 + CPU elapsed seconds. */
    static const unsigned char rtc[] = {0x40, 0x34, 0x12, 0x12, 0x01, 0x10, 0x26};
    if (p >= 0x30 && p <= 0x36) {
        rtc_reads++;
        if (p == 0x30) {
            unsigned int sec = (40 + cpu.cyc / 3555555UL) % 60;
            return (sec / 10) * 16 + sec % 10;
        }
        return rtc[p - 0x30];
    }
    return 255; /* released keys, absent sensors */
}
static void po(void *u, uint8_t p, uint8_t v) {
    if (p == 0x10) {
        cmd = v; index_ = 0;
        if (cmd == 0x2c) { x = sx; y = sy; }
    } else if (p == 0x11) {
        index_++;
        if (cmd == 0x2a || cmd == 0x2b) {
            unsigned int *start = cmd == 0x2a ? &sx : &sy;
            unsigned int *end = cmd == 0x2a ? &ex : &ey;
            if (index_ == 1) *start = v << 8;
            if (index_ == 2) *start |= v;
            if (index_ == 3) *end = v << 8;
            if (index_ == 4) *end |= v;
        } else if (cmd == 0x2c) {
            if (index_ & 1) color = v << 8;
            else {
                color |= v;
                if (x < 480 && y < 320) pixels[x + y * 480] = color;
                else bad_pixels++;
                pixel_writes++;
                if (++x > ex) { x = sx; y++; }
            }
        }
    }
}
static size_t load(const char *path, unsigned char *buf, size_t limit) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(2); }
    size_t n = fread(buf, 1, limit, f);
    if (fgetc(f) != EOF) { fprintf(stderr, "Image too large\n"); exit(2); }
    fclose(f); return n;
}
int main(int argc, char **argv) {
    if (argc != 7) return 2;
    load(argv[1], memory, 32768);
    size_t n = load(argv[2], expected, sizeof expected);
    unsigned int data = strtoul(argv[3], NULL, 0), main_pc = strtoul(argv[4], NULL, 0);
    unsigned long cycles = strtoul(argv[5], NULL, 0);
    i8080_init(&cpu);
    cpu.read_byte = rb; cpu.write_byte = wb; cpu.port_in = pi; cpu.port_out = po;
    while (cpu.pc != main_pc && cpu.cyc < 20000000UL) i8080_step(&cpu);
    if (data + n > 65536 || cpu.pc != main_pc || memcmp(memory + data, expected, n)) {
        fprintf(stderr, "FAIL: CRT / decompressed DATA mismatch\n"); return 1;
    }
    printf("CRT OK: %zu DATA bytes at %04X, SP=%04X, cycles=%lu\n", n, data, cpu.sp, cpu.cyc);
    while (cpu.cyc < cycles && !cpu.halted && !rom_writes) i8080_step(&cpu);
    if (cycles) {
        FILE *f = fopen(argv[6], "wb"); if (!f) return 2;
        fprintf(f, "P6\n480 320\n255\n");
        for (unsigned int i = 0; i < 480 * 320; i++) {
            unsigned int c = pixels[i];
            fputc(((c >> 11) & 31) * 255 / 31, f);
            fputc(((c >> 5) & 63) * 255 / 63, f);
            fputc((c & 31) * 255 / 31, f);
        }
        fclose(f);
        printf("LCD pixels=%u, outside_screen=%u, RTC reads=%u, ROM writes=%u, cycles=%lu\n",
               pixel_writes, bad_pixels, rtc_reads, rom_writes, cpu.cyc);
        if (pixel_writes < 153600 || !rtc_reads || cpu.halted) return 1;
    }
    return rom_writes ? 1 : 0;
}

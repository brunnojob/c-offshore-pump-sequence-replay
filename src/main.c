#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

static void fail(const char *message) { fprintf(stderr, "%s\n", message); exit(2); }
double number(const char *s) {
    char *end; errno = 0; double value = strtod(s, &end);
    if (errno || end == s || *end || !isfinite(value)) fail("invalid finite number");
    return value;
}
static size_t fields(char *line, char **out, size_t capacity) {
    line[strcspn(line, "\r\n")] = 0;
    size_t count = 0;
    char *p = line;
    while (*p) {
        if (count == capacity) fail("too many fields");
        if (*p == ',') fail("empty field");
        out[count++] = p;
        char *separator = strchr(p, ',');
        if (!separator) break;
        *separator = 0; p = separator + 1;
        if (!*p) fail("empty field");
    }
    return count;
}

typedef struct { int state, duty, latched[2]; double entered, previous; } Controller;
static void step(Controller *c, double time, int demand, const int *running, const int *trip, int reset) {
    if (time < 0 || time < c->previous) fail("non-monotonic clock");
    c->previous = time;
    for (int i = 0; i < 2; i++) { if (trip[i]) c->latched[i] = 1; else if (reset) c->latched[i] = 0; }
    if (c->state && c->latched[c->duty]) { c->state = 0; c->duty ^= 1; }
    if (!c->state && demand) {
        if (c->latched[c->duty]) c->duty ^= 1;
        if (!c->latched[c->duty]) { c->state = 1; c->entered = time; }
    }
    if (c->state == 1 && running[c->duty]) { c->state = 2; c->entered = time; }
    if (c->state == 1 && time - c->entered >= 3) { c->latched[c->duty] = 1; c->state = 0; c->duty ^= 1; }
    if (c->state == 2 && !running[c->duty]) { c->latched[c->duty] = 1; c->state = 0; c->duty ^= 1; }
    if (c->state == 2 && !demand && time - c->entered >= 5) { c->state = 0; c->duty ^= 1; }
}
static int bit(const char *s) { double x = number(s); if (x != 0 && x != 1) fail("expected boolean"); return (int)x; }
int main(int argc, char **argv) {
    if (argc > 1 && !(argc == 2 && !strcmp(argv[1], "--self-test"))) fail("unsupported command arguments");
    if (argc == 2 && !strcmp(argv[1], "--self-test")) {
        Controller c = {0}; int run[] = {0, 0}, trip[] = {0, 0};
        step(&c, 0, 1, run, trip, 0); assert(c.state == 1); step(&c, 3, 1, run, trip, 0); assert(c.latched[0] && !c.state);
        step(&c, 4, 1, run, trip, 0); assert(c.state == 1 && c.duty == 1); run[1] = 1; step(&c, 5, 1, run, trip, 0);
        step(&c, 6, 0, run, trip, 0); assert(c.state == 2); step(&c, 10, 0, run, trip, 0); assert(!c.state);
        puts("start failure, changeover and minimum runtime tests passed"); return 0;
    }
    Controller c = {0}; char line[512]; unsigned rows = 0;
    while (fgets(line, sizeof line, stdin)) {
        if (strlen(line) == sizeof line - 1 && line[sizeof line - 2] != '\n') fail("input record too large");
        char *f[7]; if (fields(line, f, 7) != 7) fail("expected seven columns");
        int run[] = {bit(f[2]), bit(f[3])}, trip[] = {bit(f[4]), bit(f[5])};
        step(&c, number(f[0]), bit(f[1]), run, trip, bit(f[6]));
        printf("time=%.2f state=%d duty=%d command=%d%d faults=%d%d\n", c.previous, c.state, c.duty + 1, c.state && c.duty == 0, c.state && c.duty == 1, c.latched[0], c.latched[1]); rows++;
    }
    if (!rows || ferror(stdin)) fail("missing input");
    return 0;
}

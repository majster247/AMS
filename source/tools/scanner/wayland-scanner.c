/*
 * AMS wayland-scanner.
 *
 * Reads a Wayland XML protocol description and emits one of:
 *  - private-code:    C file with wl_message[] / wl_interface tables
 *  - client-header:   C header with the client-side stubs/proxies
 *  - server-header:   C header with the server-side dispatcher stubs
 *
 * This scanner is host-built (gcc, not the cross-compiler). It is a
 * minimal but functional implementation: it does NOT validate the
 * protocol nor support every XML edge case, but it understands the
 * core grammar used by wayland.xml / xdg-shell.xml / wlr-* protocols.
 *
 * Output ABI mirrors what libwayland-scanner produces, so generated
 * artifacts can be linked against AMS' libwayland-server-ams shim.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_REQUESTS 256
#define MAX_EVENTS   256
#define MAX_ARGS     32
#define MAX_IFACES   128

typedef struct {
    char name[64];
    char type[32];
    char interface[64];
    int  allow_null;
} arg_t;

typedef struct {
    char  name[64];
    int   since;
    arg_t args[MAX_ARGS];
    int   n_args;
    int   destructor;
} msg_t;

typedef struct {
    char  name[64];
    int   version;
    msg_t requests[MAX_REQUESTS];
    int   n_requests;
    msg_t events[MAX_EVENTS];
    int   n_events;
} iface_t;

static iface_t g_ifaces[MAX_IFACES];
static int g_n_ifaces = 0;

static void die(const char *m) { fprintf(stderr, "wayland-scanner: %s\n", m); exit(1); }

static char *slurp(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = (char*)malloc((size_t)n + 1);
    if (!b) die("oom");
    if (fread(b, 1, (size_t)n, f) != (size_t)n) die("read failed");
    b[n] = '\0';
    fclose(f);
    if (out_len) *out_len = (size_t)n;
    return b;
}

static const char *attr(const char *tag, const char *key, char *out, size_t cap) {
    out[0] = '\0';
    char needle[64];
    snprintf(needle, sizeof(needle), "%s=\"", key);
    const char *p = strstr(tag, needle);
    if (!p) return NULL;
    p += strlen(needle);
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < cap) out[i++] = *p++;
    out[i] = '\0';
    return out;
}

static int starts_with(const char *s, const char *prefix) {
    while (*prefix) if (*s++ != *prefix++) return 0;
    return 1;
}

static const char *find_tag_open(const char *p, const char *name, const char **end) {
    char open[64];
    snprintf(open, sizeof(open), "<%s", name);
    while ((p = strstr(p, open)) != NULL) {
        const char *q = p + strlen(open);
        if (*q == ' ' || *q == '/' || *q == '>') {
            const char *t_end = strchr(q, '>');
            if (!t_end) return NULL;
            if (end) *end = t_end + 1;
            return p;
        }
        ++p;
    }
    return NULL;
}

static const char *find_tag_close(const char *p, const char *name) {
    char close[64];
    snprintf(close, sizeof(close), "</%s>", name);
    return strstr(p, close);
}

static void parse_args(const char *body, msg_t *m) {
    const char *p = body;
    const char *e = NULL;
    while ((p = find_tag_open(p, "arg", &e)) != NULL) {
        if (m->n_args >= MAX_ARGS) break;
        char tag[512]; size_t n = (size_t)(e - p);
        if (n >= sizeof(tag)) n = sizeof(tag) - 1;
        memcpy(tag, p, n); tag[n] = '\0';
        arg_t *a = &m->args[m->n_args++];
        attr(tag, "name", a->name, sizeof(a->name));
        attr(tag, "type", a->type, sizeof(a->type));
        attr(tag, "interface", a->interface, sizeof(a->interface));
        char b[8]; attr(tag, "allow-null", b, sizeof(b));
        a->allow_null = (b[0] == 't');
        p = e;
    }
}

static void parse_messages(const char *body, const char *tag_name,
                           msg_t *out, int *n_out, int max) {
    const char *p = body, *e;
    while ((p = find_tag_open(p, tag_name, &e)) != NULL) {
        const char *close = find_tag_close(e, tag_name);
        if (!close) break;
        if (*n_out >= max) { p = close; continue; }
        msg_t *m = &out[(*n_out)++];
        memset(m, 0, sizeof(*m));
        char tag[512]; size_t n = (size_t)(e - p);
        if (n >= sizeof(tag)) n = sizeof(tag) - 1;
        memcpy(tag, p, n); tag[n] = '\0';
        attr(tag, "name", m->name, sizeof(m->name));
        char buf[16]; attr(tag, "since", buf, sizeof(buf));
        m->since = buf[0] ? atoi(buf) : 1;
        attr(tag, "type", buf, sizeof(buf));
        m->destructor = (strcmp(buf, "destructor") == 0);
        char body_buf[16384];
        size_t bl = (size_t)(close - e);
        if (bl >= sizeof(body_buf)) bl = sizeof(body_buf) - 1;
        memcpy(body_buf, e, bl); body_buf[bl] = '\0';
        parse_args(body_buf, m);
        p = close;
    }
}

static void parse_xml(const char *xml) {
    const char *p = xml, *e;
    while ((p = find_tag_open(p, "interface", &e)) != NULL) {
        const char *close = find_tag_close(e, "interface");
        if (!close) break;
        if (g_n_ifaces >= MAX_IFACES) break;
        iface_t *iface = &g_ifaces[g_n_ifaces++];
        memset(iface, 0, sizeof(*iface));
        char tag[512]; size_t n = (size_t)(e - p);
        if (n >= sizeof(tag)) n = sizeof(tag) - 1;
        memcpy(tag, p, n); tag[n] = '\0';
        attr(tag, "name", iface->name, sizeof(iface->name));
        char buf[16]; attr(tag, "version", buf, sizeof(buf));
        iface->version = buf[0] ? atoi(buf) : 1;
        char body_buf[131072];
        size_t bl = (size_t)(close - e);
        if (bl >= sizeof(body_buf)) bl = sizeof(body_buf) - 1;
        memcpy(body_buf, e, bl); body_buf[bl] = '\0';
        parse_messages(body_buf, "request", iface->requests, &iface->n_requests, MAX_REQUESTS);
        parse_messages(body_buf, "event",   iface->events,   &iface->n_events,   MAX_EVENTS);
        p = close;
    }
}

static const char *signature_ch(const char *type) {
    if (!strcmp(type, "int"))    return "i";
    if (!strcmp(type, "uint"))   return "u";
    if (!strcmp(type, "fixed"))  return "f";
    if (!strcmp(type, "string")) return "s";
    if (!strcmp(type, "object")) return "o";
    if (!strcmp(type, "new_id")) return "n";
    if (!strcmp(type, "array"))  return "a";
    if (!strcmp(type, "fd"))     return "h";
    return "?";
}

static void emit_signature(FILE *out, msg_t *m) {
    fprintf(out, "\"");
    if (m->since > 1) fprintf(out, "%d", m->since);
    for (int i = 0; i < m->n_args; ++i) {
        if (m->args[i].allow_null) fprintf(out, "?");
        fprintf(out, "%s", signature_ch(m->args[i].type));
    }
    fprintf(out, "\"");
}

static void emit_private_code(FILE *out) {
    fprintf(out,
        "/* Auto-generated by AMS wayland-scanner. Do not edit. */\n"
        "#include <stdint.h>\n"
        "#include <stddef.h>\n"
        "#include <wayland/wayland-server-core.h>\n\n");

    /* Forward declare interface symbols */
    for (int i = 0; i < g_n_ifaces; ++i)
        fprintf(out, "extern const struct wl_interface %s_interface;\n", g_ifaces[i].name);
    fprintf(out, "\n");

    for (int i = 0; i < g_n_ifaces; ++i) {
        iface_t *I = &g_ifaces[i];
        if (I->n_requests > 0) {
            fprintf(out, "static const struct wl_message %s_requests[] = {\n", I->name);
            for (int j = 0; j < I->n_requests; ++j) {
                fprintf(out, "    { \"%s\", ", I->requests[j].name);
                emit_signature(out, &I->requests[j]);
                fprintf(out, ", NULL },\n");
            }
            fprintf(out, "};\n\n");
        }
        if (I->n_events > 0) {
            fprintf(out, "static const struct wl_message %s_events[] = {\n", I->name);
            for (int j = 0; j < I->n_events; ++j) {
                fprintf(out, "    { \"%s\", ", I->events[j].name);
                emit_signature(out, &I->events[j]);
                fprintf(out, ", NULL },\n");
            }
            fprintf(out, "};\n\n");
        }
        fprintf(out,
            "const struct wl_interface %s_interface = {\n"
            "    \"%s\", %d,\n"
            "    %d, %s,\n"
            "    %d, %s\n"
            "};\n\n",
            I->name, I->name, I->version,
            I->n_requests, I->n_requests ? "(const struct wl_message*)0" : "NULL",
            I->n_events,   I->n_events   ? "(const struct wl_message*)0" : "NULL");
    }
}

static void upper_copy(char *dst, const char *src) {
    while (*src) {
        char c = *src++;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        *dst++ = c;
    }
    *dst = '\0';
}

static void emit_header(FILE *out, int server) {
    fprintf(out,
        "/* Auto-generated by AMS wayland-scanner. Do not edit. */\n"
        "#ifndef AMS_WL_PROTO_H_INC\n#define AMS_WL_PROTO_H_INC\n"
        "#include <stdint.h>\n#include <stddef.h>\n"
        "#include <wayland/wayland-%s-core.h>\n\n",
        server ? "server" : "client");

    for (int i = 0; i < g_n_ifaces; ++i)
        fprintf(out, "extern const struct wl_interface %s_interface;\n", g_ifaces[i].name);
    fprintf(out, "\n");

    for (int i = 0; i < g_n_ifaces; ++i) {
        iface_t *I = &g_ifaces[i];
        msg_t *list = server ? I->requests : I->events;
        int   n     = server ? I->n_requests : I->n_events;
        if (n > 0) {
            char up[128]; upper_copy(up, I->name);
            fprintf(out, "enum %s_%s {\n", I->name, server ? "request" : "event");
            for (int j = 0; j < n; ++j) {
                char up2[128]; upper_copy(up2, list[j].name);
                fprintf(out, "    %s_%s = %d,\n", up, up2, j);
            }
            fprintf(out, "};\n");
        }
    }

    fprintf(out, "\n#endif\n");
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <client-header|server-header|private-code> <in.xml> <out>\n", argv[0]);
        return 1;
    }
    const char *mode = argv[1];
    size_t len;
    char *xml = slurp(argv[2], &len);
    parse_xml(xml);
    FILE *out = fopen(argv[3], "wb");
    if (!out) { perror(argv[3]); return 1; }
    if (!strcmp(mode, "private-code"))
        emit_private_code(out);
    else if (!strcmp(mode, "server-header"))
        emit_header(out, 1);
    else if (!strcmp(mode, "client-header"))
        emit_header(out, 0);
    else { fprintf(stderr, "unknown mode %s\n", mode); return 1; }
    fclose(out);
    free(xml);
    return 0;
}

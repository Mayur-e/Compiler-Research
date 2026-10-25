/**
 * runtime_events.c
 *
 * Implementation of the concrete runtime event recorder.
 *
 * Events are stored in a static array and flushed to JSON on demand.
 * Thread-safety is intentionally omitted (single-threaded benchmarks only).
 *
 * Domain: CONCRETE RUNTIME — all values are real addresses / real data.
 * This is NOT static analysis output.
 */

#include "mdehra/runtime/runtime_events.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static RTEvent g_events[RT_MAX_EVENTS];
static int     g_event_count = 0;

/* JSON escaping for strings */
static void write_json_string(FILE* f, const char* s) {
    if (!s) { fprintf(f, "null"); return; }
    fprintf(f, "\"");
    for (; *s; s++) {
        switch (*s) {
            case '"':  fprintf(f, "\\\""); break;
            case '\\': fprintf(f, "\\\\"); break;
            case '\n': fprintf(f, "\\n");  break;
            case '\r': fprintf(f, "\\r");  break;
            case '\t': fprintf(f, "\\t");  break;
            default:   fprintf(f, "%c", *s); break;
        }
    }
    fprintf(f, "\"");
}

static const char* event_type_name(int type) {
    switch (type) {
        case RT_PROGRAM_START:  return "RT_PROGRAM_START";
        case RT_PROGRAM_END:    return "RT_PROGRAM_END";
        case RT_FUNCTION_ENTER: return "RT_FUNCTION_ENTER";
        case RT_FUNCTION_EXIT:  return "RT_FUNCTION_EXIT";
        case RT_MALLOC:         return "RT_MALLOC";
        case RT_FREE:           return "RT_FREE";
        case RT_PTR_ASSIGN:     return "RT_PTR_ASSIGN";
        case RT_FIELD_LOAD:     return "RT_FIELD_LOAD";
        case RT_FIELD_STORE:    return "RT_FIELD_STORE";
        case RT_BRANCH:         return "RT_BRANCH";
        case RT_LOOP_ITER:      return "RT_LOOP_ITER";
        case RT_NULL_CHECK:     return "RT_NULL_CHECK";
        case RT_RETURN:         return "RT_RETURN";
        default:                return "RT_UNKNOWN";
    }
}

static RTEvent* next_event(void) {
    if (g_event_count >= RT_MAX_EVENTS) return NULL;
    RTEvent* ev = &g_events[g_event_count++];
    memset(ev, 0, sizeof(RTEvent));
    return ev;
}

void rt_init(void) {
    g_event_count = 0;
    memset(g_events, 0, sizeof(g_events));
}

void rt_flush(const char* output_path) {
    FILE* f = fopen(output_path, "w");
    if (!f) {
        fprintf(stderr, "[runtime_events] ERROR: Cannot open output file: %s\n", output_path);
        return;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"domain\": \"CONCRETE_RUNTIME\",\n");
    fprintf(f, "  \"event_count\": %d,\n", g_event_count);
    fprintf(f, "  \"events\": [\n");

    for (int i = 0; i < g_event_count; i++) {
        const RTEvent* ev = &g_events[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"index\": %d,\n", i);
        fprintf(f, "      \"type\": \"%s\",\n", event_type_name(ev->type));
        fprintf(f, "      \"stmt_id\": %d,\n", ev->stmt_id);
        fprintf(f, "      \"stmt_line\": %d,\n", ev->stmt_line);
        fprintf(f, "      \"func_name\": "); write_json_string(f, ev->func_name); fprintf(f, ",\n");
        fprintf(f, "      \"var_name\": ");  write_json_string(f, ev->var_name);  fprintf(f, ",\n");
        fprintf(f, "      \"field_name\": ");write_json_string(f, ev->field_name);fprintf(f, ",\n");
        fprintf(f, "      \"addr\": \"0x%llx\",\n", (unsigned long long)ev->addr);
        fprintf(f, "      \"addr2\": \"0x%llx\",\n", (unsigned long long)ev->addr2);
        fprintf(f, "      \"size\": %zu,\n", ev->size);
        fprintf(f, "      \"alloc_site\": %d,\n", ev->alloc_site);
        fprintf(f, "      \"branch_taken\": %d,\n", ev->branch_taken);
        fprintf(f, "      \"loop_iter\": %d\n", ev->loop_iter);
        fprintf(f, "    }%s\n", (i + 1 < g_event_count) ? "," : "");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    fprintf(stderr, "[runtime_events] Flushed %d events to: %s\n",
            g_event_count, output_path);
}

/* ---- Event recording implementations ---- */

void rt_program_start(const char* func_name) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type      = RT_PROGRAM_START;
    ev->func_name = func_name;
}

void rt_program_end(void) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type = RT_PROGRAM_END;
}

void rt_function_enter(int line, const char* func_name) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type      = RT_FUNCTION_ENTER;
    ev->stmt_line = line;
    ev->func_name = func_name;
}

void rt_function_exit(int line, const char* func_name) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type      = RT_FUNCTION_EXIT;
    ev->stmt_line = line;
    ev->func_name = func_name;
}

void rt_malloc(int stmt_id, int line, int alloc_site,
               const char* func, const char* var,
               void* addr, size_t size) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type       = RT_MALLOC;
    ev->stmt_id    = stmt_id;
    ev->stmt_line  = line;
    ev->func_name  = func;
    ev->var_name   = var;
    ev->addr       = (uintptr_t)addr;
    ev->size       = size;
    ev->alloc_site = alloc_site;
}

void rt_free(int line, void* addr) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type      = RT_FREE;
    ev->stmt_line = line;
    ev->addr      = (uintptr_t)addr;
}

void rt_ptr_assign(int stmt_id, int line, const char* func,
                   const char* var, void* addr) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type      = RT_PTR_ASSIGN;
    ev->stmt_id   = stmt_id;
    ev->stmt_line = line;
    ev->func_name = func;
    ev->var_name  = var;
    ev->addr      = (uintptr_t)addr;
}

void rt_field_load(int stmt_id, int line, const char* func,
                   const char* dst_var, void* base_addr,
                   const char* field, void* result_addr) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type       = RT_FIELD_LOAD;
    ev->stmt_id    = stmt_id;
    ev->stmt_line  = line;
    ev->func_name  = func;
    ev->var_name   = dst_var;
    ev->field_name = field;
    ev->addr       = (uintptr_t)base_addr;
    ev->addr2      = (uintptr_t)result_addr;
}

void rt_field_store(int stmt_id, int line, const char* func,
                    void* base_addr, const char* field, void* value_addr) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type       = RT_FIELD_STORE;
    ev->stmt_id    = stmt_id;
    ev->stmt_line  = line;
    ev->func_name  = func;
    ev->field_name = field;
    ev->addr       = (uintptr_t)base_addr;
    ev->addr2      = (uintptr_t)value_addr;
}

void rt_branch(int stmt_id, int line, const char* func, int condition) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type         = RT_BRANCH;
    ev->stmt_id      = stmt_id;
    ev->stmt_line    = line;
    ev->func_name    = func;
    ev->branch_taken = (condition != 0) ? 1 : 0;
}

void rt_loop_iter(int stmt_id, int line, const char* func, int iter) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type      = RT_LOOP_ITER;
    ev->stmt_id   = stmt_id;
    ev->stmt_line = line;
    ev->func_name = func;
    ev->loop_iter = iter;
}

void rt_null_check(int stmt_id, int line, const char* func,
                   const char* var, void* addr) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type      = RT_NULL_CHECK;
    ev->stmt_id   = stmt_id;
    ev->stmt_line = line;
    ev->func_name = func;
    ev->var_name  = var;
    ev->addr      = (uintptr_t)addr;
}

void rt_return(int stmt_id, int line, const char* func,
               const char* var, void* addr) {
    RTEvent* ev = next_event();
    if (!ev) return;
    ev->type      = RT_RETURN;
    ev->stmt_id   = stmt_id;
    ev->stmt_line = line;
    ev->func_name = func;
    ev->var_name  = var;
    ev->addr      = (uintptr_t)addr;
}

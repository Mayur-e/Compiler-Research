/**
 * runtime_events.h
 *
 * C API for concrete runtime event recording.
 *
 * This header is included by instrumented benchmark programs.
 * It declares hooks for malloc, pointer assignment, field load/store,
 * branch, and loop events.
 *
 * ALL VALUES ARE CONCRETE RUNTIME VALUES.
 * This is SEPARATE from static analysis (HRA / Liveness / MDE).
 * Do NOT confuse concrete addresses with abstract HRA NodeIDs.
 *
 * Domain: CONCRETE RUNTIME
 *   - Real addresses (uintptr_t)
 *   - Real allocation sizes
 *   - Real branch outcomes
 *   - Real loop iteration counts
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * Runtime event types.
 * Kept as plain int for C89 compatibility in instrumented files.
 */
#define RT_PROGRAM_START   0
#define RT_PROGRAM_END     1
#define RT_FUNCTION_ENTER  2
#define RT_FUNCTION_EXIT   3
#define RT_MALLOC          4
#define RT_FREE            5
#define RT_PTR_ASSIGN      6
#define RT_FIELD_LOAD      7
#define RT_FIELD_STORE     8
#define RT_BRANCH          9
#define RT_LOOP_ITER       10
#define RT_NULL_CHECK      11
#define RT_RETURN          12

/**
 * A single recorded runtime event.
 *
 * Fields are only populated as relevant for each event type.
 * Unpopulated fields are zero/NULL.
 *
 * stmt_line: source line number — used for correlation with static analysis.
 * alloc_site: matches IR AllocStmt allocSite (assigned in source order).
 * addr / addr2: concrete addresses (NOT abstract NodeIDs).
 */
typedef struct RTEvent {
    int         type;
    int         stmt_id;        /* sequential statement ID within function */
    int         stmt_line;      /* source file line number                  */
    const char* func_name;      /* enclosing function name                  */
    const char* var_name;       /* variable name (for assign, load, store)  */
    const char* field_name;     /* field name (for load/store)              */
    uintptr_t   addr;           /* primary concrete address                 */
    uintptr_t   addr2;          /* secondary address (e.g., load result)    */
    size_t      size;           /* allocation size in bytes                 */
    int         alloc_site;     /* matches IR AllocStmt.allocSite           */
    int         branch_taken;   /* 1=taken/true, 0=not-taken/false          */
    int         loop_iter;      /* loop iteration count (1-based)           */
} RTEvent;

/* Maximum number of events recordable in a single run */
#define RT_MAX_EVENTS 65536

/* Initialize event recording — call before any benchmark function */
void rt_init(void);

/* Flush all recorded events to a JSON file */
void rt_flush(const char* output_path);

/* ---- Event recording functions ---- */

void rt_program_start(const char* func_name);
void rt_program_end(void);
void rt_function_enter(int line, const char* func_name);
void rt_function_exit(int line, const char* func_name);

/** malloc — call AFTER the allocation, pass the returned address */
void rt_malloc(int stmt_id, int line, int alloc_site,
               const char* func, const char* var,
               void* addr, size_t size);

/** free — call BEFORE the free */
void rt_free(int line, void* addr);

/** pointer assignment: var = src_ptr — call AFTER the assignment */
void rt_ptr_assign(int stmt_id, int line, const char* func,
                   const char* var, void* addr);

/**
 * field load: dst = base->field — call AFTER the load
 * base_addr: concrete address of the base object before load
 * result_addr: concrete address loaded from the field
 */
void rt_field_load(int stmt_id, int line, const char* func,
                   const char* dst_var, void* base_addr,
                   const char* field, void* result_addr);

/**
 * field store: base->field = value — call AFTER the store
 * base_addr: concrete address of the base object
 * value_addr: concrete address stored into the field
 */
void rt_field_store(int stmt_id, int line, const char* func,
                    void* base_addr, const char* field, void* value_addr);

/** branch — call with the condition value BEFORE the branch executes */
void rt_branch(int stmt_id, int line, const char* func, int condition);

/** loop iteration — call at the TOP of each loop body iteration */
void rt_loop_iter(int stmt_id, int line, const char* func, int iter);

/** null check — records a pointer comparison against NULL */
void rt_null_check(int stmt_id, int line, const char* func,
                   const char* var, void* addr);

/** return — records a pointer return value */
void rt_return(int stmt_id, int line, const char* func,
               const char* var, void* addr);

#ifdef __cplusplus
}
#endif

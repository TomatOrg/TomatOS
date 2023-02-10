/* ----------------------------------------------------------------------------
Copyright (c) 2018-2021, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#include "mimalloc.h"
#include "mimalloc-internal.h"
#include "mimalloc-atomic.h"

#include <stdio.h>
#include <stdlib.h> // strtol
#include <string.h> // strncpy, strncat, strlen, strstr
#include <ctype.h>  // toupper
#include <stdarg.h>

#ifdef _MSC_VER
#pragma warning(disable:4996)   // strncpy, strncat
#endif


static long mi_max_error_count   = 16; // stop outputting errors after this (use < 0 for no limit)
static long mi_max_warning_count = 16; // stop outputting warnings after this (use < 0 for no limit)

static void mi_add_stderr_output(void);

int mi_version(void) mi_attr_noexcept {
  return MI_MALLOC_VERSION;
}

#ifdef _WIN32
#include <conio.h>
#endif

// --------------------------------------------------------
// Options
// These can be accessed by multiple threads and may be
// concurrently initialized, but an initializing data race
// is ok since they resolve to the same value.
// --------------------------------------------------------
typedef enum mi_init_e {
  UNINIT,       // not yet initialized
  DEFAULTED,    // not found in the environment, use default value
  INITIALIZED   // found in environment or set explicitly
} mi_init_t;

typedef struct mi_option_desc_s {
  long        value;  // the value
  mi_init_t   init;   // is it initialized yet? (from the environment)
  mi_option_t option; // for debugging: the option index should match the option
  const char* name;   // option name without `mimalloc_` prefix
  const char* legacy_name; // potential legacy v1.x option name
} mi_option_desc_t;

#define MI_OPTION(opt)                  mi_option_##opt, #opt, NULL
#define MI_OPTION_LEGACY(opt,legacy)    mi_option_##opt, #opt, #legacy

static mi_option_desc_t options[_mi_option_last] =
{
  // stable options
  #if MI_DEBUG || defined(MI_SHOW_ERRORS)
  { 1, UNINIT, MI_OPTION(show_errors) },
  #else
  { 0, UNINIT, MI_OPTION(show_errors) },
  #endif
  { 0, UNINIT, MI_OPTION(show_stats) },
  { 0, UNINIT, MI_OPTION(verbose) },

  // Some of the following options are experimental and not all combinations are valid. Use with care.
  { 1, UNINIT, MI_OPTION(eager_commit) },        // commit per segment directly (8MiB)  (but see also `eager_commit_delay`)
  { 0, UNINIT, MI_OPTION(deprecated_eager_region_commit) },
  { 0, UNINIT, MI_OPTION(deprecated_reset_decommits) },
  { 0, UNINIT, MI_OPTION(large_os_pages) },      // use large OS pages, use only with eager commit to prevent fragmentation of VMA's
  { 0, UNINIT, MI_OPTION(reserve_huge_os_pages) },  // per 1GiB huge pages
  { -1, UNINIT, MI_OPTION(reserve_huge_os_pages_at) }, // reserve huge pages at node N
  { 0, UNINIT, MI_OPTION(reserve_os_memory)     },
  { 0, UNINIT, MI_OPTION(deprecated_segment_cache) },  // cache N segments per thread
  { 0, UNINIT, MI_OPTION(page_reset) },          // reset page memory on free
  { 0, UNINIT, MI_OPTION_LEGACY(abandoned_page_decommit, abandoned_page_reset) },// decommit free page memory when a thread terminates
  { 0, UNINIT, MI_OPTION(deprecated_segment_reset) },
  #if defined(__NetBSD__)
  { 0, UNINIT, MI_OPTION(eager_commit_delay) },  // the first N segments per thread are not eagerly committed
  #elif defined(_WIN32)
  { 4, UNINIT, MI_OPTION(eager_commit_delay) },  // the first N segments per thread are not eagerly committed (but per page in the segment on demand)
  #else
  { 1, UNINIT, MI_OPTION(eager_commit_delay) },  // the first N segments per thread are not eagerly committed (but per page in the segment on demand)
  #endif
  { 25,   UNINIT, MI_OPTION_LEGACY(decommit_delay, reset_delay) }, // page decommit delay in milli-seconds
  { 0,    UNINIT, MI_OPTION(use_numa_nodes) },    // 0 = use available numa nodes, otherwise use at most N nodes.
  { 0,    UNINIT, MI_OPTION(limit_os_alloc) },    // 1 = do not use OS memory for allocation (but only reserved arenas)
  { 100,  UNINIT, MI_OPTION(os_tag) },            // only apple specific for now but might serve more or less related purpose
  { 16,   UNINIT, MI_OPTION(max_errors) },        // maximum errors that are output
  { 16,   UNINIT, MI_OPTION(max_warnings) },      // maximum warnings that are output
  { 8,    UNINIT, MI_OPTION(max_segment_reclaim)},// max. number of segment reclaims from the abandoned segments per try.
  { 1,    UNINIT, MI_OPTION(allow_decommit) },    // decommit slices when no longer used (after decommit_delay milli-seconds)
  { 500,  UNINIT, MI_OPTION(segment_decommit_delay) }, // decommit delay in milli-seconds for freed segments
  { 1,    UNINIT, MI_OPTION(decommit_extend_delay) },
  { 0,    UNINIT, MI_OPTION(destroy_on_exit)}     // release all OS memory on process exit; careful with dangling pointer or after-exit frees!
};

static void mi_option_init(mi_option_desc_t* desc);

void _mi_options_init(void) {
  // called on process load; should not be called before the CRT is initialized!
  // (e.g. do not call this from process_init as that may run before CRT initialization)
  mi_add_stderr_output(); // now it safe to use stderr for output
  for(int i = 0; i < _mi_option_last; i++ ) {
    mi_option_t option = (mi_option_t)i;
    long l = mi_option_get(option); MI_UNUSED(l); // initialize
    // if (option != mi_option_verbose)
    {
      mi_option_desc_t* desc = &options[option];
      _mi_verbose_message("option '%s': %ld\n", desc->name, desc->value);
    }
  }
  mi_max_error_count = mi_option_get(mi_option_max_errors);
  mi_max_warning_count = mi_option_get(mi_option_max_warnings);
}

mi_decl_nodiscard long mi_option_get(mi_option_t option) {
  mi_assert(option >= 0 && option < _mi_option_last);
  if (option < 0 || option >= _mi_option_last) return 0;
  mi_option_desc_t* desc = &options[option];
  mi_assert(desc->option == option);  // index should match the option
  if mi_unlikely(desc->init == UNINIT) {
    mi_option_init(desc);
  }
  return desc->value;
}

mi_decl_nodiscard long mi_option_get_clamp(mi_option_t option, long min, long max) {
  long x = mi_option_get(option);
  return (x < min ? min : (x > max ? max : x));
}

void mi_option_set(mi_option_t option, long value) {
  mi_assert(option >= 0 && option < _mi_option_last);
  if (option < 0 || option >= _mi_option_last) return;
  mi_option_desc_t* desc = &options[option];
  mi_assert(desc->option == option);  // index should match the option
  desc->value = value;
  desc->init = INITIALIZED;
}

void mi_option_set_default(mi_option_t option, long value) {
  mi_assert(option >= 0 && option < _mi_option_last);
  if (option < 0 || option >= _mi_option_last) return;
  mi_option_desc_t* desc = &options[option];
  if (desc->init != INITIALIZED) {
    desc->value = value;
  }
}

mi_decl_nodiscard bool mi_option_is_enabled(mi_option_t option) {
  return (mi_option_get(option) != 0);
}

void mi_option_set_enabled(mi_option_t option, bool enable) {
  mi_option_set(option, (enable ? 1 : 0));
}

void mi_option_set_enabled_default(mi_option_t option, bool enable) {
  mi_option_set_default(option, (enable ? 1 : 0));
}

void mi_option_enable(mi_option_t option) {
  mi_option_set_enabled(option,true);
}

void mi_option_disable(mi_option_t option) {
  mi_option_set_enabled(option,false);
}


static void mi_cdecl mi_out_stderr(const char* msg, void* arg) {
  MI_UNUSED(arg);
  if (msg == NULL) return;
  #ifdef _WIN32
  // on windows with redirection, the C runtime cannot handle locale dependent output
  // after the main thread closes so we use direct console output.
  if (!_mi_preloading()) {
    // _cputs(msg);  // _cputs cannot be used at is aborts if it fails to lock the console
    static HANDLE hcon = INVALID_HANDLE_VALUE;
    static bool hconIsConsole;
    if (hcon == INVALID_HANDLE_VALUE) {
      CONSOLE_SCREEN_BUFFER_INFO sbi;
      hcon = GetStdHandle(STD_ERROR_HANDLE);
      hconIsConsole = ((hcon != INVALID_HANDLE_VALUE) && GetConsoleScreenBufferInfo(hcon, &sbi));
    }
    const size_t len = strlen(msg);
    if (len > 0 && len < UINT32_MAX) {
      DWORD written = 0;
      if (hconIsConsole) {
        WriteConsoleA(hcon, msg, (DWORD)len, &written, NULL);
      }
      else if (hcon != INVALID_HANDLE_VALUE) {
        // use direct write if stderr was redirected
        WriteFile(hcon, msg, (DWORD)len, &written, NULL);
      }
      else {
        // finally fall back to fputs after all
        fputs(msg, stderr);
      }
    }
  }
  #else
  fputs(msg, stderr);
  #endif
}

// Since an output function can be registered earliest in the `main`
// function we also buffer output that happens earlier. When
// an output function is registered it is called immediately with
// the output up to that point.
#ifndef MI_MAX_DELAY_OUTPUT
#define MI_MAX_DELAY_OUTPUT ((size_t)(32*1024))
#endif
static char out_buf[MI_MAX_DELAY_OUTPUT+1];
static _Atomic(size_t) out_len;

static void mi_cdecl mi_out_buf(const char* msg, void* arg) {
  MI_UNUSED(arg);
  if (msg==NULL) return;
  if (mi_atomic_load_relaxed(&out_len)>=MI_MAX_DELAY_OUTPUT) return;
  size_t n = strlen(msg);
  if (n==0) return;
  // claim space
  size_t start = mi_atomic_add_acq_rel(&out_len, n);
  if (start >= MI_MAX_DELAY_OUTPUT) return;
  // check bound
  if (start+n >= MI_MAX_DELAY_OUTPUT) {
    n = MI_MAX_DELAY_OUTPUT-start-1;
  }
  _mi_memcpy(&out_buf[start], msg, n);
}

static void mi_out_buf_flush(mi_output_fun* out, bool no_more_buf, void* arg) {
  if (out==NULL) return;
  // claim (if `no_more_buf == true`, no more output will be added after this point)
  size_t count = mi_atomic_add_acq_rel(&out_len, (no_more_buf ? MI_MAX_DELAY_OUTPUT : 1));
  // and output the current contents
  if (count>MI_MAX_DELAY_OUTPUT) count = MI_MAX_DELAY_OUTPUT;
  out_buf[count] = 0;
  out(out_buf,arg);
  if (!no_more_buf) {
    out_buf[count] = '\n'; // if continue with the buffer, insert a newline
  }
}


// Once this module is loaded, switch to this routine
// which outputs to stderr and the delayed output buffer.
static void mi_cdecl mi_out_buf_stderr(const char* msg, void* arg) {
  mi_out_stderr(msg,arg);
  mi_out_buf(msg,arg);
}



// --------------------------------------------------------
// Default output handler
// --------------------------------------------------------

// Should be atomic but gives errors on many platforms as generally we cannot cast a function pointer to a uintptr_t.
// For now, don't register output from multiple threads.
static mi_output_fun* volatile mi_out_default; // = NULL
static _Atomic(void*) mi_out_arg; // = NULL

static mi_output_fun* mi_out_get_default(void** parg) {
  if (parg != NULL) { *parg = mi_atomic_load_ptr_acquire(void,&mi_out_arg); }
  mi_output_fun* out = mi_out_default;
  return (out == NULL ? &mi_out_buf : out);
}

void mi_register_output(mi_output_fun* out, void* arg) mi_attr_noexcept {
  mi_out_default = (out == NULL ? &mi_out_stderr : out); // stop using the delayed output buffer
  mi_atomic_store_ptr_release(void,&mi_out_arg, arg);
  if (out!=NULL) mi_out_buf_flush(out,true,arg);         // output all the delayed output now
}

// add stderr to the delayed output after the module is loaded
static void mi_add_stderr_output() {
  mi_assert_internal(mi_out_default == NULL);
  mi_out_buf_flush(&mi_out_stderr, false, NULL); // flush current contents to stderr
  mi_out_default = &mi_out_buf_stderr;           // and add stderr to the delayed output
}

// --------------------------------------------------------
// Messages, all end up calling `_mi_fputs`.
// --------------------------------------------------------
static _Atomic(size_t) error_count;   // = 0;  // when >= max_error_count stop emitting errors
static _Atomic(size_t) warning_count; // = 0;  // when >= max_warning_count stop emitting warnings

// When overriding malloc, we may recurse into mi_vfprintf if an allocation
// inside the C runtime causes another message.
// In some cases (like on macOS) the loader already allocates which
// calls into mimalloc; if we then access thread locals (like `recurse`)
// this may crash as the access may call _tlv_bootstrap that tries to
// (recursively) invoke malloc again to allocate space for the thread local
// variables on demand. This is why we use a _mi_preloading test on such
// platforms. However, C code generator may move the initial thread local address
// load before the `if` and we therefore split it out in a separate funcion.
static mi_decl_thread bool recurse = false;

static mi_decl_noinline bool mi_recurse_enter_prim(void) {
  if (recurse) return false;
  recurse = true;
  return true;
}

static mi_decl_noinline void mi_recurse_exit_prim(void) {
  recurse = false;
}

static bool mi_recurse_enter(void) {
  #if defined(__APPLE__) || defined(MI_TLS_RECURSE_GUARD)
  if (_mi_preloading()) return true;
  #endif
  return mi_recurse_enter_prim();
}

static void mi_recurse_exit(void) {
  #if defined(__APPLE__) || defined(MI_TLS_RECURSE_GUARD)
  if (_mi_preloading()) return;
  #endif
  mi_recurse_exit_prim();
}

void _mi_fputs(mi_output_fun* out, void* arg, const char* prefix, const char* message) {
  if (out==NULL || (FILE*)out==stdout || (FILE*)out==stderr) { // TODO: use mi_out_stderr for stderr?
    if (!mi_recurse_enter()) return;
    out = mi_out_get_default(&arg);
    if (prefix != NULL) out(prefix, arg);
    out(message, arg);
    mi_recurse_exit();
  }
  else {
    if (prefix != NULL) out(prefix, arg);
    out(message, arg);
  }
}

// Define our own limited `fprintf` that avoids memory allocation.
// We do this using `snprintf` with a limited buffer.
static void mi_vfprintf( mi_output_fun* out, void* arg, const char* prefix, const char* fmt, va_list args ) {
  char buf[512];
  if (fmt==NULL) return;
  if (!mi_recurse_enter()) return;
  vsnprintf(buf,sizeof(buf)-1,fmt,args);
  mi_recurse_exit();
  _mi_fputs(out,arg,prefix,buf);
}

void _mi_fprintf( mi_output_fun* out, void* arg, const char* fmt, ... ) {
  va_list args;
  va_start(args,fmt);
  mi_vfprintf(out,arg,NULL,fmt,args);
  va_end(args);
}

static void mi_vfprintf_thread(mi_output_fun* out, void* arg, const char* prefix, const char* fmt, va_list args) {
  if (prefix != NULL && strlen(prefix) <= 32 && !_mi_is_main_thread()) {
    char tprefix[64];
    snprintf(tprefix, sizeof(tprefix), "%sthread 0x%llx: ", prefix, (unsigned long long)_mi_thread_id());
    mi_vfprintf(out, arg, tprefix, fmt, args);
  }
  else {
    mi_vfprintf(out, arg, prefix, fmt, args);
  }
}

void _mi_trace_message(const char* fmt, ...) {
  if (mi_option_get(mi_option_verbose) <= 1) return;  // only with verbose level 2 or higher
  va_list args;
  va_start(args, fmt);
  mi_vfprintf_thread(NULL, NULL, "mimalloc: ", fmt, args);
  va_end(args);
}

void _mi_verbose_message(const char* fmt, ...) {
  if (!mi_option_is_enabled(mi_option_verbose)) return;
  va_list args;
  va_start(args,fmt);
  mi_vfprintf(NULL, NULL, "mimalloc: ", fmt, args);
  va_end(args);
}

static void mi_show_error_message(const char* fmt, va_list args) {
  if (!mi_option_is_enabled(mi_option_verbose)) {
    if (!mi_option_is_enabled(mi_option_show_errors)) return;
    if (mi_max_error_count >= 0 && (long)mi_atomic_increment_acq_rel(&error_count) > mi_max_error_count) return;
  }
  mi_vfprintf_thread(NULL, NULL, "mimalloc: error: ", fmt, args);
}

void _mi_warning_message(const char* fmt, ...) {
  if (!mi_option_is_enabled(mi_option_verbose)) {
    if (!mi_option_is_enabled(mi_option_show_errors)) return;
    if (mi_max_warning_count >= 0 && (long)mi_atomic_increment_acq_rel(&warning_count) > mi_max_warning_count) return;
  }
  va_list args;
  va_start(args,fmt);
  mi_vfprintf_thread(NULL, NULL, "mimalloc: warning: ", fmt, args);
  va_end(args);
}


#if MI_DEBUG
void _mi_assert_fail(const char* assertion, const char* fname, unsigned line, const char* func ) {
  _mi_fprintf(NULL, NULL, "mimalloc: assertion failed: at \"%s\":%u, %s\n  assertion: \"%s\"\n", fname, line, (func==NULL?"":func), assertion);
  abort();
}
#endif

// --------------------------------------------------------
// Errors
// --------------------------------------------------------

static mi_error_fun* volatile  mi_error_handler; // = NULL
static _Atomic(void*) mi_error_arg;     // = NULL

static void mi_error_default(int err) {
  MI_UNUSED(err);
#if (MI_DEBUG>0)
  if (err==EFAULT) {
    #ifdef _MSC_VER
    __debugbreak();
    #endif
    abort();
  }
#endif
#if (MI_SECURE>0)
  if (err==EFAULT) {  // abort on serious errors in secure mode (corrupted meta-data)
    abort();
  }
#endif
#if defined(MI_XMALLOC)
  if (err==ENOMEM || err==EOVERFLOW) { // abort on memory allocation fails in xmalloc mode
    abort();
  }
#endif
}

void mi_register_error(mi_error_fun* fun, void* arg) {
  mi_error_handler = fun;  // can be NULL
  mi_atomic_store_ptr_release(void,&mi_error_arg, arg);
}

void _mi_error_message(int err, const char* fmt, ...) {
  // show detailed error message
  va_list args;
  va_start(args, fmt);
  mi_show_error_message(fmt, args);
  va_end(args);
  // and call the error handler which may abort (or return normally)
  if (mi_error_handler != NULL) {
    mi_error_handler(err, mi_atomic_load_ptr_acquire(void,&mi_error_arg));
  }
  else {
    mi_error_default(err);
  }
}

// --------------------------------------------------------
// Initialize options by checking the environment
// --------------------------------------------------------

static void mi_option_init(mi_option_desc_t* desc) {
  if (!_mi_preloading()) {
    desc->init = DEFAULTED;
  }
}

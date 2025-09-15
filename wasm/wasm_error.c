/* wasm_error.c - WASM-compatible error handling for libpng
 *
 * Copyright (c) 2025 libpng contributors
 * Copyright (c) 2025 Superstruct Ltd, New Zealand
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 *
 * ===========================================================================
 *
 * This file provides WASM-compatible error handling that:
 * 1. Implements safe setjmp/longjmp for WASM environment
 * 2. Provides JavaScript-friendly error propagation
 * 3. Ensures proper cleanup on errors
 * 4. Maintains compatibility with libpng's standard error handling
 */

#if defined(PNG_READ_SUPPORTED) || defined(PNG_WRITE_SUPPORTED)

#if PNG_WASM_SIMD_OPT > 0 || defined(__EMSCRIPTEN__)

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define PNG_INTERNAL
#include "../pngpriv.h"

/* WASM Error Management Constants */
#define PNG_WASM_MAX_ERROR_MESSAGE_LENGTH  512
#define PNG_WASM_MAX_WARNING_COUNT         50
#define PNG_WASM_ERROR_STACK_SIZE          10

/* WASM error severity levels */
typedef enum
{
   PNG_WASM_ERROR_FATAL = 0,    /* Fatal error - stops processing */
   PNG_WASM_ERROR_RECOVERABLE,  /* Recoverable error - continues with warnings */
   PNG_WASM_WARNING,            /* Warning - continues processing */
   PNG_WASM_INFO                /* Informational message */
} png_wasm_error_severity;

/* WASM error record structure */
typedef struct png_wasm_error_record_struct
{
   png_wasm_error_severity severity;
   char message[PNG_WASM_MAX_ERROR_MESSAGE_LENGTH];
   const char* function_name;
   int line_number;
   int error_code;
   png_uint_32 timestamp;
} png_wasm_error_record;

/* WASM error context structure */
typedef struct png_wasm_error_context_struct
{
   png_structp png_ptr;

   /* Error stack for tracking nested errors */
   png_wasm_error_record error_stack[PNG_WASM_ERROR_STACK_SIZE];
   int error_count;
   int warning_count;

   /* Current error state */
   png_wasm_error_record current_error;
   int has_error;

   /* setjmp buffer for WASM-compatible error jumping */
   jmp_buf error_jmp_buf;
   int jmp_buf_valid;

   /* Cleanup callbacks */
   void (*cleanup_callback)(png_structp);
   void* cleanup_user_data;

   /* JavaScript error callback */
   void (*js_error_callback)(png_wasm_error_severity, const char*, void*);
   void* js_user_data;

} png_wasm_error_context;

/* Global WASM error context */
static png_wasm_error_context wasm_error_context = {0};

/* PRIORITY 2: Initialize WASM error handling */
PNG_FUNCTION(int, png_wasm_error_init, (png_structp png_ptr), PNG_EMPTY)
{
   if (png_ptr == NULL)
      return 0;

   /* Initialize error context */
   memset(&wasm_error_context, 0, sizeof(png_wasm_error_context));
   wasm_error_context.png_ptr = png_ptr;
   wasm_error_context.jmp_buf_valid = 0;

   /* Set up custom error and warning handlers */
   png_set_error_fn(png_ptr, (png_voidp)&wasm_error_context,
                   png_wasm_error_handler, png_wasm_warning_handler);

   return 1;
}

/* PRIORITY 2: WASM-compatible error handler */
PNG_FUNCTION(void, png_wasm_error_handler,
            (png_structp png_ptr, png_const_charp error_message), PNG_NORETURN)
{
   png_wasm_error_context* ctx = (png_wasm_error_context*)png_get_error_ptr(png_ptr);

   if (ctx == NULL)
      ctx = &wasm_error_context;

   /* Record error in stack if space available */
   if (ctx->error_count < PNG_WASM_ERROR_STACK_SIZE)
   {
      png_wasm_error_record* record = &ctx->error_stack[ctx->error_count];

      record->severity = PNG_WASM_ERROR_FATAL;
      strncpy(record->message, error_message, PNG_WASM_MAX_ERROR_MESSAGE_LENGTH - 1);
      record->message[PNG_WASM_MAX_ERROR_MESSAGE_LENGTH - 1] = '\0';
      record->function_name = "png_wasm_error_handler";
      record->line_number = __LINE__;
      record->error_code = -1;

#ifdef __EMSCRIPTEN__
      record->timestamp = (png_uint_32)emscripten_get_now();
#else
      record->timestamp = 0;
#endif

      ctx->error_count++;
   }

   /* Set current error */
   ctx->current_error.severity = PNG_WASM_ERROR_FATAL;
   strncpy(ctx->current_error.message, error_message, PNG_WASM_MAX_ERROR_MESSAGE_LENGTH - 1);
   ctx->current_error.message[PNG_WASM_MAX_ERROR_MESSAGE_LENGTH - 1] = '\0';
   ctx->has_error = 1;

   /* Call JavaScript error callback if available */
   if (ctx->js_error_callback != NULL)
   {
      ctx->js_error_callback(PNG_WASM_ERROR_FATAL, error_message, ctx->js_user_data);
   }

   /* Perform cleanup */
   if (ctx->cleanup_callback != NULL)
   {
      ctx->cleanup_callback(png_ptr);
   }

   /* Cleanup WASM-specific resources */
   png_wasm_memory_cleanup(png_ptr);
   png_wasm_zlib_cleanup_buffer(png_ptr);

   /* WASM-compatible longjmp */
   if (ctx->jmp_buf_valid)
   {
      longjmp(ctx->error_jmp_buf, 1);
   }
   else
   {
      /* If no jump buffer, we need to abort in a WASM-friendly way */
#ifdef __EMSCRIPTEN__
      emscripten_force_exit(1);
#else
      abort();
#endif
   }
}

/* PRIORITY 2: WASM-compatible warning handler */
PNG_FUNCTION(void, png_wasm_warning_handler,
            (png_structp png_ptr, png_const_charp warning_message), PNG_EMPTY)
{
   png_wasm_error_context* ctx = (png_wasm_error_context*)png_get_error_ptr(png_ptr);

   if (ctx == NULL)
      ctx = &wasm_error_context;

   /* Limit warning accumulation to prevent memory issues */
   if (ctx->warning_count >= PNG_WASM_MAX_WARNING_COUNT)
      return;

   /* Record warning in stack if space available */
   if (ctx->error_count < PNG_WASM_ERROR_STACK_SIZE)
   {
      png_wasm_error_record* record = &ctx->error_stack[ctx->error_count];

      record->severity = PNG_WASM_WARNING;
      strncpy(record->message, warning_message, PNG_WASM_MAX_ERROR_MESSAGE_LENGTH - 1);
      record->message[PNG_WASM_MAX_ERROR_MESSAGE_LENGTH - 1] = '\0';
      record->function_name = "png_wasm_warning_handler";
      record->line_number = __LINE__;
      record->error_code = 0;

#ifdef __EMSCRIPTEN__
      record->timestamp = (png_uint_32)emscripten_get_now();
#else
      record->timestamp = 0;
#endif

      ctx->error_count++;
   }

   ctx->warning_count++;

   /* Call JavaScript warning callback if available */
   if (ctx->js_error_callback != NULL)
   {
      ctx->js_error_callback(PNG_WASM_WARNING, warning_message, ctx->js_user_data);
   }
}

/* PRIORITY 2: Set up WASM-compatible setjmp for error recovery */
PNG_FUNCTION(int, png_wasm_setjmp, (png_structp png_ptr), PNG_EMPTY)
{
   png_wasm_error_context* ctx = (png_wasm_error_context*)png_get_error_ptr(png_ptr);

   if (ctx == NULL)
      ctx = &wasm_error_context;

   /* Set up jump buffer */
   int result = setjmp(ctx->error_jmp_buf);
   ctx->jmp_buf_valid = 1;

   return result;
}

/* PRIORITY 2: Clear WASM setjmp buffer */
PNG_FUNCTION(void, png_wasm_clear_jmp_buf, (png_structp png_ptr), PNG_EMPTY)
{
   png_wasm_error_context* ctx = (png_wasm_error_context*)png_get_error_ptr(png_ptr);

   if (ctx == NULL)
      ctx = &wasm_error_context;

   ctx->jmp_buf_valid = 0;
}

/* PRIORITY 2: Set JavaScript error callback */
PNG_FUNCTION(void, png_wasm_set_error_callback,
            (png_structp png_ptr,
             void (*callback)(png_wasm_error_severity, const char*, void*),
             void* user_data), PNG_EMPTY)
{
   png_wasm_error_context* ctx = (png_wasm_error_context*)png_get_error_ptr(png_ptr);

   if (ctx == NULL)
      ctx = &wasm_error_context;

   ctx->js_error_callback = callback;
   ctx->js_user_data = user_data;
}

/* PRIORITY 2: Set cleanup callback for error handling */
PNG_FUNCTION(void, png_wasm_set_cleanup_callback,
            (png_structp png_ptr, void (*callback)(png_structp), void* user_data), PNG_EMPTY)
{
   png_wasm_error_context* ctx = (png_wasm_error_context*)png_get_error_ptr(png_ptr);

   if (ctx == NULL)
      ctx = &wasm_error_context;

   ctx->cleanup_callback = callback;
   ctx->cleanup_user_data = user_data;
}

/* PRIORITY 2: Get current error information */
PNG_FUNCTION(int, png_wasm_get_error_info,
            (png_structp png_ptr, png_wasm_error_info* error_info), PNG_EMPTY)
{
   png_wasm_error_context* ctx = (png_wasm_error_context*)png_get_error_ptr(png_ptr);

   if (ctx == NULL || error_info == NULL)
      return 0;

   memset(error_info, 0, sizeof(png_wasm_error_info));

   error_info->has_error = ctx->has_error;
   error_info->error_count = ctx->error_count;
   error_info->warning_count = ctx->warning_count;

   if (ctx->has_error)
   {
      error_info->severity = ctx->current_error.severity;
      strncpy(error_info->message, ctx->current_error.message,
              PNG_WASM_MAX_ERROR_MESSAGE_LENGTH - 1);
      error_info->message[PNG_WASM_MAX_ERROR_MESSAGE_LENGTH - 1] = '\0';
   }

   return 1;
}

/* PRIORITY 2: Get error stack for debugging */
PNG_FUNCTION(int, png_wasm_get_error_stack,
            (png_structp png_ptr, png_wasm_error_record* stack, int max_entries), PNG_EMPTY)
{
   png_wasm_error_context* ctx = (png_wasm_error_context*)png_get_error_ptr(png_ptr);

   if (ctx == NULL || stack == NULL || max_entries <= 0)
      return 0;

   int entries_to_copy = (ctx->error_count < max_entries) ?
                        ctx->error_count : max_entries;

   for (int i = 0; i < entries_to_copy; i++)
   {
      stack[i] = ctx->error_stack[i];
   }

   return entries_to_copy;
}

/* PRIORITY 2: Clear error state */
PNG_FUNCTION(void, png_wasm_clear_errors, (png_structp png_ptr), PNG_EMPTY)
{
   png_wasm_error_context* ctx = (png_wasm_error_context*)png_get_error_ptr(png_ptr);

   if (ctx == NULL)
      ctx = &wasm_error_context;

   ctx->has_error = 0;
   ctx->error_count = 0;
   ctx->warning_count = 0;
   memset(ctx->error_stack, 0, sizeof(ctx->error_stack));
   memset(&ctx->current_error, 0, sizeof(ctx->current_error));
}

/* PRIORITY 2: Safe WASM error recovery macro */
#define PNG_WASM_TRY(png_ptr, code) \
   do { \
      if (png_wasm_setjmp(png_ptr) == 0) { \
         code \
      } else { \
         /* Error occurred - cleanup handled by error handler */ \
      } \
      png_wasm_clear_jmp_buf(png_ptr); \
   } while(0)

/* PRIORITY 2: Cleanup WASM error handling */
PNG_FUNCTION(void, png_wasm_error_cleanup, (png_structp png_ptr), PNG_EMPTY)
{
   png_wasm_error_context* ctx = (png_wasm_error_context*)png_get_error_ptr(png_ptr);

   if (ctx == NULL)
      return;

   /* Clear all error state */
   png_wasm_clear_errors(png_ptr);

   /* Reset error handlers to default */
   png_set_error_fn(png_ptr, NULL, NULL, NULL);

   /* Clear context */
   memset(ctx, 0, sizeof(png_wasm_error_context));
}

#endif /* PNG_WASM_SIMD_OPT > 0 || defined(__EMSCRIPTEN__) */
#endif /* PNG_READ_SUPPORTED || PNG_WRITE_SUPPORTED */

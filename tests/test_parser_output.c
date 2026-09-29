// Tests for the download reader's output accumulation in parser.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// Fault injection for the realloc used by the code under test
static int g_failRealloc = 0;
static void* test_realloc(void* ptr, size_t size) {
    if (g_failRealloc) return NULL;
    return realloc(ptr, size);
}
#define SAFE_REALLOC(ptr, sz) test_realloc(ptr, sz)

#include "mock_windows.h"

// Only the fields the code under test uses
typedef struct {
    wchar_t* accumulatedOutput;
    size_t outputBufferSize;
} SubprocessContext;

#include "parser_output_logic.c"

static int g_tests_run = 0;
static int g_tests_failed = 0;

static void check(const char* name, int condition) {
    g_tests_run++;
    if (condition) {
        printf("PASSED: %s\n", name);
    } else {
        g_tests_failed++;
        printf("FAILED: %s\n", name);
    }
}

static void init_context(SubprocessContext* ctx, size_t size) {
    ctx->outputBufferSize = size;
    ctx->accumulatedOutput = (wchar_t*)malloc(size * sizeof(wchar_t));
    ctx->accumulatedOutput[0] = L'\0';
}

static void test_append_grows_buffer(void) {
    SubprocessContext ctx;
    init_context(&ctx, 8);

    AppendAccumulatedOutput(&ctx, L"abc", TRUE);
    check("append fits", wcscmp(ctx.accumulatedOutput, L"abc\n") == 0 && ctx.outputBufferSize == 8);

    AppendAccumulatedOutput(&ctx, L"defghijk", TRUE);
    check("append grows", wcscmp(ctx.accumulatedOutput, L"abc\ndefghijk\n") == 0);
    check("size covers contents", ctx.outputBufferSize > wcslen(ctx.accumulatedOutput));

    AppendAccumulatedOutput(&ctx, L"tail", FALSE);
    check("final partial line without newline", wcscmp(ctx.accumulatedOutput, L"abc\ndefghijk\ntail") == 0);

    free(ctx.accumulatedOutput);
}

static void test_append_exact_fit(void) {
    SubprocessContext ctx;
    init_context(&ctx, 5);

    // "abc" + "\n" + terminator is exactly 5 characters
    g_failRealloc = 1;
    AppendAccumulatedOutput(&ctx, L"abc", TRUE);
    g_failRealloc = 0;
    check("exact fit needs no realloc", wcscmp(ctx.accumulatedOutput, L"abc\n") == 0 && ctx.outputBufferSize == 5);

    free(ctx.accumulatedOutput);
}

static void test_append_realloc_failure(void) {
    SubprocessContext ctx;
    init_context(&ctx, 8);

    AppendAccumulatedOutput(&ctx, L"abc", TRUE);

    g_failRealloc = 1;
    AppendAccumulatedOutput(&ctx, L"this line does not fit", TRUE);
    check("failed grow keeps size", ctx.outputBufferSize == 8);
    check("failed grow keeps contents", wcscmp(ctx.accumulatedOutput, L"abc\n") == 0);

    // A later small append must still be bounded by the real size
    AppendAccumulatedOutput(&ctx, L"xyzw", FALSE);
    check("later append that does not fit is dropped", wcscmp(ctx.accumulatedOutput, L"abc\n") == 0);
    AppendAccumulatedOutput(&ctx, L"xy", FALSE);
    check("later append that fits is kept", wcscmp(ctx.accumulatedOutput, L"abc\nxy") == 0);
    g_failRealloc = 0;

    free(ctx.accumulatedOutput);
}

static void test_append_null_buffer(void) {
    SubprocessContext ctx = { NULL, 8192 };
    AppendAccumulatedOutput(&ctx, L"abc", TRUE);
    check("NULL buffer is left alone", ctx.accumulatedOutput == NULL);
}

int main(void) {
    printf("Running parser output accumulation tests...\n");
    test_append_grows_buffer();
    test_append_exact_fit();
    test_append_realloc_failure();
    test_append_null_buffer();
    printf("\nTests run: %d, Failed: %d\n", g_tests_run, g_tests_failed);
    return g_tests_failed ? 1 : 0;
}

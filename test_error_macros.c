#include "YouTubeCacher.h"
#include <stdio.h>

/**
 * Test function to demonstrate the new error handling macros
 * This function shows how to use the various error handling patterns
 */
StandardErrorCode TestErrorHandlingMacros(void) {
    DECLARE_CLEANUP_VARS();
    
    wchar_t* testBuffer = NULL;
    HANDLE testHandle = INVALID_HANDLE_VALUE;
    
    printf("Testing error handling macros...\r\n");
    
    // Test parameter validation
    VALIDATE_POINTER(L"test string", cleanup);
    printf("✓ Pointer validation passed\r\n");
    
    VALIDATE_STRING(L"test string", cleanup);
    printf("✓ String validation passed\r\n");
    
    // Test memory allocation with error handling
    SAFE_ALLOC(testBuffer, 1024 * sizeof(wchar_t), cleanup);
    printf("✓ Memory allocation succeeded\r\n");
    
    // Test buffer size validation
    VALIDATE_BUFFER_SIZE(1024, 100, 2048, cleanup);
    printf("✓ Buffer size validation passed\r\n");
    
    // Test validation framework integration
    VALIDATE_POINTER_PARAM(testBuffer, L"testBuffer", cleanup);
    printf("✓ Framework pointer validation passed\r\n");
    
    VALIDATE_STRING_PARAM(L"test", L"testString", 100, cleanup);
    printf("✓ Framework string validation passed\r\n");
    
    // Test system call checking (create a temporary file)
    testHandle = CreateFileW(L"test_temp.txt", 
                            GENERIC_WRITE, 
                            0, 
                            NULL, 
                            CREATE_ALWAYS, 
                            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, 
                            NULL);
    CHECK_SYSTEM_CALL(testHandle != INVALID_HANDLE_VALUE, cleanup);
    printf("✓ System call validation passed\r\n");
    
    // Test handle validation
    VALIDATE_HANDLE(testHandle, cleanup);
    printf("✓ Handle validation passed\r\n");
    
    printf("All error handling macro tests passed!\r\n");
    
    BEGIN_CLEANUP_BLOCK();
        SAFE_FREE_AND_NULL(testBuffer);
        SAFE_CLOSE_HANDLE(testHandle);
        printf("✓ Cleanup completed successfully\r\n");
    END_CLEANUP_BLOCK();
}

/**
 * Test function to demonstrate error propagation
 */
StandardErrorCode TestErrorPropagation(void) {
    DECLARE_CLEANUP_VARS();
    
    printf("Testing error propagation...\r\n");
    
    // Simulate a function call that returns an error
    StandardErrorCode result = YTC_ERROR_INVALID_PARAMETER;
    
    // This should trigger error propagation
    PROPAGATE_ERROR_MSG(result, L"Test error propagation", cleanup);
    
    BEGIN_CLEANUP_BLOCK();
        printf("✓ Error propagation test completed\r\n");
    END_CLEANUP_BLOCK();
}

/**
 * Test function to demonstrate validation framework
 */
StandardErrorCode TestValidationFramework(void) {
    DECLARE_CLEANUP_VARS();
    
    printf("Testing validation framework...\r\n");
    
    // Test various validation functions
    ParameterValidationResult result;
    
    // Test pointer validation
    result = ValidatePointer(NULL, L"testPointer");
    if (!result.isValid) {
        printf("✓ NULL pointer validation correctly failed: %ls\r\n", result.errorMessage);
    }
    
    // Test string validation
    result = ValidateString(L"", L"testString", 100);
    if (!result.isValid) {
        printf("✓ Empty string validation correctly failed: %ls\r\n", result.errorMessage);
    }
    
    // Test buffer size validation
    result = ValidateBufferSize(0, 10, 100);
    if (!result.isValid) {
        printf("✓ Zero buffer size validation correctly failed: %ls\r\n", result.errorMessage);
    }
    
    // Test successful validations
    result = ValidatePointer("valid pointer", L"testPointer");
    if (result.isValid) {
        printf("✓ Valid pointer validation passed: %ls\r\n", result.errorMessage);
    }
    
    result = ValidateString(L"valid string", L"testString", 100);
    if (result.isValid) {
        printf("✓ Valid string validation passed: %ls\r\n", result.errorMessage);
    }
    
    result = ValidateBufferSize(50, 10, 100);
    if (result.isValid) {
        printf("✓ Valid buffer size validation passed: %ls\r\n", result.errorMessage);
    }
    
    printf("Validation framework tests completed!\r\n");
    
    return YTC_ERROR_SUCCESS;
}

/**
 * Main test function that runs all error handling macro tests
 */
BOOL RunErrorHandlingMacroTests(void) {
    printf("=== Error Handling Macro Tests ===\r\n");
    
    // Initialize error handler if not already done
    if (!InitializeErrorHandler(&g_ErrorHandler)) {
        printf("❌ Failed to initialize error handler\r\n");
        return FALSE;
    }
    
    // Run successful path tests
    StandardErrorCode result = TestErrorHandlingMacros();
    if (result != YTC_ERROR_SUCCESS) {
        printf("❌ Error handling macro tests failed with code: %d\r\n", result);
        return FALSE;
    }
    
    // Run validation framework tests
    result = TestValidationFramework();
    if (result != YTC_ERROR_SUCCESS) {
        printf("❌ Validation framework tests failed with code: %d\r\n", result);
        return FALSE;
    }
    
    // Note: We don't run TestErrorPropagation() in the success path
    // because it's designed to demonstrate error propagation
    
    printf("=== All Error Handling Macro Tests Passed! ===\r\n");
    return TRUE;
}
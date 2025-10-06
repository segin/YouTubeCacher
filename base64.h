#ifndef BASE64_H
#define BASE64_H

#include <windows.h>

// Base64 encoding/decoding functions for wide strings
// These handle UTF-16 wide strings by converting to UTF-8 first

// Encode a wide string to base64
// Returns allocated string that must be freed with free()
wchar_t* Base64EncodeWide(const wchar_t* input);

// Decode a base64 string to wide string
// Returns allocated string that must be freed with free()
wchar_t* Base64DecodeWide(const wchar_t* input);

// Internal helper functions
char* Base64Encode(const unsigned char* data, size_t input_length);
unsigned char* Base64Decode(const char* data, size_t* output_length);

#endif // BASE64_H
#include "YouTubeCacher.h"

static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static const int base64_decode_table[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-2,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

char* Base64Encode(const unsigned char* data, size_t input_length) {
    if (!data || input_length == 0) return NULL;
    
    size_t output_length = 4 * ((input_length + 2) / 3);
    char* encoded_data = (char*)malloc(output_length + 1);
    if (!encoded_data) return NULL;
    
    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;
        
        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        
        encoded_data[j++] = base64_chars[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 0 * 6) & 0x3F];
    }
    
    // Add padding
    size_t mod_table[] = {0, 2, 1};
    for (size_t i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[output_length - 1 - i] = '=';
    
    encoded_data[output_length] = '\0';
    return encoded_data;
}

unsigned char* Base64Decode(const char* data, size_t* output_length) {
    if (!data) return NULL;
    
    size_t input_length = strlen(data);
    if (input_length % 4 != 0) return NULL;
    
    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') (*output_length)--;
    if (data[input_length - 2] == '=') (*output_length)--;
    
    unsigned char* decoded_data = (unsigned char*)malloc(*output_length);
    if (!decoded_data) return NULL;
    
    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t sextet_a = data[i] == '=' ? (i++, 0) : (uint32_t)base64_decode_table[(int)data[i++]];
        uint32_t sextet_b = data[i] == '=' ? (i++, 0) : (uint32_t)base64_decode_table[(int)data[i++]];
        uint32_t sextet_c = data[i] == '=' ? (i++, 0) : (uint32_t)base64_decode_table[(int)data[i++]];
        uint32_t sextet_d = data[i] == '=' ? (i++, 0) : (uint32_t)base64_decode_table[(int)data[i++]];
        
        uint32_t triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6) + (sextet_c << 1 * 6) + (sextet_d << 0 * 6);
        
        if (j < *output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }
    
    return decoded_data;
}

wchar_t* Base64EncodeWide(const wchar_t* input) {
    if (!input) return NULL;
    
    // Convert wide string to UTF-8
    int utf8_size = WideCharToMultiByte(CP_UTF8, 0, input, -1, NULL, 0, NULL, NULL);
    if (utf8_size <= 0) return NULL;
    
    char* utf8_data = (char*)malloc(utf8_size);
    if (!utf8_data) return NULL;
    
    WideCharToMultiByte(CP_UTF8, 0, input, -1, utf8_data, utf8_size, NULL, NULL);
    
    // Encode to base64
    char* base64_data = Base64Encode((unsigned char*)utf8_data, utf8_size - 1); // -1 to exclude null terminator
    free(utf8_data);
    
    if (!base64_data) return NULL;
    
    // Convert base64 result to wide string
    int wide_size = MultiByteToWideChar(CP_UTF8, 0, base64_data, -1, NULL, 0);
    if (wide_size <= 0) {
        free(base64_data);
        return NULL;
    }
    
    wchar_t* wide_result = (wchar_t*)malloc(wide_size * sizeof(wchar_t));
    if (!wide_result) {
        free(base64_data);
        return NULL;
    }
    
    MultiByteToWideChar(CP_UTF8, 0, base64_data, -1, wide_result, wide_size);
    free(base64_data);
    
    return wide_result;
}

wchar_t* Base64DecodeWide(const wchar_t* input) {
    if (!input) return NULL;
    
    // Convert wide string to UTF-8
    int utf8_size = WideCharToMultiByte(CP_UTF8, 0, input, -1, NULL, 0, NULL, NULL);
    if (utf8_size <= 0) return NULL;
    
    char* utf8_data = (char*)malloc(utf8_size);
    if (!utf8_data) return NULL;
    
    WideCharToMultiByte(CP_UTF8, 0, input, -1, utf8_data, utf8_size, NULL, NULL);
    
    // Decode from base64
    size_t decoded_length;
    unsigned char* decoded_data = Base64Decode(utf8_data, &decoded_length);
    free(utf8_data);
    
    if (!decoded_data) return NULL;
    
    // Add null terminator to decoded data
    unsigned char* null_terminated = (unsigned char*)realloc(decoded_data, decoded_length + 1);
    if (!null_terminated) {
        free(decoded_data);
        return NULL;
    }
    null_terminated[decoded_length] = '\0';
    
    // Convert back to wide string
    int wide_size = MultiByteToWideChar(CP_UTF8, 0, (char*)null_terminated, -1, NULL, 0);
    if (wide_size <= 0) {
        free(null_terminated);
        return NULL;
    }
    
    wchar_t* wide_result = (wchar_t*)malloc(wide_size * sizeof(wchar_t));
    if (!wide_result) {
        free(null_terminated);
        return NULL;
    }
    
    MultiByteToWideChar(CP_UTF8, 0, (char*)null_terminated, -1, wide_result, wide_size);
    free(null_terminated);
    
    return wide_result;
}
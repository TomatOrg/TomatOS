#include "encoding.h"

#include <runtime/dotnet/gc/gc.h>

#include <converter.h>

System_String new_string_from_utf8(const char* str, size_t len) {
    // calculate the size needed
    size_t size_needed = utf8_to_utf16((const utf8_t*)str, len, NULL, 0);
    ASSERT(size_needed < SIZE_2GB);

    // allocate the string and actually do the convertion
    System_String newStr = GC_NEW_STRING(size_needed);
    newStr->Length = (int)size_needed;
    utf8_to_utf16((const utf8_t*)str, len, newStr->Chars, newStr->Length);

    return newStr;
}

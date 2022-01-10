#include "method_info.h"

#include "parameter_info.h"

#include "type.h"

void method_signature_string(method_info_t info, buffer_t* buffer) {
    type_full_name(info->return_type, buffer);
    bputc(' ', buffer);
    method_full_name(info, buffer);
}

void method_full_name(method_info_t info, buffer_t* buffer) {
    if (info->declaring_type == NULL) {
        // native method
        bprintf(buffer, "<native>::%s", info->name);
        return;
    }

    type_full_name(info->declaring_type, buffer);
    bputc(':', buffer);
    bputc(':', buffer);
    bprintf(buffer, "%s(", info->name);
    for (int i = 0; i < info->parameters_count; i++) {
        if (i != 0) {
            bputc(',', buffer);
        }
        type_full_name(info->parameters[i].parameter_type, buffer);
    }
    bputc(')', buffer);
}

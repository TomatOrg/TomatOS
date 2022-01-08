#include <mir/mir.h>
#include "jitter.h"

void random_mir_test() {
    MIR_context_t ctx = MIR_init();

    MIR_new_module(ctx, "lol");

    MIR_finish_module(ctx);

    buffer_t* buffer = create_buffer();
    MIR_output(ctx, buffer);
    TRACE("MIR Output:");
    printf("%s", buffer->buffer);
    destroy_buffer(buffer);

    MIR_finish(ctx);

    TRACE("we done here");
}

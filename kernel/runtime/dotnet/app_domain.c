#include "app_domain.h"
#include "proc/scheduler.h"

app_domain_t* create_app_domain() {
    app_domain_t* app = malloc(sizeof(app_domain_t));
    if (app == NULL) return app;
    app->context = MIR_init();
    return app;
}

void app_domain_load(app_domain_t* app, System_Reflection_Assembly assembly) {
    // actually load it

    scheduler_preempt_disable();

    assembly->MirModule->read_index = 0;
    MIR_read(app->context, assembly->MirModule);

    MIR_output(app->context, stdout);

    // load all the symbols for this module
}

void free_app_domain(app_domain_t* app) {
    MIR_finish(app->context);
    free(app);
}

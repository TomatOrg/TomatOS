#include "app_domain.h"

#include "runtime/dotnet/gc/gc.h"

#include <proc/scheduler.h>
#include <util/stb_ds.h>

#include <mir/mir-gen.h>

app_domain_t* create_app_domain() {
    app_domain_t* app = malloc(sizeof(app_domain_t));
    if (app == NULL) return app;
    app->context = MIR_init();

    // setup the gen interface
    MIR_gen_init(app->context, 1);
    MIR_gen_set_optimize_level(app->context, 0, 4);

//    MIR_gen_set_debug_file(app->context, 0, stdout);
//    MIR_gen_set_debug_level(app->context, 0, 0);

    // TODO: isinstance can be made into MIR code

    // load the builtin stuff
    MIR_load_external(app->context, "gc_new", gc_new);
    MIR_load_external(app->context, "gc_update", gc_update);
    MIR_load_external(app->context, "get_array_type", get_array_type);
    MIR_load_external(app->context, "isinstance", isinstance);

    return app;
}

void app_domain_load(app_domain_t* app, System_Reflection_Assembly assembly) {
    // load the module itself
    fseek(assembly->MirModule, 0, SEEK_SET);
    MIR_read(app->context, assembly->MirModule);

    if (assembly->EntryPoint != NULL) {
        app->EntryPoint = assembly->EntryPoint;
    }

    // load all the type references
    for (int i = 0; i < assembly->DefinedTypes->Length; i++) {
        System_Type type = assembly->DefinedTypes->Data[i];
        FILE* name = fcreate();
        type_print_full_name(type, name);
        fputc('\0', name);
        MIR_load_external(app->context, name->buffer, type);
        fclose(name);
    }

    // load all the strings
    for (int i = 0; i < hmlen(assembly->UserStringsTable); i++) {
        // skip null entries
        if (assembly->UserStringsTable[i].value == NULL) {
            continue;
        }
        char name[64];
        snprintf(name, sizeof(name), "string$%d", assembly->UserStringsTable[i].key);
        MIR_load_external(app->context, name, assembly->UserStringsTable[i].value);
    }
}

method_result_t app_domain_link_and_start(app_domain_t* app) {
    FILE* main_func_name = fcreate();
    method_print_full_name(app->EntryPoint, main_func_name);
    fputc('\0', main_func_name);

    // load all the modules
    MIR_item_t main_func = NULL;
    for (
        MIR_module_t module = DLIST_HEAD(MIR_module_t, *MIR_get_module_list(app->context));
        module != NULL;
        module = DLIST_NEXT (MIR_module_t, module)
    ) {
        MIR_load_module (app->context, module);

        // find the main function
        // TODO: only search the main module
        for (
            MIR_item_t func = DLIST_HEAD (MIR_item_t, module->items);
            func != NULL;
             func = DLIST_NEXT (MIR_item_t, func)
        ) {
            if (func->item_type != MIR_func_item) continue;
            if (strcmp (func->u.func->name, main_func_name->buffer) == 0) main_func = func;
        }
    }

    fclose(main_func_name);

    // link it
    MIR_link(app->context, MIR_set_lazy_gen_interface, NULL);

    method_result_t(*main)() = MIR_gen(app->context, 0, main_func);

    return main();
}

void free_app_domain(app_domain_t* app) {
    MIR_gen_finish(app->context);
    MIR_finish(app->context);
    free(app);
}

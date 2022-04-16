#pragma once

#include <runtime/dotnet/types.h>

typedef struct System_AppDomain {
    struct System_Object;
    int Id;
    System_Reflection_Assembly_Array Assemblies;
} *System_AppDomain;

/**
 * Get an AppDomain from its ID
 *
 * @param id    [IN] AppDomain
 */
System_AppDomain get_app_domain_by_id(int id);

System_AppDomain get_current_app_domain();

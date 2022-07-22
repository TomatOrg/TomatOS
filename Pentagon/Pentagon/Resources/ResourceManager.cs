using System;
using System.Collections.Generic;
using Pentagon.DriverServices;

namespace Pentagon.Resources;

public static class ResourceManager<T> 
    where T : class
{

    private static object _lock = new();
    private static List<T> _resources = new();
    private static List<Predicate<T>> _resourceCallbacks = new();

    /// <summary>
    /// Add a new resource to the driver system
    /// </summary>
    /// <param name="resource">The resource to add</param>
    public static void Add(T resource)
    {
        lock (_lock)
        {
            // check if someone wants this resource before we add it to the resource list
            foreach (var cb in _resourceCallbacks)
            {
                // if the callback wants this resource, then give it the resource
                if (cb(resource))
                    return;
            }
            
            // no one wants this resource, just add it to the resource list 
            _resources.Add(resource);
        }
    }

    /// <summary>
    /// Register for resources of this kind, allows driver to see when new devices (or old ones) are
    /// added to the resource manager so it can handle them
    /// </summary>
    /// <param name="callback"></param>
    public static void Register(Predicate<T> callback)
    {
        // TODO: only allow drivers to register callbacks, otherwise we can have a
        //       user DOSing the system by sleeping in a callback...
        
        lock (_lock)
        {
            // first dispatch on all existing resources
            for (var i = 0; i < _resources.Count; i++)
            {
                Log.LogString("CHECKING");
                if (!callback(_resources[i])) 
                    continue;
                
                // the user took this resource, remove it 
                _resources.RemoveAt(i);
                i--;
            }
            
            // add it to the callback list 
            _resourceCallbacks.Add(callback);
        }
    }
    
}
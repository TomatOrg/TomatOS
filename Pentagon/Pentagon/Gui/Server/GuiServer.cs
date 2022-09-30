using System;
using System.Collections.Generic;
using System.Linq.Expressions;
using System.Threading.Tasks;
using Pentagon.DriverServices;

namespace Pentagon.Gui;


public abstract class GuiEvent
{
}

/// <summary>
/// Represents a renderer, which can connect to a server and render
/// everything that is sent by the app
/// </summary>
public abstract class GuiServer
{
    
    /// <summary>
    /// The callback the server should call to pass gui events
    /// </summary>
    public Action<GuiEvent> EventHandler { get; set; }

    // cached scene
    private Func<Scene> _scene = null;
    private Scene _cachedScene = null;

    // scene batching mode
    private bool _isBatch = false;
    private bool _batchSceneUpdated = false;

    // get the scene, null if none
    public Scene Scene
    {
        get
        {
            if (_cachedScene == null && _scene != null)
            {
                _cachedScene = _scene();
            }
            
            
            return _cachedScene;
        }   
    }
    
    /// <summary>
    /// Set the scene to a static scene
    /// </summary>
    public void SetScene(Scene scene)
    {
        _scene = () => scene;
    }

    /// <summary>
    /// Set the scene to a callback
    /// </summary>
    public void SetScene(Func<Scene> scene)
    {
        _scene = scene;
    }
    
    /// <summary>
    /// Refresh the scene
    /// </summary>
    public void RefreshScene()
    {
        _cachedScene = null;
        
        if (_isBatch)
        {
            _batchSceneUpdated = true;
        }
        else
        {
            UpdateScene(Scene);
        }
    }
    
    /// <summary>
    /// Do a batch update
    /// </summary>
    /// <returns></returns>
    public IDisposable BatchUpdate()
    {
        _isBatch = true;
        _batchSceneUpdated = false;
        
        return new AfterBatchUpdate(this);
    }

    private class AfterBatchUpdate : IDisposable
    {

        private GuiServer _server;
        
        public AfterBatchUpdate(GuiServer server)
        {
            _server = server;
        }
        
        public void Dispose()
        {
            if (_server._batchSceneUpdated)
                _server.SendScene();

            _server._isBatch = false;
            _server._batchSceneUpdated = false;
        }
    }

    /// <summary>
    /// Start the gui server serving
    /// </summary>
    public void Serve()
    {
        while (true)
        {
            // accept a new connection
            Accept();

            // send the initial scene
            if (Scene != null)
            {
                SendScene();
            }
            
            // handle the connection
            Handle();
        }
    }
    
    private void SendScene()
    {
        UpdateScene(Scene);
    }

    #region API

    /// <summary>
    /// The renderer may accept a connection now 
    /// </summary>
    public abstract void Accept();

    /// <summary>
    /// The renderer may handle the connection now, should
    /// not return until the connection is dead
    /// </summary>
    public abstract void Handle();

    /// <summary>
    /// Send to the renderer 
    /// </summary>
    public abstract void UpdateScene(Scene scene);

    #endregion
    
}
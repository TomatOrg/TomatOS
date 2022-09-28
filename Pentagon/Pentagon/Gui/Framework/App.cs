using System;
using System.Drawing;
using System.Linq.Expressions;
using System.Threading;
using System.Threading.Tasks;
using Pentagon.DriverServices;
using Pentagon.Gui.Widgets;

namespace Pentagon.Gui.Framework;

public class App
{

    private Func<Widget> _scene;
    private Widget _sceneInstance;

    public App(Func<Widget> scene)
    {
        _scene = scene;
    }
    
    private Scene ForceRebuild()
    {
        return Rebuild();
    }

    private Scene Rebuild()
    {
        if (_sceneInstance == null)
        {
            _sceneInstance = _scene();
        }
        else
        {
            _sceneInstance.Build();
        }

        var builtScene = new Clear(_sceneInstance, Color.Black).BuildRecursively();
        var size = builtScene.Layout(0, 0, Expr.Width, Expr.Height);

        var renderedScene = builtScene.Render(0, 0, size.Item1, size.Item2);

        return new Scene
        {
            Commands = renderedScene,
        };
    }
    
    public void Run(GuiServer gui)
    {
        // we got the renderer, send the initial scene
        gui.EventHandler = EventHandler;
        gui.SetScene(ForceRebuild);
        gui.Serve();
    }
    
    private void EventHandler(GuiEvent e)
    {
        
    }
    
}
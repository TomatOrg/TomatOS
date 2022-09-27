using System;
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

        var builtScene = new ClearWidget(_sceneInstance, 0xff000000).BuildRecursively();
        var size = builtScene.Layout(
            Expression.Constant(0), Expression.Constant(0),
            Expression.Variable(typeof(int), "width"),
            Expression.Variable(typeof(int), "height")
        );

        var renderedScene = builtScene.Render(
            Expression.Constant(0), Expression.Constant(0),
            size.Item1, size.Item2
        );

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
using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using Pentagon.DriverServices;
using Pentagon.Gui.Widgets;
using Pentagon.Interfaces;

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

    int shift = 0, altgr = 0;


    private void EventHandler(GuiEvent e)
    {
        if (e is KeyEvent k)
        {
            if (!k.Released && (k.Code == KeyCode.LeftShift || k.Code == KeyCode.RightShift)) { shift = 1; return; }
            if (k.Released && (k.Code == KeyCode.LeftShift || k.Code == KeyCode.RightShift)) { shift = 0; return; }

            if (!k.Released && (k.Code == KeyCode.RightAlt)) { altgr = 1; return; }
            if (k.Released && (k.Code == KeyCode.RightAlt)) { altgr = 0; return; }

            if (k.Released) return;

            var c = Kernel.GetCodepoint(k.Code, shift > 0, altgr > 0);
            char[] chars = new char[1];
            chars[0] = (char)c;
            Log.LogString(new string(chars));
        }
    }
    
}
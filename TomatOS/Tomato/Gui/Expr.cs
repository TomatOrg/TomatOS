using System;
using Tomato.DriverServices;
using Tomato.Graphics;

namespace Tomato.Gui;

public enum ExprType
{
    IntLiteral,
    InfLiteral,
    Var,
    Add,
    Mul,
    Div,
    Mod,
    Pow,
    Eq,
    Neq,
    Lt,
    Lte,
    Gt,
    Gte,
    BAnd,
    BOr,
    Neg,
    BInvert,
    Min,
    Max,
    Abs,
    If,
    MeasureTextX,
    MeasureTextY,
}

public abstract class Expr
{

    internal static readonly Expr Zero = new IntLiteralExpr(0);
    internal static readonly Expr One = new IntLiteralExpr(1);
    internal static readonly Expr Two = new IntLiteralExpr(2);
    
    public static readonly Expr Width = new VarExpr("width");
    public static readonly Expr Height = new VarExpr("height");

    public static readonly Expr Inf = new InfLiteralExpr();
    
    public abstract ExprType Type { get; }

    #region Int Literals

    public static implicit operator Expr(sbyte value)
    {
        return value switch
        {
            0 => Zero,
            1 => One,
            2 => Two,
            _ => new IntLiteralExpr(value)
        };
    }
    
    public static implicit operator Expr(byte value)
    {
        return value switch
        {
            0 => Zero,
            1 => One,
            2 => Two,
            _ => new IntLiteralExpr(value)
        };
    }

    public static implicit operator Expr(short value)
    {
        return value switch
        {
            0 => Zero,
            1 => One,
            2 => Two,
            _ => new IntLiteralExpr(value)
        };
    }
    
    public static implicit operator Expr(ushort value)
    {
        return value switch
        {
            0 => Zero,
            1 => One,
            2 => Two,
            _ => new IntLiteralExpr(value)
        };
    }
    
    public static implicit operator Expr(int value)
    {
        return value switch
        {
            0 => Zero,
            1 => One,
            2 => Two,
            _ => new IntLiteralExpr(value)
        };
    }
    
    public static implicit operator Expr(uint value)
    {
        return value switch
        {
            0 => Zero,
            1 => One,
            2 => Two,
            _ => new IntLiteralExpr(value)
        };
    }

    public static implicit operator Expr(long value)
    {
        return value switch
        {
            0 => Zero,
            1 => One,
            2 => Two,
            _ => new IntLiteralExpr(value)
        };
    }
    
    // No ulong since long can't contain ulong

    #endregion

    public static Expr operator +(Expr a, Expr b)
    {
        return new AddExpr(a, b).Optimize();
    }
    
    public static Expr operator -(Expr a, Expr b)
    {
        return new AddExpr(a, -b).Optimize();
    }

    public static Expr operator *(Expr a, Expr b)
    {
        return new MulExpr(a, b).Optimize();
    }
    
    public static Expr operator /(Expr a, Expr b)
    {
        return new DivExpr(a, b).Optimize();
    }
    
    public static Expr operator %(Expr a, Expr b)
    {
        return new ModExpr(a, b).Optimize();
    }

    public static Expr operator ==(Expr a, Expr b)
    {
        return new EqExpr(a, b).Optimize();
    }

    public static Expr operator !=(Expr a, Expr b)
    {
        return new NeqExpr(a, b).Optimize();
    }
    
    public static Expr operator <(Expr a, Expr b)
    {
        return new LtExpr(a, b).Optimize();
    }
    
    public static Expr operator <=(Expr a, Expr b)
    {
        return new LteExpr(a, b).Optimize();
    }
    
    public static Expr operator >(Expr a, Expr b)
    {
        return new GtExpr(a, b).Optimize();
    }
    
    public static Expr operator >=(Expr a, Expr b)
    {
        return new GteExpr(a, b).Optimize();
    }
    
    public static Expr operator &(Expr a, Expr b)
    {
        return new BAndExpr(a, b).Optimize();
    }
    
    public static Expr operator |(Expr a, Expr b)
    {
        return new BOrExpr(a, b).Optimize();
    }
    
    public static Expr operator -(Expr a)
    {
        return new NegExpr(a).Optimize();
    }
    
    public static Expr operator ~(Expr a)
    {
        return new BInvertExpr(a).Optimize();
    }

    public Expr Min(Expr other)
    {
        return new MinExpr(this, other).Optimize();
    }

    public Expr Max(Expr other)
    {
        return new MaxExpr(this, other).Optimize();
    }

    public Expr Abs()
    {
        return new AbsExpr(this).Optimize();
    }

    public static Expr If(Expr cond, Expr t, Expr f)
    {
        return new IfExpr(cond, t, f).Optimize();
    }

    public static Expr Var(string name)
    {
        return new VarExpr(name).Optimize();
    }

    public static Expr MeasureTextX(string text, Expr fontSize)
    {
        return new MeasureTextXExpr(text, fontSize);
    }

    public static Expr MeasureTextY(string text, Expr fontSize)
    {
        return new MeasureTextYExpr(text, fontSize);
    }

    public virtual Expr Optimize()
    {
        return this;
    }

    public virtual bool Equals(Expr expr)
    {
        return false;
    }
    
}

public class IntLiteralExpr : Expr
{
    public override ExprType Type => ExprType.IntLiteral;
    
    public long Value { get; }

    public IntLiteralExpr(long value)
    {
        Value = value;
    }

    public override bool Equals(Expr expr)
    {
        return expr.Type == ExprType.IntLiteral && Value == ((IntLiteralExpr)expr).Value;
    }

    public override string ToString()
    {
        return Value switch
        {
            0 => "0",
            1 => "1",
            2 => "2",
            _ => "<int literal>"
        };
    }
}

public sealed class VarExpr : Expr
{
    public override ExprType Type => ExprType.Var;

    public string Name { get; }
    
    public VarExpr(string name)
    {
        Name = name;
    }

    public override bool Equals(Expr expr)
    {
        return expr.Type == ExprType.Var && Name == ((VarExpr)expr).Name;
    }
    
    public override string ToString()
    {
        return Name;
    }
}

public sealed class InfLiteralExpr : Expr
{
    public override ExprType Type => ExprType.InfLiteral;

    public InfLiteralExpr()
    {
    }

    public override bool Equals(Expr expr)
    {
        return expr.Type == ExprType.InfLiteral;
    }
    
    public override string ToString()
    {
        return "inf";
    }
}


public sealed class IfExpr : Expr
{

    public override ExprType Type => ExprType.If;

    public Expr Condition { get; }
    public Expr True { get; }
    public Expr False { get; }

    public IfExpr(Expr cond, Expr t, Expr f)
    {
        Condition = cond;
        True = t;
        False = f;
    }
    
    public override string ToString()
    {
        return $"if ({Condition.ToString()}) {{ {True.ToString()} }} else {{ {False.ToString()} }}";
    }
}

public abstract class UnaryExpr : Expr
{
    public Expr A { get; }
    
    public UnaryExpr(Expr a)
    {
        A = a;
    }

    public override bool Equals(Expr expr)
    {
        return Type == expr.Type && A.Equals(((UnaryExpr)expr).A);
    }
}

public abstract class BinaryExpr : Expr
{
    public Expr A { get; }
    public Expr B { get; }

    public BinaryExpr(Expr a, Expr b)
    {
        A = a;
        B = b;
    }

    public override bool Equals(Expr expr)
    {
        if (Type != expr.Type)
        {
            return false;
        }

        var be = (BinaryExpr)expr;
        return A.Equals(be.A) && B.Equals(be.B);
    }
}

public sealed class AddExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Add;

    public AddExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }

    public override Expr Optimize()
    {
#if DONT_OPTIMIZE_EXPR
        return this;
#endif
        
        // x + -x == 0
        if (B.Type == ExprType.Neg)
        {
            var b = (NegExpr)B;
            if (A.Equals(b.A))
            {
                return Zero;
            }
        }
        
        // x + 0 == x
        if (B.Equals(Zero)) 
            return A;
        if (A.Equals(Zero)) 
            return B;

        // a + b == (a + b)
        if (A.Type == ExprType.IntLiteral && B.Type == ExprType.IntLiteral)
        {
            var a = ((IntLiteralExpr)A).Value;
            var b = ((IntLiteralExpr)A).Value;
            return a + b;
        }
        
        // (x + a) + b == x + (a + b)
        if (A.Type == ExprType.Add && B.Type == ExprType.IntLiteral)
        {
            var add = (AddExpr)A;
            if (add.B.Type == ExprType.IntLiteral)
            {
                var a = ((IntLiteralExpr)add.B).Value;
                var b = ((IntLiteralExpr)B).Value;
                return add.A + (a + b);
            }
        }
        
        // a + x == x + a
        if (A.Type == ExprType.IntLiteral) 
            return B + A;
        
        // x + inf = inf
        if (A.Equals(Inf) || B.Equals(Inf))
            return Inf;
        
        return this;
    }
    
    public override string ToString()
    {
        return $"({A.ToString()} + {B.ToString()})";
    }
}

public sealed class MulExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Mul;

    public MulExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }

    public override Expr Optimize()
    {
#if DONT_OPTIMIZE_EXPR
        return this;
#endif
        
        // x * 1 == x
        if (B.Equals(One))
            return A;
        
        return this;
    }
    
    public override string ToString()
    {
        return $"({A.ToString()} * {B.ToString()})";
    }
}

public sealed class DivExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Div;

    public DivExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }

    public override Expr Optimize()
    {
#if DONT_OPTIMIZE_EXPR
        return this;
#endif
        
        // x / 1 == x
        if (B.Equals(One))
            return A;
        
        // 0 / x == 0
        if (A.Equals(Zero))
            return Zero;
        
        // (x + x) / 2 == x
        if (A.Type == ExprType.Add && B.Equals(Two))
        {
            var add = (AddExpr)A;
            if (add.A.Equals(add.B))
            {
                return add.A;
            }
        }
        
        return this;
    }
    
    public override string ToString()
    {
        return $"({A.ToString()} / {B.ToString()})";
    }
}

public sealed class ModExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Mod;

    public ModExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }
    
    public override string ToString()
    {
        return $"({A.ToString()} % {B.ToString()})";
    }
}

public sealed class PowExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Pow;

    public PowExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }
    
    public override string ToString()
    {
        return $"({A.ToString()} ** {B.ToString()})";
    }
}

public sealed class EqExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Eq;

    public EqExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }    
    
    public override string ToString()
    {
        return $"({A.ToString()} == {B.ToString()})";
    }
}

public sealed class NeqExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Neq;

    public NeqExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }

    public override string ToString()
    {
        return $"({A.ToString()} != {B.ToString()})";
    }
}

public sealed class LtExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Lt;

    public LtExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }
    
    public override string ToString()
    {
        return $"({A.ToString()} < {B.ToString()})";
    }
}

public sealed class LteExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Lte;

    public LteExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }    
    
    public override string ToString()
    {
        return $"({A.ToString()} <= {B.ToString()})";
    }
}

public sealed class GtExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Gt;

    public GtExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }
        
    public override string ToString()
    {
        return $"({A.ToString()} > {B.ToString()})";
    }

}

public sealed class GteExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Gte;

    public GteExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }
    
    public override string ToString()
    {
        return $"({A.ToString()} >= {B.ToString()})";
    }

}

public sealed class BAndExpr : BinaryExpr
{

    public override ExprType Type => ExprType.BAnd;

    public BAndExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }
    
    public override string ToString()
    {
        return $"({A.ToString()} & {B.ToString()})";
    }
}

public sealed class BOrExpr : BinaryExpr
{

    public override ExprType Type => ExprType.BOr;

    public BOrExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }
    
    public override string ToString()
    {
        return $"({A.ToString()} | {B.ToString()})";
    }
}

public sealed class NegExpr : UnaryExpr
{

    public override ExprType Type => ExprType.Neg;

    public NegExpr(Expr a) 
        : base(a)
    {
    }

    public override Expr Optimize()
    {
        // -a == -a
        if (A.Type == ExprType.IntLiteral)
        {
            return -((IntLiteralExpr)A).Value;
        }
        
        return this;
    }

    public override string ToString()
    {
        return $"-{A.ToString()}";
    }
}

public sealed class BInvertExpr : UnaryExpr
{

    public override ExprType Type => ExprType.BInvert;

    public BInvertExpr(Expr a) 
        : base(a)
    {
    }
    
    public override string ToString()
    {
        return $"~{A.ToString()}";
    }
}

public sealed class MinExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Min;

    public MinExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }

    public override Expr Optimize()
    {
#if DONT_OPTIMIZE_EXPR
        return this;
#endif
        
        // min(x, inf) == x
        if (B.Equals(Inf)) return A;
        if (A.Equals(Inf)) return B;

        // min(x, y) == min(x, y)
        if (A.Type == ExprType.IntLiteral && B.Type == ExprType.IntLiteral)
            return Math.Min(((IntLiteralExpr)A).Value, ((IntLiteralExpr)B).Value);
        
        return this;
    }
    
    public override string ToString()
    {
        return $"min({A.ToString()}, {B.ToString()})";
    }
}

public sealed class MaxExpr : BinaryExpr
{

    public override ExprType Type => ExprType.Max;

    public MaxExpr(Expr a, Expr b) 
        : base(a, b)
    {
    }

    public override Expr Optimize()
    {
#if DONT_OPTIMIZE_EXPR
        return this;
#endif
        
        // max(x, inf) == inf
        if (A.Equals(Inf) || B.Equals(Inf))
            return Inf;

        // max(x, y) == max(x, y)
        if (A.Type == ExprType.IntLiteral && B.Type == ExprType.IntLiteral)
        {
            return Math.Max(((IntLiteralExpr)A).Value, ((IntLiteralExpr)B).Value);
        }
        
        // max(-c, measureText()) == measureText(...)
        // MeasureText is non-negative
        if (A.Type == ExprType.IntLiteral && B.Type is ExprType.MeasureTextX or ExprType.MeasureTextY)
        {
            var c = ((IntLiteralExpr)A).Value;
            if (c <= 0)
            {
                return B;
            }
        }

        if (B.Type == ExprType.IntLiteral && A.Type is ExprType.MeasureTextX or ExprType.MeasureTextY)
        {
            var c = ((IntLiteralExpr)B).Value;
            if (c <= 0)
            {
                return A;
            }
        }

        return this;
    }
    
    public override string ToString()
    {
        return $"max({A.ToString()}, {B.ToString()})";
    }

}

public sealed class AbsExpr : UnaryExpr
{

    public override ExprType Type => ExprType.Abs;

    public AbsExpr(Expr a) 
        : base(a)
    {
    }

    public override string ToString()
    {
        return $"abs({A.ToString()})";
    }
}

public sealed class MeasureTextXExpr : Expr
{
    public override ExprType Type => ExprType.MeasureTextX;

    public string Text { get; }
    public Expr FontSize { get; }

    public MeasureTextXExpr(string text, Expr fontSize)
    {
        Text = text;
        FontSize = fontSize;
    }

    public override string ToString()
    {
        return $"measureTextX(text={Text}, fontSize={FontSize})";
    }
}

public sealed class MeasureTextYExpr : Expr
{
    public override ExprType Type => ExprType.MeasureTextY;

    public string Text { get; }
    public Expr FontSize { get; }

    public MeasureTextYExpr(string text, Expr fontSize)
    {
        Text = text;
        FontSize = fontSize;
    }

    public override string ToString()
    {
        return $"measureTextY(text={Text}, fontSize={FontSize})";
    }
}

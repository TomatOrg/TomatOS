namespace System;

public readonly struct TimeSpan
{

    public const long TicksPerDay = 864000000000;
    public const long TicksPerHour = 36000000000;
    public const long TicksPerMillisecond = 10000;
    public const long TicksPerMinute = 600000000;
    public const long TicksPerSecond = 10000000;
    
    public static readonly TimeSpan MaxValue = new TimeSpan(long.MaxValue);
    public static readonly TimeSpan MinValue = new TimeSpan(long.MinValue);
    public static readonly TimeSpan Zero = new TimeSpan(0);

    public double TotalDays => (double)Ticks / TicksPerDay;
    public double TotalHours => (double)Ticks / TicksPerHour;
    public double TotalMilliseconds => (double)Ticks / TicksPerMillisecond;
    public double TotalMinutes => (double)Ticks / TicksPerMinute;
    public double TotalSeconds => (double)Ticks / TicksPerSecond;
    
    public int Days => (int)(Ticks / TicksPerDay);
    public int Hours => (int)(Ticks / TicksPerHour % 24);
    public int Milliseconds => (int)(Ticks / TicksPerMillisecond % 1000);
    public int Minutes => (int)(Ticks / TicksPerMinute % 60);
    public int Seconds => (int)(Ticks / TicksPerSecond % 60);

    public long Ticks { get; }
    
    public TimeSpan(long ticks)
    {
        Ticks = ticks;
    }

    public static TimeSpan FromMilliseconds(double value)
    {
        // TODO: check for NaN
        return new TimeSpan((long)(value * TicksPerMillisecond));
    }

}
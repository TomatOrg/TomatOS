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
    public TimeSpan(int days, int hours, int minutes, int seconds, int milliseconds) :
            this(days, hours, minutes, seconds, milliseconds, 0)
    {
    }

    public TimeSpan(int days, int hours, int minutes, int seconds, int milliseconds, int microseconds)
    {
        long totalMicroseconds = (((long)days * 3600 * 24 + (long)hours * 3600 + (long)minutes * 60 + seconds) * 1000 + milliseconds) * 1000 + microseconds;
        Ticks = totalMicroseconds * (TicksPerMillisecond * 1000);
    }

    public static TimeSpan FromMilliseconds(double value)
    {
        // TODO: check for NaN
        return new TimeSpan((long)(value * TicksPerMillisecond));
    }

}
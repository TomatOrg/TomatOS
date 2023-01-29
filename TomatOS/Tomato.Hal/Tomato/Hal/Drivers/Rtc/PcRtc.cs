using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Tomato.Hal.Acpi.Resource;
using Tomato.Hal.Interfaces;
using Tomato.Hal.Managers;

namespace Tomato.Hal.Drivers.Rtc;

public class PcRtc : ITimeProvider
{
    [Flags]
    private enum RegB
    {
        Dse = 1 << 0,
        Mil = 1 << 1,
        Dm = 1 << 2,
        Sqwe = 1 << 3,
        Uie = 1 << 4,
        Aie = 1 << 5,
        Pie = 1 << 6,
        Set = 1 << 7,
    }
    
    private const int Seconds = 0;
    private const int SecondsAlarm = 1;
    private const int Minutes = 2;
    private const int MinutesAlarm = 3;
    private const int Hours = 4;
    private const int HoursAlarm = 5;
    private const int DayOfTheWeek = 6;
    private const int DayOfTheMonth = 7;
    private const int Month = 8;
    private const int Year = 9;
    private const int RegisterA = 10;
    private const int RegisterB = 11;
    private const int RegisterC = 12;
    private const int RegisterD = 13;

    /// <summary>
    /// Protect the RTC registers by not allowing MP access 
    /// </summary>
    private SemaphoreSlim _semaphoreSlim = new SemaphoreSlim(1, 1);

    private IoPort _indexPort;
    private IoPort _targetPort;
    
    public PcRtc(IoResource port)
    {
        _indexPort = port[0];
        _targetPort = port[1];

        // check the device functions correctly 
        if ((Read(RegisterD) & 0x80) != 0)
        {
            TimeManager.RegisterTimeProvider(this);
        }
    }

    private byte Read(byte offset)
    {
        _indexPort.WriteByte(offset);
        return _targetPort.ReadByte();
    }

    private static byte Bcd8ToDecimal8(byte value)
    {
        return (byte)((value >> 4) * 10 + (value & 0xf));
    }

    private async Task WaitToUpdate()
    {
        // wait for up to 0.1 seconds, sleeping for 1ms
        // at each iteration, this should be more than enough
        for (var i = 0; i < 100; i++)
        {
            if ((Read(RegisterA) & (1 << 7)) == 0)
            {
                return;
            }
            await Task.Delay(1);
        }

        throw new TimeoutException();
    }

    public async Task<DateTime> GetCurrentTime()
    {
        RegB regB = 0;
        byte second = 0;
        byte minute = 0;
        byte hour = 0;
        byte day = 0;
        byte month = 0;
        var year = 0;

        // read the time atomically 
        await _semaphoreSlim.WaitAsync();
        try
        {
            // wait for the rtc to update before we are trying to read from it 
            await WaitToUpdate();
            
            // now read it 
            regB = (RegB)Read(RegisterB);
            second = Read(Seconds);
            minute = Read(Minutes);
            hour = Read(Hours);
            day = Read(DayOfTheMonth);
            month = Read(Month);
            year = Read(Year);
        }
        finally
        {
            _semaphoreSlim.Release();
        }

        // check if in 12 hour mode
        var isPm = false;
        if ((regB & RegB.Mil) == 0)
        {
            isPm = (hour & 0x80) != 0;
            hour &= 0x7f;
        }

        // bcd -> decimal
        if ((regB & RegB.Dm) == 0)
        {
            year = Bcd8ToDecimal8((byte)year);
            month = Bcd8ToDecimal8(month);
            day = Bcd8ToDecimal8(day);
            hour = Bcd8ToDecimal8(hour);
            minute = Bcd8ToDecimal8(minute);
            second = Bcd8ToDecimal8(second);
        }
        
        // TODO: handle century, for now hard-code 20
        const byte century = 20;
        year = century * 100 + year;
        
        // now finalize the 12 hour -> 24 hour format
        if ((regB & RegB.Mil) == 0)
        {
            hour = isPm switch
            {
                true when hour < 12 => (byte)(hour + 12),
                false when hour == 12 => 0,
                _ => hour
            };
        }

        // TODO: we might want to have the ability to specify if this holds the
        // TODO: local or UTC time, in theory this should not be too hard
        return new DateTime(year, month, day, hour, minute, second, DateTimeKind.Utc);
    }
}
using System;
using System.Threading.Tasks;

namespace Tomato.Hal.Interfaces;

public interface ITimeProvider
{

    public Task<DateTime> GetCurrentTime();

}
using System;
using System.Runtime.InteropServices;
using Pentagon.Resources;

namespace Pentagon.DriverServices;

public static class IoApic
{
    public static void Scan(Acpi.Acpi acpi)
    {
        var madtMem = acpi.FindTable(Acpi.Madt.Signature);
        var madt = new Acpi.Madt(madtMem);

        foreach (var ioapic in madt.IoApics)
        {
            Log.LogString("IOAPIC Id=");
            Log.LogHex(ioapic.Id);
            Log.LogString(" Address=");
            Log.LogHex(ioapic.Address);
            Log.LogString(" GsiBase=");
            Log.LogHex(ioapic.GsiBase);
            Log.LogString("\n");
        }
        foreach (var iso in madt.Isos)
        {
            Log.LogString("ISO BusSource=");
            Log.LogHex(iso.BusSource);
            Log.LogString(" IrqSource=");
            Log.LogHex(iso.IrqSource);
            Log.LogString(" Gsi=");
            Log.LogHex(iso.Gsi);
            Log.LogString("\n");
        }
    }
}

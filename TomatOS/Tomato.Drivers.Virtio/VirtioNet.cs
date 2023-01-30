using System;
using System.Runtime.InteropServices;
using System.Buffers;
using System.Threading;
using System.Diagnostics;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;
using Tomato.Hal.Interfaces;
using Tomato.Hal.Managers;
using Tomato.Hal.Pci;
using Tomato.Hal.Io;
using Tomato.Hal;
using TinyDotNet;
using static Tomato.Hal.MemoryServices;

namespace Tomato.Drivers.Virtio;

// Modern adn transitional PCI vendor-device pairs
// Look at 4.1 Virtio over PCI Bus and 5. Device types in the 1.2 spec
[PciDriver(0x1AF4, 0x1041)]
[PciDriver(0x1AF4, 0x1000)]
public class VirtioNet : VirtioPci
{
    static readonly IpV4 OurIp = new IpV4(10, 1, 1, 1);

    const uint VIRTIO_NET_F_MAC = 1u << 5;
    const uint RequiredFeatures = VIRTIO_NET_F_MAC;

    VirtioNetConfig _devConfig;

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct Mac
    {
        internal FixedArray6<byte> Data;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct IpV4
    {
        internal FixedArray4<byte> Data;
        internal IpV4(byte a, byte b, byte c, byte d) {
            Data = new FixedArray4<byte>();
            Data[0] = a;
            Data[1] = b;
            Data[2] = c;
            Data[3] = d;
        }
    }

    internal class VirtioNetConfig
    {
        internal Field<Mac> MacAddr;

        internal VirtioNetConfig(Region r)
        {
            MacAddr = r.CreateField<Mac>(0);
        }
    }

    Memory<byte>[] _netPacketsIn;

    public VirtioNet(PciDevice a) : base(a, RequiredFeatures)
    {
        _devConfig = new VirtioNetConfig(_devCfgRegion);

        var iwRx = new Thread(() => RxWaiterThread(0));
        iwRx.Name = $"VirtioNet IRQWaiter {0} (RX)";
        iwRx.Start();

        var iwTx = new Thread(() => TxWaiterThread(1));
        iwTx.Name = $"VirtioNet IRQWaiter {1} (TX)";
        iwTx.Start();

        _netPacketsIn = new Memory<byte>[_queueInfo[0].Descriptors.Length];
        for (ushort i = 0; i < _queueInfo[0].Descriptors.Length; i++)
        {
            var mem = MemoryServices.AllocatePhysicalMemory(4096).Memory;
            _netPacketsIn[i] = mem;
            var address = MemoryServices.GetMappedPhysicalAddress(mem);
            _queueInfo[0].Descriptors.Span[i].Phys = address;
            _queueInfo[0].Descriptors.Span[i].Len = 4096;
            _queueInfo[0].Descriptors.Span[i].Flags = QueueInfo.Descriptor.Flag.Write;
            _queueInfo[0].PlaceHeadOnAvail(i);
        }
        _queueInfo[0].Notify();

        var mac = _devConfig.MacAddr.Value;
        Debug.Print($"VirtioNet: MAC {mac.Data[0]:X2}:{mac.Data[1]:X2}:{mac.Data[2]:X2}:{mac.Data[3]:X2}:{mac.Data[4]:X2}:{mac.Data[5]:X2}");
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct NetHdr
    {
        internal byte Flags;
        internal byte GsoType;
        internal ushort HdrLen;
        internal ushort GsoSize;
        internal ushort CsumStart;
        internal ushort CsumOffset;
        internal ushort NumBuffers;
    }

    void RxWaiterThread(int n)
    {
        ref var q = ref _queueInfo[n];
        while (true)
        {
            q.Interrupt.Wait();
            // 1.2 spec, 2.7.14 Receiving Used Buffers From The Device
            while (q.LastSeenUsed != q.Used.DescIdx.Value)
            {
                // get
                var head = q.Used.Ring.Span[q.LastSeenUsed % q.Size].Id;
                var pkt = _netPacketsIn[head];
                //Debug.Print($"got packet with head {head}");
                var b = new Buf(pkt, q.Descriptors.Span[head].Phys);
                ref var virtioHdr = ref b.Pop<NetHdr>();
                EthernetHandle(b);

                // no need to free the buffer and virtqueue data
                // as it's reused
                // but we need to put it back in the available ring
                q.PlaceHeadOnAvail(head);

                q.LastSeenUsed++;
            }
            // update the available ring in a single vmexit
            q.Notify();
        }
    }

    void TxWaiterThread(int n)
    {
        ref var q = ref _queueInfo[n];
        while (true)
        {
            q.Interrupt.Wait();
            // 1.2 spec, 2.7.14 Receiving Used Buffers From The Device
            while (q.LastSeenUsed != q.Used.DescIdx.Value)
            {
                // get
                var head = q.Used.Ring.Span[q.LastSeenUsed % q.Size].Id;
                //Debug.Print($"transmitted packet with head {head}");

                // free virtio-related bookkeeping
                q.FreeChain(head);
                q.LastSeenUsed++;
            }
        }
    }

    public static ushort SwapBytes(ushort word)
    {
        return (ushort)(((word >> 8) & 0x00FF) | ((word << 8) & 0xFF00));
    }
    public static uint SwapBytes(uint word)
    {
        return ((word >> 24) & 0x000000FF) | ((word >> 8) & 0x0000FF00) | ((word << 8) & 0x00FF0000) | ((word << 24) & 0xFF000000);
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct EthernetHeader
    {
        internal Mac Dest;
        internal Mac Src;
        internal ushort _etherType;

        internal EtherTypeEnum EtherType { get => (EtherTypeEnum)SwapBytes(_etherType); set => _etherType = SwapBytes((ushort)value); }

        internal enum EtherTypeEnum : ushort
        {
            IpV4 = 0x800,
            Arp = 0x806,
            IpV6 = 0x86dd,
        }
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct ArpHeader
    {
        internal ushort _hwAddrFormat;
        internal ushort _protAddrFormat;
        internal byte HwAddrLength;
        internal byte ProtAddrLength;
        internal ushort _op;

        internal HwAddrFormatEnum HwAddrFormat { get => (HwAddrFormatEnum)SwapBytes(_hwAddrFormat); set => _hwAddrFormat = SwapBytes((ushort)value); }
        internal EthernetHeader.EtherTypeEnum ProtAddrFormat { get => (EthernetHeader.EtherTypeEnum)SwapBytes(_protAddrFormat); set => _protAddrFormat = SwapBytes((ushort)value); }
        internal OpEnum Op { get => (OpEnum)SwapBytes(_op); set => _op = SwapBytes((ushort)value); }

        internal enum HwAddrFormatEnum : ushort
        {
            Ether = 1,
            Iee80211 = 6,
            Infiniband = 32,
        }

        internal enum OpEnum : ushort
        {
            Request = 1,
            Reply = 2,
        }
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct IpV4Header
    {
        internal byte _versionIhl;
        internal byte Tos;
        internal ushort _length;
        internal ushort _id;
        internal ushort _fragOffset;
        internal byte Ttl;
        internal ProtocolEnum Protocol;
        internal ushort Check; // this is funny, this does not need to be endian-swapped, because the checksum function already reads endian-natively
        internal IpV4 Src;
        internal IpV4 Dest;

        internal byte Ihl { get => (byte)(_versionIhl & 0xF); set => _versionIhl |= (byte)(value & 0xF); }
        internal byte Version { get => (byte)(_versionIhl >> 4); set => _versionIhl |= (byte)(value << 4); }
        internal ushort Length { get => SwapBytes(_length); set => _length = SwapBytes(value); }
        internal ushort Id { get => SwapBytes(_id); set => _id = SwapBytes(value); }

        // TODO: IPv6 uses this too
        internal enum ProtocolEnum : byte
        {
            Icmp = 0x01,
            Tcp = 0x06,
            Udp = 0x11,
        }
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct IcmpV4Header
    {
        internal TypeEnum Type;
        internal byte Code;
        internal ushort Check;
        internal enum TypeEnum : byte
        {
            EchoReply = 0,
            DestinationUnreachable = 3,
            EchoRequest = 8,
        }
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct IcmpV4Echo
    {
        internal ushort _id;
        internal ushort _seq;
    }


    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct ArpEthIpV4
    {
        internal Mac SenderHwAddr;
        internal IpV4 SenderProtAddr;
        internal Mac TargetHwAddr;
        internal IpV4 TargetProtAddr;
    }

    void EthernetHandle(Buf b)
    {
        ref var ethHdr = ref b.Pop<EthernetHeader>();

        if (ethHdr.EtherType == EthernetHeader.EtherTypeEnum.Arp)
        {
            ArpHandle(b);
        }
        else if (ethHdr.EtherType == EthernetHeader.EtherTypeEnum.IpV4)
        {
            IpV4Handle(b, ethHdr.Src);
        }
    }

    void IpV4Handle(Buf b, Mac senderHw)
    {
        ref var ipHdr = ref b.Pop<IpV4Header>();
        var extra = b.Pop(ipHdr.Ihl * 4 - Unsafe.SizeOf<IpV4Header>());

        // TODO: checksum and TTL check
        // TODO: fragmentation

        if (ipHdr.Protocol == IpV4Header.ProtocolEnum.Icmp)
        {
            IcmpV4Handle(b.Slice((ushort)(ipHdr.Length - (ipHdr.Ihl * 4))), ipHdr.Id, ipHdr.Src, senderHw);
        }
    }

    void IcmpV4Handle(Buf b, ushort id, IpV4 sender, Mac senderHw)
    {
        ref var icmpHdr = ref b.Pop<IcmpV4Header>();
        if (icmpHdr.Type == IcmpV4Header.TypeEnum.EchoRequest)
        {
            IcmpV4EchoReply(b.Get(), id, sender, senderHw);
        }
    }
    ushort IpChecksum(Span<byte> data)
    {
        var words = MemoryMarshal.Cast<byte, ushort>(data);
        uint sum = 0;

        for (int i = 0; i < words.Length; i++)
        {
            sum += words[i];
        }

        // odd number of bytes, add the lone byte
        if ((data.Length & 1) != 0)
            sum += data[data.Length - 2];

        // fold bytes
        while ((sum >> 16) != 0)
            sum = (sum & 0xffff) + (sum >> 16);

        return (ushort)(~sum);
    }
    void IcmpV4EchoReply(Span<byte> data, ushort id, IpV4 sender, Mac senderHw)
    {
        var b = new Buf();
        b.Push(data);

        ref var icmpHdr = ref b.Push<IcmpV4Header>();
        icmpHdr.Type = IcmpV4Header.TypeEnum.EchoReply;
        icmpHdr.Code = 0;
        icmpHdr.Check = 0;
        icmpHdr.Check = IpChecksum(b.Get());

        ref var ipHdr = ref b.Push<IpV4Header>();
        ipHdr.Version = 4;
        ipHdr.Ihl = (byte)(Unsafe.SizeOf<IpV4Header>() / 4);
        ipHdr.Tos = 0;
        ipHdr.Length = (byte)(Unsafe.SizeOf<IpV4Header>() + Unsafe.SizeOf<IcmpV4Header>() + data.Length);
        ipHdr.Id = id;
        ipHdr._fragOffset = 0;
        ipHdr.Ttl = 64;
        ipHdr.Protocol = IpV4Header.ProtocolEnum.Icmp;
        ipHdr.Src = OurIp;
        ipHdr.Dest = sender;
        ipHdr.Check = 0;
        ipHdr.Check = IpChecksum(b.Get().Slice(0, Unsafe.SizeOf<IpV4Header>()));

        ref var ethHdr = ref b.Push<EthernetHeader>();
        ethHdr.Dest = senderHw;
        ethHdr.Src = _devConfig.MacAddr.Value;
        ethHdr.EtherType = EthernetHeader.EtherTypeEnum.IpV4;

        Send(b);
    }

    void ArpHandle(Buf b)
    {
        ref var arpHdr = ref b.Pop<ArpHeader>();
        Debug.Assert(arpHdr.HwAddrFormat == ArpHeader.HwAddrFormatEnum.Ether);
        Debug.Assert(arpHdr.ProtAddrFormat == EthernetHeader.EtherTypeEnum.IpV4);
        Debug.Assert(arpHdr.HwAddrLength == 6);
        Debug.Assert(arpHdr.ProtAddrLength == 4);
        Debug.Assert(arpHdr.Op == ArpHeader.OpEnum.Request);

        ref var arpData = ref b.Pop<ArpEthIpV4>();

        var mac = arpData.SenderHwAddr;
        var ip = arpData.SenderProtAddr;
        //Debug.Print($"MAC {mac.A:X2}:{mac.B:X2}:{mac.C:X2}:{mac.D:X2}:{mac.E:X2}:{mac.F:X2} is IP {ip.A}.{ip.B}.{ip.C}.{ip.D}");

        ArpReply(arpData.SenderHwAddr, arpData.SenderProtAddr);
    }

    internal struct Buf
    {
        internal Memory<byte> _data;
        internal ulong _phys;
        internal int _curr;
        public Buf()
        {
            _data = MemoryServices.AllocatePhysicalMemory(4096).Memory;
            _phys = MemoryServices.GetMappedPhysicalAddress(_data);
            _curr = _data.Length;
        }

        public Buf(Memory<byte> data, ulong phys)
        {
            _data = data;
            _phys = phys;
            _curr = 0;
        }

        internal ref T Push<T>() where T : unmanaged
        {
            _curr -= Unsafe.SizeOf<T>();
            return ref MemoryMarshal.Cast<byte, T>(_data.Span.Slice(_curr))[0];
        }

        internal void Push(Span<byte> toPush)
        {
            _curr -= toPush.Length;
            toPush.CopyTo(_data.Span.Slice(_curr, toPush.Length));
        }

        internal ref T Pop<T>() where T : unmanaged
        {
            var data = _data.Span.Slice(_curr);
            _curr += Unsafe.SizeOf<T>();
            return ref MemoryMarshal.Cast<byte, T>(data)[0];
        }
        internal Span<byte> Pop(int c)
        {
            var data = _data.Span.Slice(_curr, c);
            _curr += c;
            return data;
        }

        internal Span<byte> Get()
        {
            return _data.Span.Slice(_curr);
        }

        internal Buf Slice(int len) {
            Buf n;
            n._data = _data.Slice(_curr, len);
            n._phys = _phys + (ulong)_curr;
            n._curr = 0;
            return n;
        }
    }

    void Send(Buf b)
    {
        ref var virtioHdr = ref b.Push<NetHdr>();
        virtioHdr.NumBuffers = 0;
        virtioHdr.Flags = 0;
        var head = _queueInfo[1].GetNewDescriptor();
        _queueInfo[1].Descriptors.Span[head].Phys = b._phys + (ulong)b._curr;
        _queueInfo[1].Descriptors.Span[head].Len = (uint)(4096 - b._curr);
        _queueInfo[1].Descriptors.Span[head].Flags = 0;
        _queueInfo[1].PlaceHeadOnAvail(head);
        _queueInfo[1].Notify();
    }

    void ArpReply(Mac hwAddr, IpV4 protAddr)
    {
        var b = new Buf();

        ref var arpData = ref b.Push<ArpEthIpV4>();
        arpData.TargetHwAddr = hwAddr;
        arpData.TargetProtAddr = protAddr;
        arpData.SenderHwAddr = _devConfig.MacAddr.Value;
        arpData.SenderProtAddr = OurIp;

        ref var arpHdr = ref b.Push<ArpHeader>();
        arpHdr.HwAddrFormat = ArpHeader.HwAddrFormatEnum.Ether;
        arpHdr.ProtAddrFormat = EthernetHeader.EtherTypeEnum.IpV4;
        arpHdr.HwAddrLength = (byte)Unsafe.SizeOf<Mac>();
        arpHdr.ProtAddrLength = (byte)Unsafe.SizeOf<IpV4>();
        arpHdr.Op = ArpHeader.OpEnum.Reply;
        
        ref var ethHdr = ref b.Push<EthernetHeader>();
        ethHdr.Dest = hwAddr;
        ethHdr.Src = _devConfig.MacAddr.Value;
        ethHdr.EtherType = EthernetHeader.EtherTypeEnum.Arp;

        Send(b);
    }
}

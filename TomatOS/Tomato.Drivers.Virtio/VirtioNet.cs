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
using System.Text;
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
        internal Mac(byte a, byte b, byte c, byte d, byte e, byte f)
        {
            Data = new FixedArray6<byte>();
            Data[0] = a;
            Data[1] = b;
            Data[2] = c;
            Data[3] = d;
            Data[4] = e;
            Data[5] = f;
        }
        internal static Mac Broadcast = new Mac(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct IpV4 : IEquatable<IpV4>
    {
        internal FixedArray4<byte> Data;
        internal IpV4(byte a, byte b, byte c, byte d)
        {
            Data = new FixedArray4<byte>();
            Data[0] = a;
            Data[1] = b;
            Data[2] = c;
            Data[3] = d;
        }
        public override int GetHashCode()
        {
            unchecked
            {
                return Data[0] + Data[1] * 256 + Data[2] * 256 * 256 + Data[3] * 256 * 256 * 256;
            }
        }

        public override bool Equals(object obj)
        {
            return obj is IpV4 v && Equals(v);
        }

        public bool Equals(IpV4 other)
        {
            return Data[0] == other.Data[0] && Data[1] == other.Data[1] && Data[2] == other.Data[2] && Data[3] == other.Data[3];
        }

        public static bool operator ==(IpV4 left, IpV4 right)
        {
            return left.Equals(right);
        }

        public static bool operator !=(IpV4 left, IpV4 right)
        {
            return !(left == right);
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
        Test().Wait();
    }

    struct IPEndPoint
    {
        internal ushort Port;
        internal IpV4 Address;
        internal IPEndPoint(IpV4 addr, ushort p) {
            Address = addr;
            Port = p;
        }
    }

    struct UdpReceiveResult
    {
        internal Buf Buffer;
        internal IPEndPoint RemoteEndPoint;
    }

    static TaskCompletionSource<UdpReceiveResult> Tcs;

    class UdpClient
    {
        VirtioNet vn;
        ushort OurPort = 1234;

        internal UdpClient(VirtioNet v)
        {
            vn = v;
        }
        internal Task<UdpReceiveResult> ReceiveAsync()
        {
            var tcs = new TaskCompletionSource<UdpReceiveResult>(TaskCreationOptions.RunContinuationsAsynchronously);
            VirtioNet.Tcs = tcs;
            return tcs.Task;
        }
        internal Task SendAsync(Buf b, IPEndPoint ep)
        {
            vn.UdpSend(b, OurPort, ep.Address, ep.Port);
            return Task.CompletedTask;
        }

    }

    internal async Task Test()
    {
        var c = new UdpClient(this);
        var endpoint = new IPEndPoint(new IpV4(10, 1, 1, 2), 69);

        var start = Stopwatch.GetTimestamp();

        _ = c.SendAsync(TftpRrq(), endpoint);

        ulong rxSize = 0;

        while (true)
        {
            var rx = await c.ReceiveAsync();
            var buf = rx.Buffer;
            var opcode = (TftpOpcodeEnum)SwapBytes(buf.Pop<ushort>());
            if (opcode == TftpOpcodeEnum.Data)
            {
                var blockIdx = SwapBytes(buf.Pop<ushort>());
                _ = c.SendAsync(TftpAck(blockIdx), rx.RemoteEndPoint);
                var length = buf.Get().Length;
                rxSize += (ulong)length;
                if (length != 512)
                {
                    var end = Stopwatch.GetTimestamp();
                    float time = ((float)(end - start)) / Stopwatch.Frequency;
                    Debug.Print($"Completed transfer in {time}s at {rxSize / time / (1024 * 1024)}MB/s");
                }
            }
        }
    }

    Buf TftpRrq()
    {
        var b = new Buf();
        b.PushByte(0);
        b.Push(new Span<byte>(Encoding.ASCII.GetBytes("octet")));
        b.PushByte(0);
        b.Push(new Span<byte>(Encoding.ASCII.GetBytes("out/build/test.hdd")));
        ref var opcode = ref b.Push<ushort>();
        opcode = SwapBytes((ushort)TftpOpcodeEnum.ReadRequest);
        return b;
    }
    Buf TftpAck(ushort blockIdx)
    {
        var buf = new Buf();

        ref var block = ref buf.Push<ushort>();
        block = SwapBytes(blockIdx);
        ref var opco = ref buf.Push<ushort>();
        opco = SwapBytes((ushort)TftpOpcodeEnum.Ack);
        return buf;
    }


    void UdpSend(Buf b, ushort ourPort, IpV4 ip, ushort port)
    {
        ref var udpHdr = ref b.Push<UdpHeader>();
        udpHdr.DestPort = port;
        udpHdr.SrcPort = ourPort;
        udpHdr.Checksum = 0;
        udpHdr.Length = (ushort)(4096 - b._curr);
        IpSend(b, ip, IpV4Header.ProtocolEnum.Udp);
    }
    void IpSend(Buf b, IpV4 ip, IpV4Header.ProtocolEnum protocol)
    {
        ushort id = 0; // TODO: have this be a PID
        ref var ipHdr = ref b.Push<IpV4Header>();
        ipHdr.Version = 4;
        ipHdr.Ihl = (byte)(Unsafe.SizeOf<IpV4Header>() / 4);
        ipHdr.Tos = 0;
        ipHdr.Length = (ushort)(4096 - b._curr);
        ipHdr.Id = id;
        ipHdr._fragOffset = 0;
        ipHdr.Ttl = 64;
        ipHdr.Protocol = protocol;
        ipHdr.Src = OurIp;
        ipHdr.Dest = ip;
        ipHdr.Check = 0;
        ipHdr.Check = IpChecksum(b.Get().Slice(0, Unsafe.SizeOf<IpV4Header>()));
        EthernetSend(b, ArpLookup(ip), EthernetHeader.EtherTypeEnum.IpV4);
    }
    void EthernetSend(Buf b, Mac mac, EthernetHeader.EtherTypeEnum type)
    {
        ref var ethHdr = ref b.Push<EthernetHeader>();
        ethHdr.Dest = mac;
        ethHdr.Src = _devConfig.MacAddr.Value;
        ethHdr.EtherType = type;
        Send(b);
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

        ArpDictionary[ipHdr.Dest] = senderHw;

        // TODO: checksum and TTL check
        // TODO: fragmentation
        var subB = b.Slice((ushort)(ipHdr.Length - (ipHdr.Ihl * 4)));
        if (ipHdr.Protocol == IpV4Header.ProtocolEnum.Icmp)
        {
            IcmpV4Handle(subB, ipHdr.Id, ipHdr.Src, senderHw);
        }
        else if (ipHdr.Protocol == IpV4Header.ProtocolEnum.Udp)
        {
            UdpHandle(subB, ipHdr.Id, ipHdr.Src, senderHw);
        }
    }

    internal enum TftpOpcodeEnum : byte
    {
        ReadRequest = 1,
        WriteRequest = 2,
        Data = 3,
        Ack = 4,
        Error = 5,
    }

    void UdpHandle(Buf b, ushort id, IpV4 sender, Mac senderHw)
    {
        ref var udpHdr = ref b.Pop<UdpHeader>();
        UdpReceiveResult r;
        r.Buffer = b;
        r.RemoteEndPoint.Address = sender;
        r.RemoteEndPoint.Port = udpHdr.SrcPort;
        if (Tcs != null) Tcs.SetResult(r);
        Tcs = null;
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

        IpSend(b, sender, IpV4Header.ProtocolEnum.Icmp);
    }

    static Dictionary<IpV4, Mac> ArpDictionary = new();
    static Dictionary<IpV4, AutoResetEvent> ArpSync = new();
    Mac ArpLookup(IpV4 ip)
    {
        if (ArpDictionary.TryGetValue(ip, out var mac))
        {
            return mac;
        }

        var sync = new AutoResetEvent(false);

        var b = new Buf();
        ref var arpData = ref b.Push<ArpEthIpV4>();
        arpData.SenderHwAddr = _devConfig.MacAddr.Value;
        arpData.SenderProtAddr = OurIp;
        arpData.TargetHwAddr = Mac.Broadcast;
        arpData.TargetProtAddr = ip;
        ref var arpHdr = ref b.Push<ArpHeader>();
        arpHdr.HwAddrFormat = ArpHeader.HwAddrFormatEnum.Ether;
        arpHdr.ProtAddrFormat = EthernetHeader.EtherTypeEnum.IpV4;
        arpHdr.HwAddrLength = 6;
        arpHdr.ProtAddrLength = 4;
        arpHdr.Op = ArpHeader.OpEnum.Request;
        EthernetSend(b, Mac.Broadcast, EthernetHeader.EtherTypeEnum.Arp);

        ArpSync[ip] = sync;
        sync.WaitOne();

        if (ArpDictionary.TryGetValue(ip, out var mac2))
        {
            return mac2;
        }
        throw new SystemException();
    }
    void ArpHandle(Buf b)
    {
        ref var arpHdr = ref b.Pop<ArpHeader>();
        Debug.Assert(arpHdr.HwAddrFormat == ArpHeader.HwAddrFormatEnum.Ether);
        Debug.Assert(arpHdr.ProtAddrFormat == EthernetHeader.EtherTypeEnum.IpV4);
        Debug.Assert(arpHdr.HwAddrLength == 6);
        Debug.Assert(arpHdr.ProtAddrLength == 4);

        ref var arpData = ref b.Pop<ArpEthIpV4>();

        var mac = arpData.SenderHwAddr;
        var ip = arpData.SenderProtAddr;
        // Debug.Print($"MAC {mac.Data[0]:X2}:{mac.Data[1]:X2}:{mac.Data[2]:X2}:{mac.Data[3]:X2}:{mac.Data[4]:X2}:{mac.Data[5]:X2} is IP {ip.Data[0]}.{ip.Data[1]}.{ip.Data[2]}.{ip.Data[3]}");
        ArpDictionary[ip] = mac;
        if (arpHdr.Op == ArpHeader.OpEnum.Reply) { var sync = ArpSync[ip]; ArpSync.Remove(ip); sync.Set(); }
        else if (arpHdr.Op == ArpHeader.OpEnum.Request) ArpReply(arpData.SenderHwAddr, arpData.SenderProtAddr);
    }


    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct UdpHeader
    {
        internal ushort _srcPort;
        internal ushort _destPort;
        internal ushort _length;
        internal ushort _checksum;
        internal ushort SrcPort { get => SwapBytes(_srcPort); set => _srcPort = SwapBytes(value); }
        internal ushort DestPort { get => SwapBytes(_destPort); set => _destPort = SwapBytes(value); }
        internal ushort Length { get => SwapBytes(_length); set => _length = SwapBytes(value); }
        internal ushort Checksum { get => SwapBytes(_checksum); set => _checksum = SwapBytes(value); }
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

        internal void PushByte(byte toPush)
        {
            _curr--;
            _data.Span[_curr] = toPush;
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

        internal Buf Slice(int len)
        {
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

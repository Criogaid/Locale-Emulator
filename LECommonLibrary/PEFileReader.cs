using System.IO;

namespace LECommonLibrary
{
    public enum PEType
    {
        X32,
        X64,
        Unknown
    }

    public static class PEFileReader
    {
        public static PEType GetPEType(string path)
        {
            if (string.IsNullOrEmpty(path))
                return PEType.Unknown;

            try
            {
                using (var br = new BinaryReader(new FileStream(path,
                                                    FileMode.Open,
                                                    FileAccess.Read,
                                                    FileShare.ReadWrite)))
                {
                    if (br.BaseStream.Length < 0x3C + 4 || br.ReadUInt16() != 0x5A4D)
                        return PEType.Unknown;

                    br.BaseStream.Seek(0x3C, SeekOrigin.Begin);
                    var ntOffset = (long)br.ReadUInt32();
                    if (ntOffset > br.BaseStream.Length - sizeof(uint) - sizeof(ushort))
                        return PEType.Unknown;

                    br.BaseStream.Seek(ntOffset, SeekOrigin.Begin);
                    if (br.ReadUInt32() != 0x00004550)
                        return PEType.Unknown;

                    var machine = br.ReadUInt16();

                    if (machine == 0x014C)
                        return PEType.X32;
                    else if (machine == 0x8664)
                        return PEType.X64;

                    return PEType.Unknown;
                }
            }
            catch
            {
                return PEType.Unknown;
            }
        }
    }
}
using System;
using System.IO;
using System.Text;

namespace LEProc
{
    internal sealed class RuntimeEnvironment
    {
        private const uint HeaderSize = 56;
        private const uint Version = 2;
        private const uint MaxSize = 1024 * 1024;
        private const uint MaxStringBytes = 32768;

        internal uint AnsiCodePage { get; set; }
        internal uint OemCodePage { get; set; }
        internal uint LocaleID { get; set; }
        internal uint FontCharset { get; set; }
        internal uint RegistryMode { get; set; }
        internal uint HookUILanguageMode { get; set; }
        internal string TimezoneId { get; set; }
        internal string StandardNameOverride { get; set; }
        internal string DaylightNameOverride { get; set; }

        internal byte[] Serialize()
        {
            using (var payload = new MemoryStream())
            {
                payload.SetLength(HeaderSize);
                payload.Position = HeaderSize;
                var timezone = AppendString(payload, TimezoneId, false, true);
                var standard = AppendString(payload, StandardNameOverride, true, false);
                var daylight = AppendString(payload, DaylightNameOverride, true, false);
                if (payload.Length > MaxSize)
                    throw new InvalidDataException("Runtime environment exceeds the ABI size limit.");

                payload.Position = 0;
                using (var writer = new BinaryWriter(payload, Encoding.Unicode, true))
                {
                    writer.Write((uint)payload.Length);
                    writer.Write(Version);
                    writer.Write(AnsiCodePage);
                    writer.Write(OemCodePage);
                    writer.Write(LocaleID);
                    writer.Write(FontCharset);
                    writer.Write(RegistryMode);
                    writer.Write(HookUILanguageMode);
                    writer.Write(timezone.Offset);
                    writer.Write(timezone.Length);
                    writer.Write(standard.Offset);
                    writer.Write(standard.Length);
                    writer.Write(daylight.Offset);
                    writer.Write(daylight.Length);
                }
                return payload.ToArray();
            }
        }

        private static StringLocation AppendString(
            MemoryStream payload,
            string value,
            bool allowEmpty,
            bool timezone)
        {
            if (value == null)
                value = string.Empty;
            if (!allowEmpty && value.Length == 0)
                throw new ArgumentException("A time-zone identifier is required.");
            if (value.IndexOf('\0') >= 0)
                throw new ArgumentException("Runtime environment strings cannot contain embedded NUL characters.");
            if (timezone && (value.IndexOf('\\') >= 0 || value.IndexOf('/') >= 0))
                throw new ArgumentException("The time-zone identifier is not a valid Windows identifier.");

            var bytes = Encoding.Unicode.GetBytes(value);
            if (bytes.Length > MaxStringBytes)
                throw new ArgumentException("Runtime environment string is too long.");
            var offset = checked((uint)payload.Length);
            payload.Write(bytes, 0, bytes.Length);
            payload.WriteByte(0);
            payload.WriteByte(0);
            return new StringLocation(offset, checked((uint)bytes.Length));
        }

        private struct StringLocation
        {
            internal readonly uint Offset;
            internal readonly uint Length;

            internal StringLocation(uint offset, uint length)
            {
                Offset = offset;
                Length = length;
            }
        }
    }
}

using System;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32;

namespace LEProc
{
    /// <summary>
    /// Resolves a time zone's display names in the emulated locale from the system's own
    /// MUI resources (the registry's MUI_Std / MUI_Dlt indirect strings), so no name is hard-coded.
    /// </summary>
    internal static class TimeZoneNames
    {
        private static readonly char[] PathSeparators =
            { Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar };

        private const uint LoadLibraryAsDataFile = 0x00000002;
        private const string ZoneKey = @"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Time Zones\";

        /// <summary>
        /// Returns empty strings when the language resource is unavailable; the native side then
        /// keeps the registry's own names.
        /// </summary>
        internal static void Resolve(string timezoneId, CultureInfo culture, out string standard, out string daylight)
        {
            standard = string.Empty;
            daylight = string.Empty;
            if (string.IsNullOrEmpty(timezoneId) || timezoneId.IndexOfAny(PathSeparators) >= 0)
                return;

            try
            {
                using (var baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
                using (var key = baseKey.OpenSubKey(ZoneKey + timezoneId, false))
                {
                    if (key == null)
                        return;
                    standard = Load(key.GetValue("MUI_Std") as string, culture);
                    daylight = Load(key.GetValue("MUI_Dlt") as string, culture);
                }
            }
            catch (Exception e)
            {
                if (!(e is System.Security.SecurityException) && !(e is UnauthorizedAccessException)
                    && !(e is IOException))
                    throw;
            }
        }

        // Parses "@tzres.dll,-632" and loads string 632 from <system>\<language>\tzres.dll.mui,
        // trying the culture and then its parents (ja-JP, ja).
        private static string Load(string indirect, CultureInfo culture)
        {
            string file;
            uint id;
            if (!TryParse(indirect, out file, out id))
                return string.Empty;

            for (var current = culture; current != null && !string.IsNullOrEmpty(current.Name); current = current.Parent)
            {
                var path = Path.Combine(Environment.SystemDirectory, current.Name, file + ".mui");
                if (!File.Exists(path))
                    continue;
                var module = LoadLibraryEx(path, IntPtr.Zero, LoadLibraryAsDataFile);
                if (module == IntPtr.Zero)
                    continue;
                try
                {
                    var buffer = new StringBuilder(256);
                    if (LoadString(module, id, buffer, buffer.Capacity) > 0)
                        return buffer.ToString();
                }
                finally
                {
                    FreeLibrary(module);
                }
            }
            return string.Empty;
        }

        private static bool TryParse(string indirect, out string file, out uint id)
        {
            file = null;
            id = 0;
            if (string.IsNullOrEmpty(indirect) || indirect[0] != '@')
                return false;
            var comma = indirect.LastIndexOf(',');
            if (comma <= 1 || comma + 1 >= indirect.Length)
                return false;
            int parsed;
            if (!int.TryParse(indirect.Substring(comma + 1), NumberStyles.AllowLeadingSign,
                    CultureInfo.InvariantCulture, out parsed) || parsed == int.MinValue)
                return false;
            file = indirect.Substring(1, comma - 1);
            if (file.IndexOfAny(PathSeparators) >= 0)
                return false;
            id = (uint)Math.Abs(parsed);
            return true;
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr LoadLibraryEx(string fileName, IntPtr file, uint flags);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool FreeLibrary(IntPtr module);

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        private static extern int LoadString(IntPtr module, uint id, StringBuilder buffer, int maxChars);
    }
}

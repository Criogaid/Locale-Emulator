using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using Amemiya.Extensions;
using Microsoft.Win32;

namespace LEProc
{
    internal class LoaderWrapper
    {
        private const uint CREATE_NORMAL = 0x00000000;
        private const uint CREATE_SUSPENDED = 0x00000004;
        private LEB _leb;
        private LERegistryRedirector _registry = new LERegistryRedirector(0);

        internal LoaderWrapper()
            : this(null, null, null)
        {
        }

        internal LoaderWrapper(string applicationName)
            : this(applicationName, null, null)
        {
        }

        internal LoaderWrapper(string applicationName, string commandLine, string currentDirectory)
        {
            ApplicationName = applicationName;
            CommandLine = commandLine;
            CurrentDirectory = currentDirectory;

            _leb = new LEB
                   {
                       AnsiCodePage = 932,
                       OemCodePage = 932,
                       LocaleID = 0x411,
                       DefaultCharset = 128,
                       HookUILanguageAPI = 0,
                       // As we have abandoned the "default font" parameter,
                       // we decide to put here some empty bytes.
                       DefaultFaceName = new byte[64]
                   };
            Timezone = "Tokyo Standard Time";
        }

        /// <summary>
        ///     Application that will be run.
        /// </summary>
        internal string ApplicationName { get; set; }

        /// <summary>
        ///     Command arguments.
        /// </summary>
        internal string CommandLine { get; set; }

        /// <summary>
        ///     Working directory.
        /// </summary>
        internal string CurrentDirectory { get; set; }

        /// <summary>
        ///     Whether the process should be created with CREATE_SUSPENDED. Useful when debugging with OllyDbg.
        /// </summary>
        internal bool DebugMode { get; set; }

        /// <summary>
        ///     New AnsiCodePage. Default value is 932.
        /// </summary>
        internal uint AnsiCodePage
        {
            get { return _leb.AnsiCodePage; }
            set { _leb.AnsiCodePage = value; }
        }

        /// <summary>
        ///     New OemCodePage. Default value is 932.
        /// </summary>
        internal uint OemCodePage
        {
            get { return _leb.OemCodePage; }
            set { _leb.OemCodePage = value; }
        }

        /// <summary>
        ///     New LocaleID. Default value is 0x411(1041).
        /// </summary>
        internal uint LocaleID
        {
            get { return _leb.LocaleID; }
            set { _leb.LocaleID = value; }
        }

        /// <summary>
        ///     New DefaultCharset. Default value is 128(Shift-JIS).
        /// </summary>
        internal uint DefaultCharset
        {
            get { return _leb.DefaultCharset; }
            set { _leb.DefaultCharset = value; }
        }

        /// <summary>
        ///     Should we hook UI-related API? Default value is 0.
        /// </summary>
        internal uint HookUILanguageAPI
        {
            get { return _leb.HookUILanguageAPI; }
            set { _leb.HookUILanguageAPI = value; }
        }

        /// <summary>
        ///     String that represents a Timezone.
        ///     This can be found in HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Time Zones.
        /// </summary>
        internal string Timezone
        {
            get { return _leb.Timezone.GetStandardName(); }
            set
            {
                var tzi = TimeZoneInfo.FindSystemTimeZoneById(value);
                _leb.Timezone.SetStandardName(tzi.StandardName);
                _leb.Timezone.SetDaylightName(tzi.DaylightName);

                var tzi2 = ReadTZIFromRegistry(value);
                _leb.Timezone.Bias = tzi2.Bias;
                _leb.Timezone.StandardBias = tzi2.StandardBias;
                // Persisted profiles have always used standard time without DST transitions.
                _leb.Timezone.DaylightBias = 0;
            }
        }

        /// <summary>
        ///     Get or set the number of registry redirection entries.
        /// </summary>
        internal int NumberOfRegistryRedirectionEntries
        {
            get { return _registry.NumberOfRegistryRedirectionEntries; }
            set { _registry = new LERegistryRedirector(value); }
        }

        internal bool AddRegistryRedirectEntry(
            string root,
            string subkey,
            string valueName,
            string dataType,
            string data)
        {
            return _registry.AddRegistryEntry(root,
                                              subkey,
                                              valueName,
                                              dataType,
                                              data);
        }

        /// <summary>
        ///     Create process.
        /// </summary>
        /// <returns>Error number</returns>
        internal uint Start()
        {
            if (string.IsNullOrEmpty(ApplicationName))
                throw new Exception("ApplicationName cannot null.");

            var newLEB = ArrayExtensions.StructToBytes(_leb);
            newLEB = newLEB.CombineWith(_registry.GetBinaryData());

            var locLEB = Marshal.AllocHGlobal(newLEB.Length);
            var processInformation = new PROCESS_INFORMATION();
            try
            {
                Marshal.Copy(newLEB, 0, locLEB, newLEB.Length);
                AttachConsole(ATTACH_PARENT_PROCESS);
                var startupInfo = new STARTUPINFO { cb = Marshal.SizeOf(typeof(STARTUPINFO)) };
                var commandLine = CommandLine == null ? null : new StringBuilder(CommandLine);
                var ret = LeCreateProcess(locLEB, ApplicationName, commandLine, CurrentDirectory,
                    DebugMode ? CREATE_SUSPENDED : CREATE_NORMAL, ref startupInfo, out processInformation,
                    IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
                if (ret == 0)
                {
                    var waitResult = WaitForSingleObject(processInformation.hProcess, INFINITE);
                    if (waitResult == INFINITE)
                        throw new Win32Exception(Marshal.GetLastWin32Error());
                    if (waitResult != 0)
                        throw new InvalidOperationException("等待进程返回了意外状态：" + waitResult);
                }
                return ret;
            }
            finally
            {
                if (processInformation.hThread != IntPtr.Zero)
                    CloseHandle(processInformation.hThread);
                if (processInformation.hProcess != IntPtr.Zero)
                    CloseHandle(processInformation.hProcess);
                Marshal.FreeHGlobal(locLEB);
            }
        }

        private static byte[] EncodeTimezoneName(string name)
        {
            // WCHAR[32] includes the terminator; registry IDs have no such limit.
            const int maxNameLength = 31;
            var length = Math.Min(name.Length, maxNameLength);
            if (length > 0 && char.IsHighSurrogate(name[length - 1]))
                length--;
            var result = new byte[64];
            Encoding.Unicode.GetBytes(name, 0, length, result, 0);
            return result;
        }

        public static T BytesToStruct<T>(byte[] bytes)
        {
            var size = Marshal.SizeOf(typeof (T));
            var buffer = Marshal.AllocHGlobal(size);
            try
            {
                Marshal.Copy(bytes, 0, buffer, size);
                return (T)Marshal.PtrToStructure(buffer, typeof (T));
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }

        private _REG_TZI_FORMAT ReadTZIFromRegistry(string id)
        {
            var tzi =
                (byte[])
                Registry.GetValue(
                                  $"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\{id}",
                                  "TZI",
                                  null);

            return BytesToStruct<_REG_TZI_FORMAT>(tzi);
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct PROCESS_INFORMATION
        {
            public IntPtr hProcess;
            public IntPtr hThread;
            public uint dwProcessId;
            public uint dwThreadId;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct STARTUPINFO
        {
            public int cb;
            public String lpReserved;
            public String lpDesktop;
            public String lpTitle;
            public uint dwX;
            public uint dwY;
            public uint dwXSize;
            public uint dwYSize;
            public uint dwXCountChars;
            public uint dwYCountChars;
            public uint dwFillAttribute;
            public uint dwFlags;
            public short wShowWindow;
            public short cbReserved2;
            public IntPtr lpReserved2;
            public IntPtr hStdInput;
            public IntPtr hStdOutput;
            public IntPtr hStdError;
        }

        [DllImport("LoaderDll.dll", CharSet = CharSet.Unicode)]
        public static extern uint LeCreateProcess(IntPtr leb,
                                                  [MarshalAs(UnmanagedType.LPWStr), In] string applicationName,
                                                  [In, Out] StringBuilder commandLine,
                                                  [MarshalAs(UnmanagedType.LPWStr), In] string currentDirectory,
                                                  uint creationFlags,
                                                  ref STARTUPINFO startupInfo,
                                                  out PROCESS_INFORMATION processInformation,
                                                  IntPtr processAttributes,
                                                  IntPtr threadAttributes,
                                                  IntPtr environment,
                                                  IntPtr token);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern UInt32 WaitForSingleObject(IntPtr hHandle, UInt32 dwMilliseconds);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool CloseHandle(IntPtr hHandle);

        const uint INFINITE = 0xFFFFFFFF;

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool AttachConsole(uint dwProcessId);

        const uint ATTACH_PARENT_PROCESS = 0x0ffffffff;

        [StructLayout(LayoutKind.Sequential)]
        internal struct LEB
        {
            internal uint AnsiCodePage;
            internal uint OemCodePage;
            internal uint LocaleID;
            internal uint DefaultCharset;
            internal uint HookUILanguageAPI;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)] internal byte[] DefaultFaceName;
            internal RTL_TIME_ZONE_INFORMATION Timezone;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct RTL_TIME_ZONE_INFORMATION
        {
            internal int Bias;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)] internal byte[] StandardName;
            internal TIME_FIELDS StandardDate;
            internal int StandardBias;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)] internal byte[] DaylightName;
            internal TIME_FIELDS DaylightDate;
            internal int DaylightBias;

            public override string ToString()
            {
                return
                    $"StandardName={Encoding.Unicode.GetString(StandardName)}";
            }

            public string GetStandardName()
            {
                return Encoding.Unicode.GetString(StandardName).Replace("\x00", "");
            }

            public void SetStandardName(string name)
            {
                StandardName = EncodeTimezoneName(name);
            }

            public string GetDaylightName()
            {
                return Encoding.Unicode.GetString(DaylightName).Replace("\x00", "");
            }

            public void SetDaylightName(string name)
            {
                DaylightName = EncodeTimezoneName(name);
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct _REG_TZI_FORMAT
        {
            internal int Bias;
            internal int StandardBias;
            internal int DaylightBias;
            internal _SYSTEMTIME StandardDate;
            internal _SYSTEMTIME DaylightDate;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct _SYSTEMTIME
        {
            internal ushort wYear;
            internal ushort wMonth;
            internal ushort wDayOfWeek;
            internal ushort wDay;
            internal ushort wHour;
            internal ushort wMinute;
            internal ushort wSecond;
            internal ushort wMilliseconds;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct TIME_FIELDS
        {
            internal ushort Year; // range [1601...]
            internal ushort Month; // range [1..12]
            internal ushort Day; // range [1..31]
            internal ushort Hour; // range [0..23]
            internal ushort Minute; // range [0..59]
            internal ushort Second; // range [0..59]
            internal ushort Milliseconds; // range [0..999]
            internal ushort Weekday; // range [0..6] == [Sunday..Saturday]
        }
    }
}

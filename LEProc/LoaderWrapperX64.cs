using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using LECommonLibrary;

namespace LEProc
{
    internal sealed class LoaderWrapper
    {
        private const uint CreateNormal = 0;
        private const uint CreateSuspended = 0x00000004;
        private const uint Infinite = 0xFFFFFFFF;
        private const uint AttachParentProcess = 0x0FFFFFFFF;
        private const uint LepEnvironmentVersion = 4;

        private LEPB _environment;
        private bool _advancedRegistryRedirection;

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
            _environment = new LEPB
            {
                Size = (uint)Marshal.SizeOf(typeof(LEPB)),
                Version = LepEnvironmentVersion,
                AnsiCodePage = 932,
                OemCodePage = 932,
                LocaleID = 0x411,
                FontCharset = 128,
                RegistryRedirectionMode = 0,
                HookUILanguageMode = 0,
                TimezoneId = "Tokyo Standard Time",
                StandardNameOverride = string.Empty,
                DaylightNameOverride = string.Empty
            };
        }

        internal string ApplicationName { get; set; }
        internal string CommandLine { get; set; }
        internal string CurrentDirectory { get; set; }
        internal bool DebugMode { get; set; }

        internal uint AnsiCodePage
        {
            get { return _environment.AnsiCodePage; }
            set { _environment.AnsiCodePage = value; }
        }

        internal uint OemCodePage
        {
            get { return _environment.OemCodePage; }
            set { _environment.OemCodePage = value; }
        }

        internal uint LocaleID
        {
            get { return _environment.LocaleID; }
            set { _environment.LocaleID = value; }
        }

        internal uint DefaultCharset
        {
            get { return _environment.FontCharset; }
            set { _environment.FontCharset = value; }
        }

        internal uint HookUILanguageAPI
        {
            get { return _environment.HookUILanguageMode; }
            set
            {
                _advancedRegistryRedirection = value != 0;
                _environment.HookUILanguageMode = _advancedRegistryRedirection ? 2u : 0u;
                if (_environment.RegistryRedirectionMode != 0)
                    _environment.RegistryRedirectionMode = _advancedRegistryRedirection ? 2u : 1u;
            }
        }

        internal string Timezone
        {
            get { return _environment.TimezoneId; }
            set { _environment.TimezoneId = value ?? string.Empty; }
        }

        internal int NumberOfRegistryRedirectionEntries
        {
            get { return _environment.RegistryRedirectionMode == 0 ? 0 : 1; }
            set
            {
                _environment.RegistryRedirectionMode = value <= 0
                    ? 0u
                    : (_advancedRegistryRedirection ? 2u : 1u);
            }
        }

        internal bool AddRegistryRedirectEntry(
            string root,
            string subkey,
            string valueName,
            string dataType,
            string data)
        {
            // x64 core derives these entries from the selected redirection mode.
            return true;
        }

        internal uint Start()
        {
            if (string.IsNullOrEmpty(ApplicationName))
                throw new ArgumentException(Localization.GetString("ApplicationNameMissing"));

            IntPtr environment = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(LEPB)));
            PROCESS_INFORMATION processInformation = new PROCESS_INFORMATION();
            try
            {
                Marshal.StructureToPtr(_environment, environment, false);
                AttachConsole(AttachParentProcess);
                var startupInfo = new STARTUPINFO { cb = Marshal.SizeOf(typeof(STARTUPINFO)) };
                var commandLine = CommandLine == null ? null : new StringBuilder(CommandLine);
                var result = LepCreateProcess(
                    environment,
                    ApplicationName,
                    commandLine,
                    CurrentDirectory,
                    DebugMode ? CreateSuspended : CreateNormal,
                    ref startupInfo,
                    out processInformation,
                    IntPtr.Zero,
                    IntPtr.Zero,
                    IntPtr.Zero,
                    IntPtr.Zero);

                if (result == 0)
                {
                    var waitResult = WaitForSingleObject(processInformation.hProcess, Infinite);
                    if (waitResult == Infinite)
                        throw new Win32Exception(Marshal.GetLastWin32Error());
                    if (waitResult != 0)
                        throw new InvalidOperationException(
                            string.Format(Localization.GetString("ProcessWaitError"), waitResult));
                }

                return result;
            }
            finally
            {
                if (processInformation.hThread != IntPtr.Zero)
                    CloseHandle(processInformation.hThread);
                if (processInformation.hProcess != IntPtr.Zero)
                    CloseHandle(processInformation.hProcess);
                Marshal.FreeHGlobal(environment);
            }
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct LEPB
        {
            internal uint Size;
            internal uint Version;
            internal uint AnsiCodePage;
            internal uint OemCodePage;
            internal uint LocaleID;
            internal uint FontCharset;
            internal uint RegistryRedirectionMode;
            internal uint HookUILanguageMode;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] internal string TimezoneId;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] internal string StandardNameOverride;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] internal string DaylightNameOverride;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct PROCESS_INFORMATION
        {
            internal IntPtr hProcess;
            internal IntPtr hThread;
            internal uint dwProcessId;
            internal uint dwThreadId;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct STARTUPINFO
        {
            internal int cb;
            internal string lpReserved;
            internal string lpDesktop;
            internal string lpTitle;
            internal uint dwX;
            internal uint dwY;
            internal uint dwXSize;
            internal uint dwYSize;
            internal uint dwXCountChars;
            internal uint dwYCountChars;
            internal uint dwFillAttribute;
            internal uint dwFlags;
            internal short wShowWindow;
            internal short cbReserved2;
            internal IntPtr lpReserved2;
            internal IntPtr hStdInput;
            internal IntPtr hStdOutput;
            internal IntPtr hStdError;
        }

        [DllImport("LoaderDll.dll", CharSet = CharSet.Unicode)]
        private static extern uint LepCreateProcess(
            IntPtr environment,
            [MarshalAs(UnmanagedType.LPWStr), In] string applicationName,
            [In, Out] StringBuilder commandLine,
            [MarshalAs(UnmanagedType.LPWStr), In] string currentDirectory,
            uint creationFlags,
            ref STARTUPINFO startupInfo,
            out PROCESS_INFORMATION processInformation,
            IntPtr processAttributes,
            IntPtr threadAttributes,
            IntPtr environmentBlock,
            IntPtr token);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr handle);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool AttachConsole(uint processId);
    }
}

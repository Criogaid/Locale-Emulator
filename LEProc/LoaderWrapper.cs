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
        private const uint WaitFailed = 0xFFFFFFFF;
        private const uint Infinite = 0xFFFFFFFF;
        private const uint AttachParentProcess = 0x0FFFFFFFF;

        private readonly RuntimeEnvironment _environment;

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
            _environment = new RuntimeEnvironment
            {
                AnsiCodePage = 932,
                OemCodePage = 932,
                LocaleID = 0x411,
                FontCharset = 128,
                RegistryMode = 0,
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

        internal uint RegistryMode
        {
            get { return _environment.RegistryMode; }
            set { _environment.RegistryMode = value; }
        }

        internal uint HookUILanguageMode
        {
            get { return _environment.HookUILanguageMode; }
            set { _environment.HookUILanguageMode = value; }
        }

        internal string StandardName
        {
            get { return _environment.StandardNameOverride; }
            set { _environment.StandardNameOverride = value; }
        }

        internal string DaylightName
        {
            get { return _environment.DaylightNameOverride; }
            set { _environment.DaylightNameOverride = value; }
        }

        internal string Timezone
        {
            get { return _environment.TimezoneId; }
            set { _environment.TimezoneId = value; }
        }

        internal uint Start()
        {
            if (string.IsNullOrEmpty(ApplicationName))
                throw new ArgumentException(Localization.GetString("ApplicationNameMissing"));

            var serializedEnvironment = _environment.Serialize();
            var environment = Marshal.AllocHGlobal(serializedEnvironment.Length);
            var processInformation = new NativeProcessInformation();
            try
            {
                Marshal.Copy(serializedEnvironment, 0, environment, serializedEnvironment.Length);
                AttachConsole(AttachParentProcess);
                var startupInfo = new NativeStartupInfo { Size = Marshal.SizeOf(typeof(NativeStartupInfo)) };
                var commandLine = CommandLine == null ? null : new StringBuilder(CommandLine);
                var result = NativeLoaderAdapter.Start(
                    environment,
                    ApplicationName,
                    commandLine,
                    CurrentDirectory,
                    DebugMode ? CreateSuspended : CreateNormal,
                    ref startupInfo,
                    out processInformation);

                if (result == 0)
                {
                    var waitResult = WaitForSingleObject(processInformation.Process, Infinite);
                    if (waitResult == WaitFailed)
                        throw new Win32Exception(Marshal.GetLastWin32Error());
                    if (waitResult != 0)
                        throw new InvalidOperationException(
                            string.Format(Localization.GetString("ProcessWaitError"), waitResult));
                }
                return result;
            }
            finally
            {
                if (processInformation.Thread != IntPtr.Zero)
                    CloseHandle(processInformation.Thread);
                if (processInformation.Process != IntPtr.Zero)
                    CloseHandle(processInformation.Process);
                Marshal.FreeHGlobal(environment);
            }
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr handle);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool AttachConsole(uint processId);
    }
}

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

        internal LoaderWrapper(RuntimeEnvironment environment)
        {
            if (environment == null)
                throw new ArgumentNullException("environment");
            Environment = environment;
        }

        // Locale, code pages and time zone come from the caller's profile; none is assumed here.
        internal RuntimeEnvironment Environment { get; private set; }
        internal string ApplicationName { get; set; }
        internal string CommandLine { get; set; }
        internal string CurrentDirectory { get; set; }
        internal bool DebugMode { get; set; }

        internal uint Start()
        {
            if (string.IsNullOrEmpty(ApplicationName))
                throw new ArgumentException(Localization.GetString("ApplicationNameMissing"));

            var serializedEnvironment = Environment.Serialize();
            var environment = Marshal.AllocHGlobal(serializedEnvironment.Length);
            var processInformation = new NativeProcessInformation();
            try
            {
                Marshal.Copy(serializedEnvironment, 0, environment, serializedEnvironment.Length);
                AttachConsole(AttachParentProcess);
                var startupInfo = new NativeStartupInfo { Size = Marshal.SizeOf(typeof(NativeStartupInfo)) };
                var commandLine = CommandLine == null ? null : new StringBuilder(CommandLine);
                var result = CreateProcess(
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

        // Brings a suspended child that another process created, and that this launcher's architecture
        // matches, under an already serialized runtime environment. Returns the NTSTATUS of the attach.
        internal static uint Attach(uint processId, uint threadId, byte[] serializedEnvironment)
        {
            const uint ProcessAllAccess = 0x001FFFFF;
            const uint ProcessInjectionAccess = 0x00000008 | 0x00000010 | 0x00000020 | 0x00000400; // VM op/read/write, query
            const uint StatusInvalidParameter = 0xC000000D;
            const uint StatusAccessDenied = 0xC0000022;

            if (serializedEnvironment == null || serializedEnvironment.Length == 0)
                return StatusInvalidParameter;

            var process = OpenProcess(ProcessAllAccess, false, processId);
            if (process == IntPtr.Zero)
                process = OpenProcess(ProcessInjectionAccess, false, processId);
            if (process == IntPtr.Zero)
                return StatusAccessDenied;

            var environment = Marshal.AllocHGlobal(serializedEnvironment.Length);
            try
            {
                Marshal.Copy(serializedEnvironment, 0, environment, serializedEnvironment.Length);
                var clientId = new NativeClientId
                {
                    Process = new IntPtr(processId),
                    Thread = new IntPtr(threadId)
                };
                return AttachProcess(environment, process, ref clientId);
            }
            finally
            {
                Marshal.FreeHGlobal(environment);
                CloseHandle(process);
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct NativeClientId
        {
            internal IntPtr Process;
            internal IntPtr Thread;
        }

        [DllImport("LoaderDll.dll", EntryPoint = "LeAttachProcess")]
        private static extern uint AttachProcess(IntPtr environment, IntPtr process, ref NativeClientId clientId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr OpenProcess(uint desiredAccess, bool inheritHandle, uint processId);

        [DllImport("LoaderDll.dll", EntryPoint = "LeCreateProcess", CharSet = CharSet.Unicode)]
        private static extern uint CreateProcess(
            IntPtr environment,
            [MarshalAs(UnmanagedType.LPWStr)] string applicationName,
            [In, Out] StringBuilder commandLine,
            [MarshalAs(UnmanagedType.LPWStr)] string currentDirectory,
            uint creationFlags,
            ref NativeStartupInfo startupInfo,
            out NativeProcessInformation processInformation,
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

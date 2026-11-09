using System;
using System.Runtime.InteropServices;
using System.Text;

namespace LEProc
{
    internal static class NativeLoaderAdapter
    {
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

        internal static uint Start(
            IntPtr environment,
            string applicationName,
            StringBuilder commandLine,
            string currentDirectory,
            uint creationFlags,
            ref NativeStartupInfo startupInfo,
            out NativeProcessInformation processInformation)
        {
            return CreateProcess(environment, applicationName, commandLine, currentDirectory, creationFlags,
                ref startupInfo, out processInformation, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
        }
    }
}

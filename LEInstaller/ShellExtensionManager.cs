using System;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace LEInstaller
{
    class ShellExtensionManager
    {
        private static string clsid = "{C52B9871-E5E9-41FD-B84D-C5ACADBEC7AE}";
        private static string fileType = "*";
        private static string friendlyName = "LocaleEmulator.LEContextMenuHandler Class";
        private static string keyName = $@"Software\Classes\{fileType}\shellex\ContextMenuHandlers\{clsid}";
        private static string staticVerbKeyName = @"Software\Classes\exefile\shell\LocaleEmulator.Run";

        // The x64 package ships the Windows 11 first-level menu (sparse package). Appx registration is
        // only exposed through the Windows PowerShell Appx module, so the packaged script performs it.
        public static bool IsModernMenuAvailable(string productDirectory)
        {
            return File.Exists(Path.Combine(productDirectory, "ModernMenu.ps1"))
                && File.Exists(Path.Combine(productDirectory, "AppxManifest.xml"))
                && File.Exists(Path.Combine(productDirectory, "LEGUI.exe"))
                && File.Exists(Path.Combine(productDirectory, "LEExplorerCommand.dll"));
        }

        public static void ChangeModernMenu(string productDirectory, bool register)
        {
            // A 32-bit installer on 64-bit Windows must reach the native PowerShell, not the WOW64 one.
            var systemDirectory = Environment.Is64BitOperatingSystem && !Environment.Is64BitProcess
                ? Path.Combine(Environment.GetEnvironmentVariable("windir"), "Sysnative")
                : Environment.SystemDirectory;
            var powershell = Path.Combine(systemDirectory, @"WindowsPowerShell\v1.0\powershell.exe");
            var script = Path.Combine(productDirectory, "ModernMenu.ps1");
            var startInfo = new ProcessStartInfo(powershell,
                "-NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" + script + "\" -Action "
                + (register ? "Register" : "Unregister"))
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardError = true
            };
            using (var process = Process.Start(startInfo))
            {
                var error = process.StandardError.ReadToEndAsync();
                if (!process.WaitForExit(120000))
                {
                    process.Kill();
                    throw new TimeoutException(I18n.GetString("ModernMenuTimeout"));
                }
                if (process.ExitCode != 0)
                    throw new InvalidOperationException(error.Result.Trim());
            }
        }

        public static void RegisterShellExtContextMenuHandler(bool allUsers)
        {
            var rootName = allUsers ? Registry.LocalMachine : Registry.CurrentUser;
            using (var key = rootName.CreateSubKey(keyName))
            {
                if (key == null)
                    throw new InvalidOperationException(I18n.GetString("CannotCreateContextMenuKey"));
                key.SetValue(null, friendlyName);
            }
        }

        public static void UnregisterShellExtContextMenuHandler(bool allUsers)
        {
            var rootName = allUsers ? Registry.LocalMachine : Registry.CurrentUser;
            rootName.DeleteSubKeyTree(keyName, false);
        }

        public static bool IsInstalled(bool allUsers)
        {
            var rootName = allUsers ? Registry.LocalMachine : Registry.CurrentUser;
            using (var key = rootName.OpenSubKey(keyName, false))
            {
                return key != null;
            }
        }

        public static void RegisterStaticExplorerVerb(string launcherPath)
        {
            if (!File.Exists(launcherPath))
                throw new FileNotFoundException(I18n.GetString("MissingLauncher"), launcherPath);

            using (var key = Registry.CurrentUser.CreateSubKey(staticVerbKeyName))
            {
                if (key == null)
                    throw new InvalidOperationException(I18n.GetString("CannotCreateContextMenuKey"));

                key.SetValue(null, I18n.GetString("RunWithLocaleEmulator"), RegistryValueKind.String);
                key.SetValue("MUIVerb", I18n.GetString("RunWithLocaleEmulator"), RegistryValueKind.String);
                key.SetValue("Icon", launcherPath, RegistryValueKind.String);
                key.SetValue("NeverDefault", string.Empty, RegistryValueKind.String);
                key.SetValue("MultiSelectModel", "Single", RegistryValueKind.String);

                using (var command = key.CreateSubKey("command"))
                {
                    if (command == null)
                        throw new InvalidOperationException(I18n.GetString("CannotCreateContextMenuKey"));
                    command.SetValue(null, "\"" + launcherPath + "\" \"%1\"", RegistryValueKind.String);
                }
            }
        }

        public static void UnregisterStaticExplorerVerb()
        {
            Registry.CurrentUser.DeleteSubKeyTree(staticVerbKeyName, false);
        }

        public static bool IsStaticExplorerVerbInstalled()
        {
            using (var key = Registry.CurrentUser.OpenSubKey(staticVerbKeyName, false))
            {
                return key != null;
            }
        }
    }
}

using System;
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

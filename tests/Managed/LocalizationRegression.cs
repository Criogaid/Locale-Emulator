using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Threading;
using System.Windows.Forms;
using LECommonLibrary;

namespace ManagedRegression
{
    internal static class LocalizationRegression
    {
        internal static void RunLanguageProbe()
        {
            Thread.CurrentThread.CurrentUICulture = CultureInfo.GetCultureInfo("zh-CN");
            Check(Localization.GetString("CoreFilesMissing").Contains("程序目录"),
                "Common language resources did not select zh-CN.");

            var directory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            var installer = Assembly.LoadFrom(Path.Combine(directory, "LEInstaller.exe"));
            var formType = installer.GetType("LEInstaller.Form1", true);
            using (var form = (Form)Activator.CreateInstance(formType, new object[] { 0 }))
            {
                Check(form.Text != "Locale Emulator Installer" && form.Text != "Title",
                    "Installer did not apply the zh-CN resource dictionary.");
            }
        }

        internal static void InstallerAndCommonResourcesUseUiCulture()
        {
            var start = new ProcessStartInfo(Assembly.GetExecutingAssembly().Location, "--localization-probe")
            {
                UseShellExecute = false,
                WorkingDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location)
            };
            using (var child = Process.Start(start))
            {
                try
                {
                    Check(child.WaitForExit(15000), "Localization probe timed out.");
                    Check(child.ExitCode == 0, "Localization probe failed.");
                }
                finally
                {
                    if (!child.HasExited)
                        child.Kill();
                }
            }
        }

        private static void Check(bool condition, string message)
        {
            if (!condition)
                throw new InvalidOperationException(message);
        }
    }
}

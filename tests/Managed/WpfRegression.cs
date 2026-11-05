using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Windows;
using System.Windows.Controls;
using LECommonLibrary;
using LEGUI;

namespace ManagedRegression
{
    internal static class WpfRegression
    {
        private static App application;

        internal static void AppConfigSaveFailureDoesNotLaunchOrShutdown()
        {
            WithConfig(directory =>
            {
                var configPath = Path.Combine(directory, "folder.le.config", "target.exe.LE.CONFIG");
                Directory.CreateDirectory(Path.GetDirectoryName(configPath));
                typeof(App).GetField("StandaloneFilePath", BindingFlags.Static | BindingFlags.NonPublic)
                    .SetValue(null, configPath);
                LEConfig.SaveApplicationConfigFile(configPath, new LEProfile(true));
                var window = new AppConfig();
                try
                {
                    foreach (var buttonName in new[] { "bSaveAppSetting", "bShortcut" })
                    {
                        using (File.Open(configPath, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
                            ExpectIOException(() => Click(window, buttonName));
                        Check(!application.Dispatcher.HasShutdownStarted, "Failed save shut down the dispatcher.");
                    }

                    var parameter = (MaskedTextBox)window.FindName("tbAppParameter");
                    parameter.Text = "--retry \"two words\"";
                    Invoke(window, "SaveSetting");
                    Check(LEConfig.GetProfiles(configPath)[0].Parameter == parameter.Text,
                        "The application editor could not save corrected input after a failure.");
                }
                finally { window.Close(); }
            });
        }

        internal static void GlobalSaveFailureKeepsSelectedProfile()
        {
            WithConfig(directory =>
            {
                var original = new LEProfile(true);
                LEConfig.SaveGlobalConfigFile(original);
                var window = new GlobalConfig();
                try
                {
                    ((ComboBox)window.FindName("cbGlobalProfiles")).SelectedIndex = 0;
                    ((CheckBox)window.FindName("cbRedirectRegistry")).IsChecked = !original.RedirectRegistry;
                    using (File.Open(LEConfig.GlobalConfigPath, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
                        ExpectIOException(() => Click(window, "bSaveGlobalSetting"));
                    Check(Profiles(window).Count == 1 && Profiles(window)[0].Equals(original),
                        "Failed save changed the selected in-memory profile.");
                    Check(LEConfig.GetProfiles()[0].Equals(original), "Failed save changed the persisted profile.");
                    Click(window, "bSaveGlobalSetting");
                    Check(Profiles(window)[0].RedirectRegistry == !original.RedirectRegistry
                        && LEConfig.GetProfiles()[0].Equals(Profiles(window)[0]),
                        "Retry did not update both persisted and in-memory profiles.");
                }
                finally { window.Close(); }
            });
        }

        internal static void GlobalSaveAsFailureKeepsProfiles()
        {
            WithConfig(directory =>
            {
                var original = new LEProfile(true);
                LEConfig.SaveGlobalConfigFile(original);
                var window = new GlobalConfig();
                try
                {
                    ((ComboBox)window.FindName("cbGlobalProfiles")).SelectedIndex = 0;
                    var saveAs = (Action<string>)Delegate.CreateDelegate(typeof(Action<string>), window,
                        typeof(GlobalConfig).GetMethod("SaveProfileAs", BindingFlags.Instance | BindingFlags.NonPublic));
                    using (File.Open(LEConfig.GlobalConfigPath, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
                        ExpectIOException(() => saveAs("Added profile"));
                    Check(Profiles(window).Count == 1 && Profiles(window)[0].Equals(original),
                        "Failed Save As added an in-memory profile.");
                    Check(((ComboBox)window.FindName("cbGlobalProfiles")).Items.Count == 1,
                        "Failed Save As changed the visible profile list.");
                    saveAs("Added profile");
                    Check(Profiles(window).Count == 2 && LEConfig.GetProfiles().Length == 2,
                        "Save As could not be retried after a write failure.");
                }
                finally { window.Close(); }
            });
        }

        private static void WithConfig(Action<string> test)
        {
            if (application == null)
            {
                application = new App { ShutdownMode = ShutdownMode.OnExplicitShutdown };
                application.InitializeComponent();
            }
            var directory = Path.Combine(Path.GetTempPath(), "LE WPF regression " + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            var originalPath = LEConfig.GlobalConfigPath;
            try
            {
                LEConfig.GlobalConfigPath = Path.Combine(directory, "LEConfig.xml");
                test(directory);
            }
            finally
            {
                LEConfig.GlobalConfigPath = originalPath;
                Directory.Delete(directory, true);
            }
        }

        private static List<LEProfile> Profiles(GlobalConfig window)
        {
            return (List<LEProfile>)typeof(GlobalConfig).GetField("_profiles",
                BindingFlags.Instance | BindingFlags.NonPublic).GetValue(window);
        }

        private static void Click(Window window, string name)
        {
            ((Button)window.FindName(name)).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
        }

        private static void Invoke(object target, string methodName)
        {
            var method = target.GetType().GetMethod(methodName, BindingFlags.Instance | BindingFlags.NonPublic);
            ((Action)Delegate.CreateDelegate(typeof(Action), target, method))();
        }

        private static void ExpectIOException(Action action)
        {
            try { action(); }
            catch (IOException) { return; }
            throw new Exception("Expected the GUI save event to stop with IOException.");
        }

        private static void Check(bool condition, string message)
        {
            if (!condition) throw new Exception(message);
        }
    }
}

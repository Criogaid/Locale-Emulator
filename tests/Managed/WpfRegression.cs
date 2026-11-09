using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;
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
                            ExpectException<IOException>(() => Click(window, buttonName));
                        Check(!application.Dispatcher.HasShutdownStarted, "Failed save shut down the dispatcher.");
                    }

                    var parameter = (MaskedTextBox)window.FindName("tbAppParameter");
                    var previous = File.ReadAllBytes(configPath);
                    parameter.Text = "invalid\u0001input";
                    ExpectException<InvalidDataException>(() => Click(window, "bSaveAppSetting"));
                    Check(System.Linq.Enumerable.SequenceEqual(previous, File.ReadAllBytes(configPath)),
                        "Invalid XML input damaged the application configuration.");
                    Check(!application.Dispatcher.HasShutdownStarted, "Invalid input shut down the editor.");
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
                        ExpectException<IOException>(() => Click(window, "bSaveGlobalSetting"));
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
                        ExpectException<IOException>(() => saveAs("Added profile"));
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

        internal static void UnavailableProfileSelectionPreservesConfiguration()
        {
            foreach (var missing in new[] { "location", "timezone" })
            {
                WithConfig(directory =>
                {
                    var original = new LEProfile(true);
                    if (missing == "location") original.Location = "unavailable-culture";
                    else original.Timezone = "Unavailable Time Zone";
                    var path = Path.Combine(directory, "target.exe.le.config");
                    typeof(App).GetField("StandaloneFilePath", BindingFlags.Static | BindingFlags.NonPublic)
                        .SetValue(null, path);
                    LEConfig.SaveApplicationConfigFile(path, original);
                    LEConfig.SaveGlobalConfigFile(original);
                    var appWindow = new AppConfig();
                    var globalWindow = new GlobalConfig();
                    try
                    {
                        ((ComboBox)globalWindow.FindName("cbGlobalProfiles")).SelectedIndex = 0;
                        ExpectException<InvalidDataException>(() => Invoke(appWindow, "SaveSetting"));
                        ExpectException<InvalidDataException>(() => Click(globalWindow, "bSaveGlobalSetting"));
                        var saveAs = (Action<string>)Delegate.CreateDelegate(typeof(Action<string>), globalWindow,
                            typeof(GlobalConfig).GetMethod("SaveProfileAs", BindingFlags.Instance | BindingFlags.NonPublic));
                        ExpectException<InvalidDataException>(() => saveAs("Invalid selection"));
                        Check(LEConfig.GetProfiles(path)[0].Equals(original)
                            && LEConfig.GetProfiles()[0].Equals(original)
                            && Profiles(globalWindow).Count == 1 && Profiles(globalWindow)[0].Equals(original),
                            "Unavailable selections changed persisted or in-memory profiles.");
                        foreach (var window in new Window[] { appWindow, globalWindow })
                        {
                            ((ComboBox)window.FindName("cbLocation")).SelectedIndex = 0;
                            ((ComboBox)window.FindName("cbTimezone")).SelectedIndex = 0;
                        }
                        Invoke(appWindow, "SaveSetting");
                        Click(globalWindow, "bSaveGlobalSetting");
                        Check(LEConfig.GetProfiles(path).Length == 1 && LEConfig.GetProfiles().Length == 1,
                            "Selecting available values could not recover the editors.");
                    }
                    finally { appWindow.Close(); globalWindow.Close(); }
                });
            }
        }

        internal static void ReadOnlyLanguageLoadsAndReleasesFile()
        {
            var start = new System.Diagnostics.ProcessStartInfo(
                Assembly.GetExecutingAssembly().Location, "--language-probe") { UseShellExecute = false };
            using (var child = System.Diagnostics.Process.Start(start))
            {
                try
                {
                    Check(child.WaitForExit(15000), "Language probe timed out.");
                    Check(child.ExitCode == 0, "Read-only language probe failed.");
                }
                finally { if (!child.HasExited) child.Kill(); }
            }
        }

        internal static void ShortcutWithoutAssociatedIconUsesShellDefault()
        {
            var method = typeof(AppConfig).GetMethod("GetShortcutIconPath",
                BindingFlags.Static | BindingFlags.NonPublic);
            var extension = ".le-no-icon-" + Guid.NewGuid().ToString("N");
            var path = Path.Combine(Path.GetTempPath(), "target" + extension);
            var arguments = new object[] { path, 0 };
            Check((string)method.Invoke(null, arguments) == null,
                "A missing associated icon should leave the Shell Link default icon unchanged.");
        }

        internal static void AssociatedIconPathAndIndexAreParsed()
        {
            var parser = typeof(AssociationReader).GetMethod("ParseAssociatedIcon",
                BindingFlags.Static | BindingFlags.NonPublic);
            var arguments = new object[] { "\"C:\\Program Files\\wordicon.exe\",13", 0 };
            var path = (string)parser.Invoke(null, arguments);
            Check(path == @"C:\Program Files\wordicon.exe" && (int)arguments[1] == 13,
                "A registry DefaultIcon path and index were not separated for Shell Link.");
        }

        internal static void RunLanguageProbe()
        {
            System.Threading.Thread.CurrentThread.CurrentUICulture = System.Globalization.CultureInfo.GetCultureInfo("zh-CN");
            var path = Path.Combine(Path.GetDirectoryName(typeof(App).Assembly.Location), "Lang", "zh-CN.xaml");
            var original = File.ReadAllBytes(path);
            var expected = System.Xml.Linq.XDocument.Load(path);
            var keyName = System.Xml.Linq.XName.Get("Key", "http://schemas.microsoft.com/winfx/2006/xaml");
            var translation = System.Linq.Enumerable.First(expected.Root.Elements(), element => (string)element.Attribute(keyName) == "Save").Value;
            var attributes = File.GetAttributes(path);
            var moved = path + ".language-probe";
            try
            {
                File.SetAttributes(path, attributes | FileAttributes.ReadOnly);
                WithConfig(directory =>
                {
                    var type = typeof(App).Assembly.GetType("LEGUI.I18n", true);
                    type.GetMethod("LoadLanguage", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, null);
                    Check((string)Application.Current.Resources.MergedDictionaries[0]["Save"] == translation,
                        "The read-only external translation was not loaded.");
                    File.SetAttributes(path, attributes);
                    File.Move(path, moved);
                    File.Move(moved, path);
                    try
                    {
                        File.WriteAllText(path,
                            "<ResourceDictionary xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" " +
                            "xmlns:system=\"clr-namespace:System;assembly=mscorlib\"><system:String x:Key=\"Save\">Save</system:String></ResourceDictionary>",
                            Encoding.UTF8);
                        type.GetField("cacheDictionary", BindingFlags.Static | BindingFlags.NonPublic).SetValue(null, null);
                        type.GetMethod("LoadLanguage", BindingFlags.Static | BindingFlags.NonPublic).Invoke(null, null);
                        var getString = type.GetMethod("GetString", BindingFlags.Static | BindingFlags.NonPublic);
                        Check((string)getString.Invoke(null, new object[] { "MissingKey" }) == "MissingKey",
                            "A missing translation key did not fall back to its key.");
                    }
                    finally
                    {
                        File.WriteAllBytes(path, original);
                        type.GetField("cacheDictionary", BindingFlags.Static | BindingFlags.NonPublic).SetValue(null, null);
                    }
                });
            }
            finally
            {
                if (!File.Exists(path) && File.Exists(moved)) File.Move(moved, path);
                File.SetAttributes(path, attributes);
            }
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

        private static void ExpectException<T>(Action action) where T : Exception
        {
            try { action(); }
            catch (T) { return; }
            throw new Exception("Expected the GUI save event to stop with " + typeof(T).Name + ".");
        }

        private static void Check(bool condition, string message)
        {
            if (!condition) throw new Exception(message);
        }
    }
}

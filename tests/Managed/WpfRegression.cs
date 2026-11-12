using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
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

        internal static void LanguagePresetsResolveCulturesAndTimezones()
        {
            var presets = GetLanguagePresets();
            Check(presets.Length > 0, "No language presets were offered.");
            foreach (var preset in presets)
            {
                var type = preset.GetType();
                CultureInfo.GetCultureInfo((string)type.GetProperty("Location").GetValue(preset, null));
                TimeZoneInfo.FindSystemTimeZoneById((string)type.GetProperty("Timezone").GetValue(preset, null));
            }
        }

        internal static void LanguagePresetsHaveUniqueLanguagesAndLocations()
        {
            var languages = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var locations = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var preset in GetLanguagePresets())
            {
                var type = preset.GetType();
                var language = (string)type.GetProperty("Language").GetValue(preset, null);
                var location = (string)type.GetProperty("Location").GetValue(preset, null);
                Check(languages.Add(language), "Duplicate preset language: " + language);
                Check(locations.Add(location), "Duplicate preset location: " + location);
            }
        }

        internal static void LanguagePresetsCreateProfilesWithFreshGuidsAndDefaults()
        {
            var guids = new HashSet<Guid>();
            foreach (var preset in GetLanguagePresets())
            {
                var type = preset.GetType();
                var location = (string)type.GetProperty("Location").GetValue(preset, null);
                var timezone = (string)type.GetProperty("Timezone").GetValue(preset, null);
                var createProfile = (Func<string, LEProfile>)Delegate.CreateDelegate(typeof(Func<string, LEProfile>),
                    preset, type.GetMethod("CreateProfile"));
                const string name = "Preset regression";
                foreach (var profile in new[] { createProfile(name), createProfile(name) })
                {
                    Guid guid;
                    Check(Guid.TryParse(profile.Guid, out guid) && guid != Guid.Empty && guids.Add(guid),
                        "Preset profile has an invalid, empty or reused GUID: " + profile.Guid);
                    Check(profile.Name == name && profile.Location == location && profile.Timezone == timezone,
                        "Preset profile did not preserve its requested name, region and time zone.");
                    Check(!profile.RunAsAdmin && profile.RedirectRegistry && !profile.ShowInMainMenu,
                        "Preset profile elevation, registry redirection or main menu defaults changed.");
                }
            }
        }

        internal static void LanguagePresetNamesAvoidCollisions()
        {
            var type = typeof(App).Assembly.GetType("LEGUI.LanguagePresets", true);
            var uniqueName = (Func<string, IEnumerable<string>, string>)Delegate.CreateDelegate(
                typeof(Func<string, IEnumerable<string>, string>), type.GetMethod("UniqueName"));
            const string name = "Run in X";
            Check(uniqueName(name, new string[0]) == name, "An unused preset name was changed.");
            Check(uniqueName(name, new[] { "Run in Y", "Run in X (2)" }) == name,
                "An unused base name was changed because a suffixed name was taken.");
            Check(uniqueName(name, new[] { name }) == "Run in X (2)",
                "A duplicate preset name did not receive the first suffix.");
            Check(uniqueName(name, new[] { "run IN x" }) == "Run in X (2)",
                "Preset name collisions were not case-insensitive.");
            Check(uniqueName(name, new[] { "RUN IN X", "run in x (2)" }) == "Run in X (3)",
                "Preset naming did not skip a taken suffix case-insensitively.");
        }

        private static object[] GetLanguagePresets()
        {
            var type = typeof(App).Assembly.GetType("LEGUI.LanguagePresets", true);
            return ((IEnumerable)type.GetField("All").GetValue(null)).Cast<object>().ToArray();
        }

        internal static void AppConfigSaveFailureDoesNotLaunchOrShutdown()
        {
            WithConfig(directory =>
            {
                var configPath = Path.Combine(directory, "folder.le.config", "target.exe.LE.CONFIG");
                Directory.CreateDirectory(Path.GetDirectoryName(configPath));
                typeof(App).GetField("StandaloneFilePath", BindingFlags.Static | BindingFlags.NonPublic)
                    .SetValue(null, configPath);
                LEConfig.SaveApplicationConfigFile(configPath, LEProfile.CreateDefault("Regression", false));
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
                var original = LEProfile.CreateDefault("Regression", false);
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
                var original = LEProfile.CreateDefault("Regression", false);
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
                    var original = LEProfile.CreateDefault("Regression", false);
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

        internal static void LocExtensionUsesUiCultureAndPropertyTypes()
        {
            var start = new System.Diagnostics.ProcessStartInfo(
                Assembly.GetExecutingAssembly().Location, "--language-probe") { UseShellExecute = false };
            using (var child = System.Diagnostics.Process.Start(start))
            {
                try
                {
                    Check(child.WaitForExit(15000), "Language probe timed out.");
                    Check(child.ExitCode == 0, "LocExtension language probe failed.");
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
            var originalCulture = System.Threading.Thread.CurrentThread.CurrentUICulture;
            try
            {
                foreach (var culture in new[] { "en", "ja", "ja-JP" })
                {
                    System.Threading.Thread.CurrentThread.CurrentUICulture = System.Globalization.CultureInfo.GetCultureInfo(culture);
                    var expectedText = culture == "en" ? "Save" : "保存";
                    var expectedFont = culture == "en" ? "Segoe UI" : "Meiryo UI";
                    var text = new LocExtension("Save").ProvideValue(new PropertyTarget(TextBlock.TextProperty));
                    var font = new LocExtension("UIFont").ProvideValue(new PropertyTarget(System.Windows.Documents.TextElement.FontFamilyProperty));
                    Check(text is string && (string)text == expectedText, "Localized text was not returned as a string.");
                    Check(font is System.Windows.Media.FontFamily && ((System.Windows.Media.FontFamily)font).Source == expectedFont,
                        "Localized font was not returned as a FontFamily.");
                    Check((string)new LocExtension("MissingKey").ProvideValue(new PropertyTarget(TextBlock.TextProperty)) == "MissingKey",
                        "A missing translation key did not fall back to its key.");
                }
                LocalizationRegression.AssertSatelliteLocations();
            }
            finally { System.Threading.Thread.CurrentThread.CurrentUICulture = originalCulture; }
        }

        private sealed class PropertyTarget : IServiceProvider, System.Windows.Markup.IProvideValueTarget
        {
            private readonly DependencyProperty property;
            internal PropertyTarget(DependencyProperty property) { this.property = property; }
            public object TargetObject { get { return null; } }
            public object TargetProperty { get { return property; } }
            public object GetService(Type serviceType)
            {
                return serviceType == typeof(System.Windows.Markup.IProvideValueTarget) ? this : null;
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

using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Resources;
using System.Threading;
using System.Windows.Forms;
using System.Xml.Linq;

namespace ManagedRegression
{
    internal static class LocalizationRegression
    {
        internal static void ValidateSourceResources(string repository)
        {
            var cultures = new HashSet<string>(CultureInfo.GetCultures(CultureTypes.AllCultures)
                .Select(culture => culture.Name), StringComparer.OrdinalIgnoreCase);
            foreach (var project in new[] { "LEGUI", "LEContextMenuHandler", "LEInstaller", "LECommonLibrary" })
            {
                var directory = Path.Combine(repository, project);
                var neutral = ReadResxKeys(Path.Combine(directory, "Strings.resx"));
                var menu = project == "LEContextMenuHandler";
                if (menu)
                    Check(neutral.SetEquals(new[] { "RunDefault", "Submenu", "ManageApp", "ManageAll" }),
                        "Neutral menu resources must contain the four menu keys.");
                var translations = Directory.GetFiles(directory, "Strings.*.resx");
                Check(translations.Length > 0, "No translations found: " + directory);
                foreach (var path in translations)
                {
                    var culture = Path.GetFileNameWithoutExtension(path).Substring("Strings.".Length);
                    Check(cultures.Contains(culture) && !string.IsNullOrEmpty(culture)
                        && string.Equals(CultureInfo.GetCultureInfo(culture).Name, culture, StringComparison.OrdinalIgnoreCase),
                        "Invalid or noncanonical .NET culture: " + path);
                    var translated = ReadResxKeys(path);
                    Check(translated.IsSubsetOf(neutral), "Translation contains unknown keys: " + path);
                    if (menu)
                        Check(translated.SetEquals(neutral), "Menu translation is incomplete: " + path);
                }
                Console.WriteLine("PASS ResxSources " + project + " (" + neutral.Count + " keys, "
                    + translations.Length + " translations)");
            }
        }

        private static HashSet<string> ReadResxKeys(string path)
        {
            var document = XDocument.Load(path);
            Check(document.Root != null && document.Root.Name == "root", "Invalid resx root: " + path);
            var keys = new HashSet<string>(StringComparer.Ordinal);
            foreach (var data in document.Root.Elements("data"))
            {
                var key = (string)data.Attribute("name");
                Check(!string.IsNullOrWhiteSpace(key), "Empty resource key: " + path);
                Check(keys.Add(key), "Duplicate resource key: " + path + "/" + key);
                Check(data.Elements("value").Count() == 1, "Resource must have one value: " + path + "/" + key);
            }
            Check(keys.Count > 0, "Empty resource set: " + path);
            using (var reader = new ResXResourceReader(path))
            {
                var parsed = new HashSet<string>(StringComparer.Ordinal);
                foreach (DictionaryEntry entry in reader)
                {
                    Check(entry.Key is string && entry.Value is string, "Resource must be a string: " + path);
                    parsed.Add((string)entry.Key);
                }
                Check(parsed.SetEquals(keys), "Resx reader did not load all keys: " + path);
            }
            return keys;
        }

        internal static void RunLanguageProbe()
        {
            var previousCulture = Thread.CurrentThread.CurrentUICulture;
            try
            {
                var common = LoadProduct("LECommonLibrary.dll");
                var menu = LoadProduct("LEContextMenuHandler.dll");
                var gui = LoadProduct("LEGUI.exe");
                var installer = LoadProduct("LEInstaller.exe");
                AssertText(common, "zh-CN", "VersionTitle", "Locale Emulator 版本 {0}");
                AssertText(menu, "zh-CN", "RunDefault", "以此程序配置运行");
                AssertText(gui, "zh-CN", "Save", "保存");
                AssertText(installer, "zh-CN", "Title", "Locale Emulator 安装程序");
                AssertText(menu, "zh-TW", "RunDefault", "以此程式設定執行");
                AssertText(gui, "zh-TW", "Save", "儲存");
                AssertText(menu, "ja-JP", "RunDefault", "プロファイルで実行");
                AssertText(gui, "ja-JP", "UIFont", "Meiryo UI");
                AssertText(common, "en-US", "VersionTitle", "Locale Emulator Version {0}");
                foreach (var culture in new[] { "zh-TW", "ja-JP", "sv-SE" })
                    AssertText(common, culture, "VersionTitle", "Locale Emulator Version {0}");

                foreach (var product in new[] { common, menu, gui, installer })
                {
                    var key = product == menu ? "RunDefault" : product == gui ? "Shortcut" : "Title";
                    Thread.CurrentThread.CurrentUICulture = CultureInfo.GetCultureInfo("en-US");
                    var english = GetString(product, key);
                    Check(english != key, "Neutral resource was not loaded: " + product.FullName);
                    AssertText(product, "sv-SE", key, english);
                    foreach (var culture in new[] { "en-US", "zh-CN", "zh-TW", "ja-JP", "sv-SE" })
                        AssertText(product, culture, "MissingLocalizationRegressionKey", "MissingLocalizationRegressionKey");
                }

                Thread.CurrentThread.CurrentUICulture = CultureInfo.GetCultureInfo("en-US");
                var fallback = GetString(gui, "SaveOperationError");
                Check(fallback != "SaveOperationError", "Neutral GUI fallback key was not loaded.");
                AssertText(gui, "zh-TW", "SaveOperationError", fallback);
                Thread.CurrentThread.CurrentUICulture = CultureInfo.GetCultureInfo("zh-CN");
                var formType = installer.GetType("LEInstaller.Form1", true);
                using (var form = (Form)Activator.CreateInstance(formType, new object[] { 0 }))
                    Check(form.Text == "Locale Emulator 安装程序", "Installer window did not apply zh-CN resources.");
                AssertSatelliteLocations();
            }
            finally
            {
                Thread.CurrentThread.CurrentUICulture = previousCulture;
            }
        }

        internal static void AssertSatelliteLocations()
        {
            var directory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            var products = new[] { "LECommonLibrary.resources", "LEContextMenuHandler.resources",
                "LEGUI.resources", "LEInstaller.resources" };
            foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
            {
                var name = assembly.GetName();
                if (!products.Contains(name.Name)) continue;
                var expected = Path.Combine(directory, name.CultureInfo.Name, name.Name + ".dll");
                Check(!assembly.GlobalAssemblyCache && string.Equals(assembly.Location, expected, StringComparison.OrdinalIgnoreCase),
                    "Satellite resolved outside the probe directory: " + assembly.FullName + " at " + assembly.Location);
            }
        }

        private static Assembly LoadProduct(string name)
        {
            var path = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), name);
            var assembly = Assembly.LoadFrom(path);
            Check(string.Equals(assembly.Location, path, StringComparison.OrdinalIgnoreCase),
                "Product resolved outside the probe directory: " + assembly.Location);
            return assembly;
        }

        private static string GetString(Assembly assembly, string key)
        {
            var typeName = assembly.GetName().Name + (assembly.GetName().Name == "LECommonLibrary" ? ".Localization" : ".I18n");
            return (string)assembly.GetType(typeName, true).GetMethod("GetString",
                BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic).Invoke(null, new object[] { key });
        }

        private static void AssertText(Assembly assembly, string culture, string key, string expected)
        {
            Thread.CurrentThread.CurrentUICulture = CultureInfo.GetCultureInfo(culture);
            var actual = GetString(assembly, key);
            Check(actual == expected, assembly.GetName().Name + "/" + culture + "/" + key
                + ": expected [" + expected + "], got [" + actual + "].");
        }

        internal static void ResourceManagersFollowCultureAndFallback()
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

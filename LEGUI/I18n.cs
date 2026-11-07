using System;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Windows;
using System.Windows.Markup;

namespace LEGUI
{
    internal class I18n
    {
        internal static readonly CultureInfo CurrentCultureInfo = CultureInfo.CurrentUICulture;
        //internal static readonly CultureInfo CurrentCultureInfo = CultureInfo.GetCultureInfo("zh-CN");

        private static ResourceDictionary cacheDictionary;
        private static ResourceDictionary fallbackDictionary;

        internal static string GetString(string key)
        {
            var dict = LoadDictionary();
            var value = dict.Contains(key) ? dict[key] as string : null;
            if (string.IsNullOrEmpty(value) && fallbackDictionary != null && !ReferenceEquals(dict, fallbackDictionary))
                value = fallbackDictionary.Contains(key) ? fallbackDictionary[key] as string : null;
            return string.IsNullOrEmpty(value) ? key : value;
        }

        private static ResourceDictionary LoadDictionary()
        {
            if (cacheDictionary != null)
                return cacheDictionary;
            if (Application.Current.Resources.MergedDictionaries.Count > 0)
                fallbackDictionary = Application.Current.Resources.MergedDictionaries[0];

            ResourceDictionary dictionary = null;
            try
            {
                var langDir = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), @"Lang\");
                var candidates = new[]
                {
                    CurrentCultureInfo.Name + ".xaml",
                    CurrentCultureInfo.TwoLetterISOLanguageName + ".xaml"
                };
                foreach (var candidate in candidates)
                {
                    var path = Path.Combine(langDir, candidate);
                    if (!File.Exists(path))
                        continue;
                    using (var stream = File.OpenRead(path))
                        dictionary = XamlReader.Load(stream) as ResourceDictionary;
                    if (dictionary != null)
                        break;
                }
            }
            catch (Exception error) when (error is IOException || error is UnauthorizedAccessException
                                          || error is XamlParseException)
            {
                // 可选翻译读取失败时继续使用内置英文资源。
            }

            if (dictionary == null)
                dictionary = fallbackDictionary;
            if (dictionary == null)
                throw new Exception("No language file.");

            cacheDictionary = dictionary;
            return cacheDictionary;
        }

        internal static void LoadLanguage()
        {
            var dict = LoadDictionary();

            Application.Current.Resources.MergedDictionaries.Clear();
            Application.Current.Resources.MergedDictionaries.Add(dict);
        }
    }
}
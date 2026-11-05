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

        internal static string GetString(string key)
        {
            var dict = LoadDictionary();
            var value = dict[key] as string;
            return string.IsNullOrEmpty(value) ? key : value;
        }

        private static ResourceDictionary LoadDictionary()
        {
            if (cacheDictionary != null)
                return cacheDictionary;

            ResourceDictionary dictionary = null;
            try
            {
                var langDir = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), @"Lang\");

                var firstLangPath = Path.Combine(langDir, CurrentCultureInfo.Name + ".xaml");
                var fallbackLangPath = Path.Combine(langDir,
                                                    $@"{CurrentCultureInfo.TwoLetterISOLanguageName}.xaml");

                var path = File.Exists(firstLangPath) ? firstLangPath : fallbackLangPath;
                using (var stream = File.OpenRead(path))
                    dictionary = XamlReader.Load(stream) as ResourceDictionary;
                if (dictionary != null)
                    Application.Current.Resources.MergedDictionaries.Insert(0, dictionary);
            }
            catch (Exception error) when (error is IOException || error is UnauthorizedAccessException
                                          || error is XamlParseException)
            {
                // External translations are optional; preserve the bundled default on read or parse failure.
            }

            //If dictionary is still null, use default language.
            if (dictionary == null)
                if (Application.Current.Resources.MergedDictionaries.Count > 0)
                    dictionary = Application.Current.Resources.MergedDictionaries[0];
                else
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
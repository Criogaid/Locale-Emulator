using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Xml.Linq;

namespace LECommonLibrary
{
    public static class Localization
    {
        private static readonly CultureInfo CurrentCultureInfo = CultureInfo.CurrentUICulture;
        private static Dictionary<string, string> cache;

        public static string GetString(string key)
        {
            if (cache == null)
                cache = LoadDictionary();

            string value;
            return cache.TryGetValue(key, out value) && !string.IsNullOrEmpty(value) ? value : key;
        }

        private static Dictionary<string, string> LoadDictionary()
        {
            var directory = Path.Combine(GlobalHelper.ProductDirectory, "CommonLang");
            var candidates = new[]
            {
                CurrentCultureInfo.Name,
                CurrentCultureInfo.TwoLetterISOLanguageName,
                "DefaultLanguage"
            };

            foreach (var candidate in candidates.Distinct(StringComparer.OrdinalIgnoreCase))
            {
                var path = Path.Combine(directory, candidate + ".xml");
                if (!File.Exists(path))
                    continue;

                try
                {
                    return XDocument.Load(path).Root.Elements()
                        .ToDictionary(element => element.Name.LocalName, element => element.Value,
                            StringComparer.Ordinal);
                }
                catch (Exception error) when (error is IOException || error is UnauthorizedAccessException
                                              || error is System.Xml.XmlException)
                {
                    // 可选翻译读取失败时继续尝试下一个语言或内置英文资源。
                }
            }

            return new Dictionary<string, string>(StringComparer.Ordinal);
        }
    }
}

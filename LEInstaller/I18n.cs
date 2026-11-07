using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Xml.Linq;

namespace LEInstaller
{
    internal static class I18n
    {
        private static readonly CultureInfo CurrentCultureInfo = CultureInfo.CurrentUICulture;
        private static Dictionary<string, string> cache;

        internal static string GetString(string key)
        {
            if (cache == null)
                cache = LoadDictionary();

            string value;
            return cache.TryGetValue(key, out value) && !string.IsNullOrEmpty(value) ? value : key;
        }

        private static Dictionary<string, string> LoadDictionary()
        {
            var directory = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "InstallerLang");
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
                catch (Exception error) when (error is IOException || error is UnauthorizedAccessException || error is System.Xml.XmlException)
                {
                }
            }

            return new Dictionary<string, string>(StringComparer.Ordinal);
        }
    }
}

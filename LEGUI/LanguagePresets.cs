using System;
using System.Collections.Generic;
using System.Linq;
using LECommonLibrary;

namespace LEGUI
{
    /// <summary>A ready-made region + time zone pairing offered by the "New" button.</summary>
    internal sealed class LanguagePreset
    {
        public LanguagePreset(string language, string location, string timezone)
        {
            Language = language;
            Location = location;
            Timezone = timezone;
        }

        public string Language { get; }
        public string Location { get; }
        public string Timezone { get; }

        public string DefaultName => "Run in " + Language;

        public LEProfile CreateProfile(string name)
        {
            return new LEProfile(name, Guid.NewGuid().ToString(), false, string.Empty,
                                 Location, Timezone, false, true, false, false);
        }
    }

    internal static class LanguagePresets
    {
        // Time zone ids are the Windows ids that TimeZoneInfo uses, so they match the values the profile editor stores.
        public static readonly IReadOnlyList<LanguagePreset> All = new[]
        {
            new LanguagePreset("Japanese", "ja-JP", "Tokyo Standard Time"),
            new LanguagePreset("Simplified Chinese", "zh-CN", "China Standard Time"),
            new LanguagePreset("Traditional Chinese (Taiwan)", "zh-TW", "Taipei Standard Time"),
            new LanguagePreset("Traditional Chinese (Hong Kong)", "zh-HK", "China Standard Time"),
            new LanguagePreset("Korean", "ko-KR", "Korea Standard Time"),
            new LanguagePreset("English (US)", "en-US", "Pacific Standard Time"),
            new LanguagePreset("Russian", "ru-RU", "Russian Standard Time"),
            new LanguagePreset("Thai", "th-TH", "SE Asia Standard Time"),
            new LanguagePreset("German", "de-DE", "W. Europe Standard Time"),
            new LanguagePreset("French", "fr-FR", "Romance Standard Time"),
            new LanguagePreset("Spanish", "es-ES", "Romance Standard Time"),
            new LanguagePreset("Portuguese (Brazil)", "pt-BR", "E. South America Standard Time"),
            new LanguagePreset("Turkish", "tr-TR", "Turkey Standard Time"),
        };

        /// <summary>A name not yet used by <paramref name="existing"/>: "Run in X", then "Run in X (2)", ...</summary>
        public static string UniqueName(string baseName, IEnumerable<string> existing)
        {
            var taken = new HashSet<string>(existing, StringComparer.OrdinalIgnoreCase);
            if (!taken.Contains(baseName))
                return baseName;
            for (var i = 2; ; i++)
            {
                var candidate = baseName + " (" + i + ")";
                if (!taken.Contains(candidate))
                    return candidate;
            }
        }
    }
}

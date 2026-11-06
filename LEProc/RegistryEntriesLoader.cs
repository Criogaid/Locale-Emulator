using System.Linq;
using System.Globalization;

namespace LEProc
{
    public static class RegistryEntriesLoader
    {
        private static readonly RegistryEntry[] entries;
        private static readonly RegistryEntry[] entriesIncludingAdvanced;

        static RegistryEntriesLoader()
        {
            entries = new[]
            {
                new RegistryEntry("HKEY_LOCAL_MACHINE", @"System\CurrentControlSet\Control\Nls\Language", "InstallLanguage", "REG_SZ", GetLanguageId),
                new RegistryEntry("HKEY_LOCAL_MACHINE", @"System\CurrentControlSet\Control\Nls\Language", "Default", "REG_SZ", GetLanguageId),
                new RegistryEntry("HKEY_LOCAL_MACHINE", @"System\CurrentControlSet\Control\Nls\CodePage", "OEMCP", "REG_SZ", culture => culture.TextInfo.OEMCodePage.ToString()),
                new RegistryEntry("HKEY_LOCAL_MACHINE", @"System\CurrentControlSet\Control\Nls\CodePage", "ACP", "REG_SZ", culture => culture.TextInfo.ANSICodePage.ToString())

            };

            var advancedEntries = new[]
            {
                new RegistryEntry("HKEY_CURRENT_USER", @"Control Panel\International", "Locale", "REG_SZ", culture => culture.TextInfo.LCID.ToString("X8")),
                new RegistryEntry("HKEY_CURRENT_USER", @"Control Panel\International", "LocaleName", "REG_SZ", culture => $"{culture.TextInfo.CultureName}\x00"),
                new RegistryEntry("HKEY_CURRENT_USER", @"Control Panel\Desktop", "PreferredUILanguages", "REG_MULTI_SZ", culture => $"{culture.TextInfo.CultureName}\x00"),
                new RegistryEntry("HKEY_CURRENT_USER", @"Control Panel\Desktop\MuiCached", "MachinePreferredUILanguages", "REG_MULTI_SZ", culture => $"{culture.TextInfo.CultureName}\x00")
            };

            entriesIncludingAdvanced = entries.Concat(advancedEntries).ToArray();
        }

        public static RegistryEntry[] GetRegistryEntries(bool isAdvanced)
        {
            return isAdvanced ? entriesIncludingAdvanced : entries;
        }

        private static string GetLanguageId(CultureInfo culture)
        {
            return (culture.LCID & 0xffff).ToString("X4", CultureInfo.InvariantCulture);
        }
    }
}
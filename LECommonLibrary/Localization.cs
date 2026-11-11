using System.Globalization;
using System.Resources;

namespace LECommonLibrary
{
    /// <summary>
    /// Product-owned UI and diagnostic text from the standard Strings.resx resources.
    /// Language follows the Windows UI language; a missing key falls back through the culture chain
    /// to the neutral English resource.
    /// </summary>
    public static class Localization
    {
        private static readonly ResourceManager Manager =
            new ResourceManager("LECommonLibrary.Strings", typeof(Localization).Assembly);

        public static string GetString(string key)
        {
            try
            {
                var value = Manager.GetString(key, CultureInfo.CurrentUICulture);
                return string.IsNullOrEmpty(value) ? key : value;
            }
            catch (MissingManifestResourceException)
            {
                return key;
            }
        }
    }
}

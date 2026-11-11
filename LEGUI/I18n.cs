using System.Globalization;
using System.Reflection;
using System.Resources;

namespace LEGUI
{
    /// <summary>
    /// UI text from the standard Strings.resx; satellite assemblies provide translations and a
    /// missing key falls back through the culture chain to the neutral English resource.
    /// </summary>
    internal static class I18n
    {
        private static readonly ResourceManager Manager =
            new ResourceManager("LEGUI.Strings", Assembly.GetExecutingAssembly());

        internal static string GetString(string key)
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

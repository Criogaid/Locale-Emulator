namespace LECommonLibrary
{
    public struct LEProfile
    {
        public string Guid;
        public bool IsAdvancedRedirection;
        public string Location;
        public string Name;
        public string Parameter;
        public bool RedirectRegistry;
        public bool RunAsAdmin;
        public bool RunWithSuspend;
        public bool ShowInMainMenu;
        public string Timezone;

        /// <summary>The one place that defines the built-in region and time zone.</summary>
        public const string DefaultLocation = "ja-JP";
        public const string DefaultTimezone = "Tokyo Standard Time";

        /// <summary>
        ///     Create a new profile using the built-in default (ja-JP) settings.
        /// </summary>
        public static LEProfile CreateDefault(string name, bool runAsAdmin)
        {
            return new LEProfile(
                name,
                System.Guid.NewGuid().ToString(),
                false,
                string.Empty,
                DefaultLocation,
                DefaultTimezone,
                runAsAdmin,
                true,
                false,
                false);
        }

        /// <summary>
        ///     Create a new LEProfile using arguments.
        /// </summary>
        public LEProfile(string name,
                         string guid,
                         bool showInMainMenu,
                         string parameter,
                         string location,
                         string timezone,
                         bool runAsAdmin,
                         bool redirectRegistry,
                         bool isAdvancedRedirection,
                         bool runWithSuspend)
        {
            Name = name;
            Guid = guid;
            ShowInMainMenu = showInMainMenu;
            Parameter = parameter;
            Location = location;
            Timezone = timezone;
            RunAsAdmin = runAsAdmin;
            RedirectRegistry = redirectRegistry;
            IsAdvancedRedirection = isAdvancedRedirection;
            RunWithSuspend = runWithSuspend;
        }
    }
}
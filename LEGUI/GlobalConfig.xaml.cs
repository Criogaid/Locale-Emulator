using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media.Effects;
using LECommonLibrary;

namespace LEGUI
{
    /// <summary>
    ///     Interaction logic for GlobalConfig.xaml
    /// </summary>
    public partial class GlobalConfig
    {
        private readonly List<CultureInfo> _cultureInfos = new List<CultureInfo>();
        private List<LEProfile> _profiles = new List<LEProfile>();
        private Exception _unreadableConfig;
        private readonly List<TimeZoneInfo> _timezones = new List<TimeZoneInfo>();

        public GlobalConfig()
        {
            InitializeComponent();

            // Region.
            _cultureInfos = CultureInfo.GetCultures(CultureTypes.AllCultures).OrderBy(i => i.DisplayName).ToList();
            cbLocation.ItemsSource = _cultureInfos.Select(c => c.DisplayName);

            //Timezone.
            _timezones = TimeZoneInfo.GetSystemTimeZones().ToList();
            cbTimezone.ItemsSource = _timezones.Select(t => t.DisplayName);

            //Profiles.
            try
            {
                _profiles = LEConfig.ReadProfiles(LEConfig.GlobalConfigPath).ToList();
            }
            catch (Exception error)
            {
                // A missing file is an empty list; an existing but unreadable one must not be overwritten.
                if (File.Exists(LEConfig.GlobalConfigPath))
                    _unreadableConfig = error;
            }
            cbGlobalProfiles.ItemsSource = _profiles.Select(p => p.Name);
        }

        private void SaveGlobalProfiles(params LEProfile[] profiles)
        {
            if (_unreadableConfig != null)
                throw new InvalidDataException(_unreadableConfig.Message, _unreadableConfig);

            LEConfig.SaveGlobalConfigFile(profiles);
        }

        private void bSaveGlobalSetting_Click(object sender, RoutedEventArgs e)
        {
            if (cbGlobalProfiles.SelectedIndex < 0)
                return;

            App.ValidateProfileSelection(cbLocation.SelectedIndex, cbTimezone.SelectedIndex);
            var crt = _profiles[cbGlobalProfiles.SelectedIndex];
            crt.Location = _cultureInfos[cbLocation.SelectedIndex].Name;
            crt.Timezone = _timezones[cbTimezone.SelectedIndex].Id;
            crt.ShowInMainMenu = cbShowInMainMenu.IsChecked != null && (bool)cbShowInMainMenu.IsChecked;

            crt.RunAsAdmin = cbStartAsAdmin.IsChecked != null && (bool)cbStartAsAdmin.IsChecked;
            crt.RedirectRegistry = cbRedirectRegistry.IsChecked != null && (bool)cbRedirectRegistry.IsChecked;
            crt.IsAdvancedRedirection = cbIsAdvancedRedirection.IsChecked != null
                                        && (bool)cbIsAdvancedRedirection.IsChecked;
            crt.RunWithSuspend = cbStartAsSuspend.IsChecked != null && (bool)cbStartAsSuspend.IsChecked;

            var profiles = _profiles.ToArray();
            profiles[cbGlobalProfiles.SelectedIndex] = crt;
            SaveGlobalProfiles(profiles);

            _profiles[cbGlobalProfiles.SelectedIndex] = crt;
        }

        private void cbGlobalProfiles_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            bDeleteGlobalSetting.IsEnabled = cbGlobalProfiles.Items.Count != 0;
            bSaveGlobalSetting.IsEnabled = cbGlobalProfiles.Items.Count != 0;

            if (cbGlobalProfiles.SelectedIndex == -1)
            {
                return;
            }

            var crt = _profiles[cbGlobalProfiles.SelectedIndex];

            cbTimezone.SelectedIndex = _timezones.FindIndex(tz => tz.Id == crt.Timezone);
            cbLocation.SelectedIndex = _cultureInfos.FindIndex(ci => ci.Name == crt.Location);

            cbShowInMainMenu.IsChecked = crt.ShowInMainMenu;
            cbStartAsAdmin.IsChecked = crt.RunAsAdmin;
            cbRedirectRegistry.IsChecked = crt.RedirectRegistry;
            cbIsAdvancedRedirection.IsChecked = crt.IsAdvancedRedirection;
            cbStartAsSuspend.IsChecked = crt.RunWithSuspend;
        }

        private void bSaveGlobalSettingAs_Click(object sender, RoutedEventArgs e)
        {
            mainGrid.Effect = new BlurEffect();

            try
            {
                var ib = new InputBox
                         {
                             Owner = this,
                             WindowStartupLocation = WindowStartupLocation.CenterOwner,
                             Instruction = I18n.GetString("SaveAsInstruction"),
                             OkText = I18n.GetString("Save"),
                             CancelText = I18n.GetString("Cancel")
                         };

                if (ib.ShowDialog() == true && !string.IsNullOrWhiteSpace(ib.Text))
                {
                    SaveProfileAs(ib.Text);

                    cbGlobalProfiles.SelectedIndex = _profiles.Count - 1;
                }
            }
            finally
            {
                mainGrid.Effect = null;
            }
        }

        private void SaveProfileAs(string name)
        {
            App.ValidateProfileSelection(cbLocation.SelectedIndex, cbTimezone.SelectedIndex);
            var pro = new LEProfile(name,
                                    Guid.NewGuid().ToString(),
                                    cbShowInMainMenu.IsChecked != null && (bool)cbShowInMainMenu.IsChecked,
                                    string.Empty,
                                    _cultureInfos[cbLocation.SelectedIndex].Name,
                                    _timezones[cbTimezone.SelectedIndex].Id,
                                    cbStartAsAdmin.IsChecked != null && (bool)cbStartAsAdmin.IsChecked,
                                    cbRedirectRegistry.IsChecked != null && (bool)cbRedirectRegistry.IsChecked,
                                    cbIsAdvancedRedirection.IsChecked != null && (bool)cbIsAdvancedRedirection.IsChecked,
                                    cbStartAsSuspend.IsChecked != null && (bool)cbStartAsSuspend.IsChecked);

            SaveGlobalProfiles(_profiles.Concat(new[] { pro }).ToArray());

            _profiles.Add(pro);

            // Update cbGlobalProfiles.
            cbGlobalProfiles.ItemsSource = _profiles.Select(p => p.Name);
        }

        private void bDeleteGlobalSetting_Click(object sender, RoutedEventArgs e)
        {
            if (cbGlobalProfiles.SelectedIndex == -1)
                return;

            if (MessageBoxResult.No == MessageBox.Show(I18n.GetString("ConfirmDel"), "", MessageBoxButton.YesNo))
                return;

            var selectedIndex = cbGlobalProfiles.SelectedIndex;
            SaveGlobalProfiles(_profiles.Where((profile, index) => index != selectedIndex).ToArray());

            _profiles.RemoveAt(selectedIndex);

            // Update cbGlobalProfiles.
            cbGlobalProfiles.ItemsSource = _profiles.Select(p => p.Name);
            cbGlobalProfiles.SelectedIndex = 0;
        }
    }
}
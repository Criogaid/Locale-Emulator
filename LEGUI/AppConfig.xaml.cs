using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices.ComTypes;
using System.Windows;
using LECommonLibrary;

namespace LEGUI
{
    /// <summary>
    ///     Interaction logic for AppConfig.xaml
    /// </summary>
    public partial class AppConfig
    {
        private readonly List<CultureInfo> _cultureInfos = new List<CultureInfo>();
        private readonly List<TimeZoneInfo> _timezones = new List<TimeZoneInfo>();

        private string ApplicationPath
        {
            get { return App.StandaloneFilePath.Substring(0, App.StandaloneFilePath.Length - ".le.config".Length); }
        }

        public AppConfig()
        {
            InitializeComponent();

            Title += Path.GetFileName(ApplicationPath);

            // Region.
            _cultureInfos = CultureInfo.GetCultures(CultureTypes.AllCultures).OrderBy(i => i.DisplayName).ToList();
            cbLocation.ItemsSource = _cultureInfos.Select(c => c.DisplayName);
            cbLocation.SelectedIndex = _cultureInfos.FindIndex(c => c.Name == "ja-JP");

            //Timezone.
            _timezones = TimeZoneInfo.GetSystemTimeZones().ToList();
            cbTimezone.ItemsSource = _timezones.Select(t => t.DisplayName);
            cbTimezone.SelectedIndex = _timezones.FindIndex(tz => tz.Id == "Tokyo Standard Time");

            // Load exists config.
            var configs = LEConfig.GetProfiles(App.StandaloneFilePath);
            if (configs.Length > 0)
            {
                var conf = configs[0];

                if (!string.IsNullOrEmpty(conf.Parameter))
                {
                    tbAppParameter.FontStyle = FontStyles.Normal;
                    tbAppParameter.Text = conf.Parameter;
                }
                cbTimezone.SelectedIndex = _timezones.FindIndex(tz => tz.Id == conf.Timezone);
                cbLocation.SelectedIndex = _cultureInfos.FindIndex(ci => ci.Name == conf.Location);

                cbStartAsAdmin.IsChecked = conf.RunAsAdmin;
                cbRedirectRegistry.IsChecked = conf.RedirectRegistry;
                cbIsAdvancedRedirection.IsChecked = conf.IsAdvancedRedirection;
                cbStartAsSuspend.IsChecked = conf.RunWithSuspend;
            }
        }

        private void SaveSetting()
        {
            App.ValidateProfileSelection(cbLocation.SelectedIndex, cbTimezone.SelectedIndex);
            var crt = new LEProfile(Path.GetFileName(App.StandaloneFilePath),
                                    Guid.NewGuid().ToString(),
                                    false,
                                    tbAppParameter.Text,
                                    _cultureInfos[cbLocation.SelectedIndex].Name,
                                    _timezones[cbTimezone.SelectedIndex].Id,
                                    cbStartAsAdmin.IsChecked != null && (bool)cbStartAsAdmin.IsChecked,
                                    cbRedirectRegistry.IsChecked != null && (bool)cbRedirectRegistry.IsChecked,
                                    cbIsAdvancedRedirection.IsChecked != null && (bool)cbIsAdvancedRedirection.IsChecked,
                                    cbStartAsSuspend.IsChecked != null && (bool)cbStartAsSuspend.IsChecked
                );

            LEConfig.SaveApplicationConfigFile(App.StandaloneFilePath, crt);
        }

        private void CreateShortcut(string path)
        {
            try
            {
                var link = (IShellLink)new ShellLink();

                link.SetPath(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location),
                                          "LEProc.exe"));
                link.SetArguments(WindowsCommandLine.JoinArguments(new[] { "-run", path }));
                link.SetIconLocation(AssociationReader.GetAssociatedIcon(Path.GetExtension(path)).Replace("%1", path), 0);

                link.SetDescription(string.Format(I18n.GetString("ShortcutDescription"), Path.GetFileName(path)));
                link.SetWorkingDirectory(Path.GetDirectoryName(path));

                var file = (IPersistFile)link;
                file.Save(
                          Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
                                       Path.GetFileNameWithoutExtension(path) + ".lnk"),
                          false);
            }
            catch (Exception e)
            {
                MessageBox.Show(e.Message + "\r\n\r\n" + e.StackTrace);
            }
        }

        private void RunAndShutdown()
        {
            //Run the application.
            Process.Start(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "LEProc.exe"),
                          WindowsCommandLine.JoinArguments(new[] { "-run", ApplicationPath }));

            Application.Current.Shutdown();
        }

        private void bSaveAppSetting_Click(object sender, RoutedEventArgs e)
        {
            SaveSetting();

            RunAndShutdown();
        }

        private void bShortcut_Click(object sender, RoutedEventArgs e)
        {
            SaveSetting();

            CreateShortcut(ApplicationPath);

            RunAndShutdown();
        }

        private void bDeleteAppSetting_Click(object sender, RoutedEventArgs e)
        {
            if (MessageBoxResult.No == MessageBox.Show(I18n.GetString("ConfirmDel"), "", MessageBoxButton.YesNo))
                return;

            if (File.Exists(App.StandaloneFilePath))
                File.Delete(App.StandaloneFilePath);

            Application.Current.Shutdown();
        }
    }
}
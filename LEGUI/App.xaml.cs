using System;
using System.IO;
using System.Windows;
using LECommonLibrary;

namespace LEGUI
{
    /// <summary>
    ///     Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        internal static string StandaloneFilePath = string.Empty;

        internal static void ValidateProfileSelection(int locationIndex, int timezoneIndex)
        {
            if (locationIndex < 0 || timezoneIndex < 0)
                throw new InvalidDataException(I18n.GetString("InvalidProfileSelection"));
        }

        protected override void OnStartup(StartupEventArgs e)
        {
            AppDomain.CurrentDomain.UnhandledException +=
                (sender, args) => MessageBox.Show(((Exception) args.ExceptionObject).Message,
                    I18n.GetString("Title"), MessageBoxButton.OK, MessageBoxImage.Error);
            DispatcherUnhandledException += (sender, args) =>
            {
                if (!(args.Exception is IOException) && !(args.Exception is UnauthorizedAccessException)
                    && !(args.Exception is InvalidDataException))
                    return;

                args.Handled = true;
                MessageBox.Show(string.Format(I18n.GetString("SaveOperationError"), args.Exception.Message),
                    I18n.GetString("Title"), MessageBoxButton.OK, MessageBoxImage.Error);
            };

            I18n.LoadLanguage();
            base.OnStartup(e);
        }

        private void App_OnStartup(object sender, StartupEventArgs e)
        {
            if (e.Args.Length != 0)
            {
                StandaloneFilePath = SystemHelper.EnsureAbsolutePath(e.Args[0]);

                // This happens when user is trying to drop a exe onto LEGUI.
                if (!StandaloneFilePath.EndsWith(".le.config", true, null))
                    StandaloneFilePath += ".le.config";
            }

            var isGlobalProfile = string.IsNullOrEmpty(StandaloneFilePath);

            // We check StandaloneFilePath before loading UI, because this wil be faster.
            if (
                !SystemHelper.CheckPermission(isGlobalProfile
                                                  ? Path.GetDirectoryName(LEConfig.GlobalConfigPath)
                                                  : Path.GetDirectoryName(StandaloneFilePath)))
            {
                if (SystemHelper.IsAdministrator())
                {
                    // We can do nothing now.
                    if (isGlobalProfile)
                        MessageBox.Show(
                                        string.Format(I18n.GetString("HomeDirectoryNotWritable"),
                                            Path.GetDirectoryName(LEConfig.GlobalConfigPath)),
                                        I18n.GetString("Title"),
                                        MessageBoxButton.OK,
                                        MessageBoxImage.Error);
                    else
                        MessageBox.Show(
                                        string.Format(I18n.GetString("StandaloneDirectoryNotWritable"),
                                            Path.GetDirectoryName(StandaloneFilePath)),
                                        I18n.GetString("Title"),
                                        MessageBoxButton.OK,
                                        MessageBoxImage.Error);

                    Current.Shutdown(1);
                }
                else
                {
                    // If we are not administrator, we can ask for administrator permission.
                    try
                    {
                        SystemHelper.RunWithElevatedProcess(
                                                            Path.Combine(
                                                                         Path.GetDirectoryName(LEConfig.GlobalConfigPath),
                                                                         "LEGUI.exe"),
                                                            e.Args);
                        Current.Shutdown();
                    }
                    catch (Exception)
                    {
                        MessageBox.Show(I18n.GetString("ElevationError"),
                                        I18n.GetString("Title"),
                                        MessageBoxButton.OK,
                                        MessageBoxImage.Error);
                        Current.Shutdown(1);
                    }
                }
                return;
            }

            try
            {
                LEConfig.CheckGlobalConfigFile(true);
            }
            catch (Exception error) when (error is IOException || error is UnauthorizedAccessException)
            {
                MessageBox.Show(string.Format(I18n.GetString("CreateConfigError"), error.Message),
                    I18n.GetString("Title"), MessageBoxButton.OK, MessageBoxImage.Error);
                Current.Shutdown(1);
                return;
            }


            Current.StartupUri = isGlobalProfile
                                     ? new Uri("GlobalConfig.xaml", UriKind.RelativeOrAbsolute)
                                     : new Uri("AppConfig.xaml", UriKind.RelativeOrAbsolute);
        }
    }
}
using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using LECommonLibrary;

namespace LEProc
{
    internal static class Program
    {
        internal static string[] Args;

        /// <summary>
        ///     The main entry point for the application.
        /// </summary>
        [STAThread]
        private static int Main(string[] args)
        {
            SystemHelper.DisableDPIScale();

            if (!GlobalHelper.CheckCoreDLLs())
            {
                MessageBox.Show(
                    string.Format(Localization.GetString("CoreFilesMissing"),
                        GlobalHelper.RuntimeDirectory),
                    string.Format(Localization.GetString("VersionTitle"), GlobalHelper.GetLEVersion()),
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);

                return 1;
            }

            if (args.Length == 0)
            {
                MessageBox.Show(
                    Localization.GetString("Usage"),
                    string.Format(Localization.GetString("VersionTitle"), GlobalHelper.GetLEVersion()));

                GlobalHelper.ShowErrorDebugMessageBox("SYSTEM_REPORT", 0);

                return 1;
            }

            try
            {
                Args = args;

                if (((Args[0] == "-run" || Args[0] == "-manage") && Args.Length < 2)
                    || (Args[0] == "-runas" && Args.Length < 3))
                    throw new ArgumentException(Localization.GetString("MissingArguments"));

                // Every launching entry point hands the target to the launcher matching its architecture.
                var target = Args[0] == "-run" ? Args[1]
                    : Args[0] == "-runas" ? Args[2]
                    : Args[0] == "-manage" || Args[0] == "-global" ? null
                    : Args[0];
                if (target != null && File.Exists(target))
                {
                    var launcher = GlobalHelper.GetLauncherPath(target);
                    if (launcher != null && !IsCurrentExecutable(launcher))
                        return ForwardToLauncher(launcher);
                }

                switch (Args[0])
                {
                    case "-run": //-run %APP%
                        return RunWithIndependentProfile(Args[1]);

                    case "-manage": //-manage %APP%
                        Process.Start(Path.Combine(GlobalHelper.ProductDirectory, "LEGUI.exe"),
                            WindowsCommandLine.QuoteArgument(Args[1] + ".le.config"));
                        return 0;

                    case "-global": //-global
                        Process.Start(Path.Combine(GlobalHelper.ProductDirectory, "LEGUI.exe"));
                        return 0;

                    case "-runas": //-runas %GUID% %APP%
                        return RunWithGlobalProfile(Args[1], Args[2]);

                    default:
                        if (!File.Exists(Args[0]))
                            throw new FileNotFoundException(
                                string.Format(Localization.GetString("InvalidCommand"), Args[0]), Args[0]);
                        return RunWithDefaultProfile(Args[0]);
                }
            }
            catch (Exception e)
            {
                MessageBox.Show(string.Format(Localization.GetString("StartupError"), e.Message),
                    Localization.GetString("Title"),
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                return 1;
            }
        }

        private static bool IsCurrentExecutable(string launcher)
        {
            return string.Equals(Path.GetFullPath(launcher),
                Path.GetFullPath(Assembly.GetExecutingAssembly().Location),
                StringComparison.OrdinalIgnoreCase);
        }

        // Static Explorer verbs always point at one launcher; hand the target to the
        // launcher whose architecture matches it (e.g. x86\LEProc.exe for 32-bit targets).
        private static int ForwardToLauncher(string launcher)
        {
            var startInfo = new ProcessStartInfo(launcher, WindowsCommandLine.JoinArguments(Args))
            {
                UseShellExecute = false
            };
            using (var process = Process.Start(startInfo))
            {
                process.WaitForExit();
                return process.ExitCode;
            }
        }

        private static int RunWithDefaultProfile(string path)
        {
            path = SystemHelper.EnsureAbsolutePath(path);

            var conf = path + ".le.config";

            var appProfile = LEConfig.GetProfiles(conf);
            var globalProfiles = LEConfig.GetProfiles();

            // app profile > first global profile > new profile(ja-JP)
            var profile = appProfile.Any()
                ? appProfile.First()
                : globalProfiles.Any() ? globalProfiles.First() : LEProfile.CreateDefault(LEProfile.DefaultLocation, false);

            return DoRunWithLEProfile(path, 1, profile);
        }

        private static int RunWithGlobalProfile(string guid, string path)
        {
            path = SystemHelper.EnsureAbsolutePath(path);

            var profiles = LEConfig.GetProfiles().Where(p => p.Guid == guid).ToArray();
            if (profiles.Length == 0)
                throw new InvalidDataException(
                    string.Format(Localization.GetString("GlobalProfileMissing"), guid));

            var profile = profiles[0];

            return DoRunWithLEProfile(path, 3, profile);
        }

        private static int RunWithIndependentProfile(string path)
        {
            path = SystemHelper.EnsureAbsolutePath(path);
            var conf = path + ".le.config";
            if (!File.Exists(conf))
            {
                Process.Start(
                    Path.Combine(GlobalHelper.ProductDirectory, "LEGUI.exe"),
                    WindowsCommandLine.QuoteArgument(path + ".le.config"));
                return 0;
            }

            var profiles = LEConfig.GetProfiles(conf);
            if (profiles.Length == 0)
                throw new InvalidDataException(
                    string.Format(Localization.GetString("ApplicationProfileInvalid"), conf));
            return DoRunWithLEProfile(path, 2, profiles[0]);
        }

        private static int DoRunWithLEProfile(string absPath, int argumentsStart, LEProfile profile)
        {
            try
            {
                if (!File.Exists(absPath))
                    throw new FileNotFoundException(
                        string.Format(Localization.GetString("MissingTargetFile"), absPath), absPath);

                if (profile.RunAsAdmin && !SystemHelper.IsAdministrator())
                {
                    return ElevateProcess() ? 0 : 1;
                }

                if (profile.RunWithSuspend)
                {
                    if (DialogResult.No ==
                        MessageBox.Show(
                            Localization.GetString("DebugModeWarning"),
                            Localization.GetString("Title"),
                            MessageBoxButtons.YesNo,
                            MessageBoxIcon.Information
                        ))
                        return 0;
                }

                var applicationName = string.Empty;
                var commandLine = string.Empty;
                var currentDirectory = Path.GetDirectoryName(absPath);

                if (string.Equals(Path.GetExtension(absPath), ".exe", StringComparison.OrdinalIgnoreCase))
                {
                    var plan = LaunchPlan.Create(absPath, profile.Parameter, Args.Skip(argumentsStart));
                    applicationName = plan.ApplicationName;
                    commandLine = plan.CommandLine;
                    currentDirectory = plan.CurrentDirectory;
                }
                else
                {
                    var jb = AssociationReader.GetAssociatedProgram(Path.GetExtension(absPath));

                    if (jb == null)
                        return 1;

                    var plan = LaunchPlan.CreateAssociated(absPath, jb[0], jb[1],
                        profile.Parameter, Args.Skip(argumentsStart));
                    applicationName = plan.ApplicationName;
                    commandLine = plan.CommandLine;
                    currentDirectory = plan.CurrentDirectory;
                }

                var culture = CultureInfo.GetCultureInfo(profile.Location);
                var textInfo = culture.TextInfo;
                var ansiCodePage = (uint) textInfo.ANSICodePage;
                var oemCodePage = (uint) textInfo.OEMCodePage;
                var localeID = (uint) textInfo.LCID;
                var defaultCharset = (uint) GetCharsetFromANSICodepage(textInfo.ANSICodePage);

                string standardName, daylightName;
                TimeZoneNames.Resolve(profile.Timezone, culture, out standardName, out daylightName);

                var registryMode = profile.RedirectRegistry
                    ? (profile.IsAdvancedRedirection ? 2u : 1u)
                    : 0u;


                var l = new LoaderWrapper(new RuntimeEnvironment
                {
                    AnsiCodePage = ansiCodePage,
                    OemCodePage = oemCodePage,
                    LocaleID = localeID,
                    FontCharset = defaultCharset,
                    RegistryMode = registryMode,
                    HookUILanguageMode = profile.IsAdvancedRedirection ? 2u : 0u,
                    TimezoneId = profile.Timezone,
                    StandardNameOverride = standardName,
                    DaylightNameOverride = daylightName
                })
                {
                    ApplicationName = applicationName,
                    CommandLine = commandLine,
                    CurrentDirectory = currentDirectory,
                    DebugMode = profile.RunWithSuspend
                };

                uint ret = l.Start();
                if (ret == 0)
                    return 0;

                if (SystemHelper.IsAdministrator())
                {
                    GlobalHelper.ShowErrorDebugMessageBox(commandLine, ret);
                    return unchecked((int)ret);
                }

                return ElevateProcess() ? 0 : 1;
            }
            catch (Exception e)
            {
                MessageBox.Show(string.Format(Localization.GetString("StartupError"), e.Message),
                    Localization.GetString("Title"),
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                return 1;
            }
        }

        private static bool ElevateProcess()
        {
            try
            {
                SystemHelper.RunWithElevatedProcess(
                    Assembly.GetExecutingAssembly().Location,
                    Args);
                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        // The system maps a code page to its GDI charset; unknown code pages keep ANSI_CHARSET.
        private static int GetCharsetFromANSICodepage(int ansicp)
        {
            const uint TCI_SRCCODEPAGE = 2;
            const int ANSI_CHARSET = 0;

            CharsetInfo info;
            return TranslateCharsetInfo(new IntPtr(ansicp), out info, TCI_SRCCODEPAGE)
                ? (int)info.Charset
                : ANSI_CHARSET;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct CharsetInfo
        {
            internal uint Charset;
            internal uint CodePage;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 6)]
            internal uint[] FontSignature;
        }

        // For TCI_SRCCODEPAGE the "pointer" argument carries the code page value itself.
        [DllImport("gdi32.dll")]
        private static extern bool TranslateCharsetInfo(IntPtr source, out CharsetInfo info, uint flags);
    }
}

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Xml.Linq;
using LECommonLibrary;

namespace ManagedRegression
{
    internal static class Program
    {
        [STAThread]
        private static int Main(string[] args)
        {
            if (args.Length > 0 && args[0] == "--child")
            {
                Console.WriteLine(Encode(Environment.CurrentDirectory));
                Console.WriteLine(Encode(File.ReadAllText("relative.txt")));
                foreach (var argument in args.Skip(1))
                    Console.WriteLine(Encode(argument));
                return 0;
            }

            return RunTests();
        }

        private static int RunTests()
        {
            var tests = new Action[]
            {
                RuntimeProductAssembliesAreAdjacent,
                WindowsArgumentsRoundTrip,
                QuoteArgumentRejectsUnrepresentableInput,
                ProfileParametersRemainRaw,
                TargetWorkingDirectoryAndArgumentsReachChild,
                MissingTargetIsRejected,
                ProfileXmlRoundTrip,
                OptionalProfileFieldsKeepDefaults,
                InvalidConfigKeepsEmptyResultContract,
                GlobalConfigCreationPreservesExistingFile,
                SaveFailureIsObservable,
                WpfRegression.AppConfigSaveFailureDoesNotLaunchOrShutdown,
                WpfRegression.GlobalSaveFailureKeepsSelectedProfile,
                WpfRegression.GlobalSaveAsFailureKeepsProfiles
            };
            var failures = 0;
            foreach (var test in tests)
            {
                try
                {
                    test();
                    Console.WriteLine("PASS " + test.Method.Name);
                }
                catch (Exception error)
                {
                    failures++;
                    Console.Error.WriteLine("FAIL " + test.Method.Name + ": " + error);
                }
            }
            Console.WriteLine("Tests: " + tests.Length + ", failures: " + failures);
            return failures == 0 ? 0 : 1;
        }

        private static void RuntimeProductAssembliesAreAdjacent()
        {
            var directory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            var products = new[] { typeof(LEConfig).Assembly, typeof(LEGUI.App).Assembly, LoadLauncherAssembly() };
            foreach (var product in products)
                Check(!product.GlobalAssemblyCache && string.Equals(Path.GetDirectoryName(product.Location),
                    directory, StringComparison.OrdinalIgnoreCase),
                    "Runtime product assembly resolved outside the harness directory: "
                    + product.FullName + " at " + product.Location);
        }

        private static Assembly LoadLauncherAssembly()
        {
            return Assembly.LoadFrom(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location),
                "LEProc.exe"));
        }

        private static void WindowsArgumentsRoundTrip()
        {
            var cases = new List<string>
            {
                "", "plain", "two words", "\t", "line\nbreak", "日本語 中文", "a\"b",
                "\\", "C:\\folder with space\\", "\\\\server\\share\\", "\"\"", "ends\\\\"
            };
            for (var count = 0; count <= 8; count++)
            {
                cases.Add("before" + new string('\\', count) + "\"after");
                cases.Add("with space " + new string('\\', count));
            }
            foreach (var value in cases)
                EqualArguments(new[] { value }, Parse("probe.exe " + WindowsCommandLine.QuoteArgument(value)));
            EqualArguments(cases, Parse("probe.exe " + WindowsCommandLine.JoinArguments(cases)));
            EqualArguments(new string[0], Parse("probe.exe " + WindowsCommandLine.JoinArguments(new string[0])));
        }

        private static void QuoteArgumentRejectsUnrepresentableInput()
        {
            Throws<ArgumentNullException>(() => WindowsCommandLine.QuoteArgument(null));
            Throws<ArgumentException>(() => WindowsCommandLine.QuoteArgument("before\0after"));
            Throws<ArgumentNullException>(() => WindowsCommandLine.JoinArguments(null));
        }

        private static void ProfileParametersRemainRaw()
        {
            var executable = Assembly.GetExecutingAssembly().Location;
            const string raw = "--mode \"two words\" \"\" C:\\plain\\";
            var plan = CreateLaunchPlan(executable, raw, new[] { "ignored" });
            Check(plan.CommandLine == WindowsCommandLine.QuoteArgument(executable) + " " + raw,
                "Raw profile command line was modified.");
            EqualArguments(new[] { "--mode", "two words", "", "C:\\plain\\" }, Parse(plan.CommandLine));
            EqualArguments(new[] { "two words", "", "a\"b", "trailing space \\" },
                Parse(CreateLaunchPlan(executable, null,
                    new[] { "two words", "", "a\"b", "trailing space \\" }).CommandLine));
        }

        private static void TargetWorkingDirectoryAndArgumentsReachChild()
        {
            InTemporaryDirectory(directory =>
            {
                var targetDirectory = Path.Combine(directory, "target with spaces");
                Directory.CreateDirectory(targetDirectory);
                var target = Path.Combine(targetDirectory, "Child probe.exe");
                File.Copy(Assembly.GetExecutingAssembly().Location, target);
                File.Copy(typeof(LEConfig).Assembly.Location,
                    Path.Combine(targetDirectory, "LECommonLibrary.dll"));
                File.WriteAllText(Path.Combine(targetDirectory, "relative.txt"), "target-relative-content");
                var expected = new[] { "two words", "a\"b", "", "C:\\folder with space\\", "日本語" };
                var plan = CreateLaunchPlan(target, "", new[] { "--child" }.Concat(expected));
                var start = new ProcessStartInfo
                {
                    FileName = plan.ApplicationName,
                    Arguments = plan.CommandLine.Substring(WindowsCommandLine.QuoteArgument(plan.ApplicationName).Length + 1),
                    WorkingDirectory = plan.CurrentDirectory,
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true
                };
                using (var child = Process.Start(start))
                {
                    if (!child.WaitForExit(10000))
                    {
                        child.Kill();
                        child.WaitForExit();
                        throw new Exception("Child process timed out.");
                    }
                    var output = child.StandardOutput.ReadToEnd();
                    var error = child.StandardError.ReadToEnd();
                    Check(child.ExitCode == 0, "Child process failed: " + error);
                    var actual = output.Split(new[] { "\r\n", "\n" }, StringSplitOptions.None).ToList();
                    actual.RemoveAt(actual.Count - 1);
                    var decoded = actual.Select(line => Encoding.Unicode.GetString(Convert.FromBase64String(line))).ToArray();
                    Check(string.Equals(decoded[0], targetDirectory, StringComparison.OrdinalIgnoreCase),
                        "Child working directory is not the target directory.");
                    Check(decoded[1] == "target-relative-content", "Child could not read a target-relative file.");
                    EqualArguments(expected, decoded.Skip(2));
                }
            });
        }

        private static void MissingTargetIsRejected()
        {
            InTemporaryDirectory(directory =>
                Throws<FileNotFoundException>(() => CreateLaunchPlan(Path.Combine(directory, "missing.exe"), "", new string[0])));
            Throws<ArgumentException>(() => CreateLaunchPlan(" ", "", new string[0]));
        }

        private static void ProfileXmlRoundTrip()
        {
            InTemporaryDirectory(directory =>
            {
                var path = Path.Combine(directory, "application.le.config");
                var profile = new LEProfile("日本語 & <profile> \"name\"", Guid.NewGuid().ToString(), true,
                    "--title \"A & B <C>\" \"\" C:\\folder with space\\", "ja-JP", "Tokyo Standard Time",
                    true, false, true, true);
                LEConfig.SaveApplicationConfigFile(path, profile);
                var loaded = LEConfig.GetProfiles(path);
                Check(loaded.Length == 1 && loaded[0].Equals(profile), "Application profile fields did not round-trip.");
                var originalPath = LEConfig.GlobalConfigPath;
                try
                {
                    LEConfig.GlobalConfigPath = Path.Combine(directory, "LEConfig.xml");
                    var second = new LEProfile(true);
                    LEConfig.SaveGlobalConfigFile(profile, second);
                    var profiles = LEConfig.GetProfiles();
                    Check(profiles.Length == 2 && profiles[0].Equals(profile) && profiles[1].Equals(second),
                        "Global profile order or fields did not round-trip.");
                    Check(LEConfig.GetProfile(profile.Name).Equals(profile), "Profile lookup returned different fields.");
                }
                finally { LEConfig.GlobalConfigPath = originalPath; }
            });
        }

        private static void OptionalProfileFieldsKeepDefaults()
        {
            InTemporaryDirectory(directory =>
            {
                var path = Path.Combine(directory, "legacy.xml");
                LEConfig.SaveApplicationConfigFile(path, new LEProfile(true));
                var document = XDocument.Load(path);
                foreach (var name in new[] { "RunAsAdmin", "RedirectRegistry", "IsAdvancedRedirection", "RunWithSuspend" })
                    document.Descendants(name).Remove();
                document.Save(path);
                var loaded = LEConfig.GetProfiles(path);
                Check(loaded.Length == 1 && !loaded[0].RunAsAdmin && loaded[0].RedirectRegistry
                    && !loaded[0].IsAdvancedRedirection && !loaded[0].RunWithSuspend,
                    "Missing optional profile fields lost their existing defaults.");
            });
        }

        private static void InvalidConfigKeepsEmptyResultContract()
        {
            InTemporaryDirectory(directory =>
            {
                var path = Path.Combine(directory, "invalid.xml");
                Check(LEConfig.GetProfiles(path).Length == 0, "Missing config should return no profiles.");
                foreach (var xml in new[] { "<LEConfig>", "<LEConfig><Profiles /></LEConfig>",
                    "<LEConfig><Profiles><Profile Name='broken' /></Profiles></LEConfig>" })
                {
                    File.WriteAllText(path, xml);
                    Check(LEConfig.GetProfiles(path).Length == 0, "Unreadable or empty config should return no profiles.");
                    Check(File.ReadAllText(path) == xml, "Reading invalid config modified it.");
                }
            });
        }

        private static void GlobalConfigCreationPreservesExistingFile()
        {
            InTemporaryDirectory(directory =>
            {
                var originalPath = LEConfig.GlobalConfigPath;
                try
                {
                    LEConfig.GlobalConfigPath = Path.Combine(directory, "LEConfig.xml");
                    Check(!LEConfig.CheckGlobalConfigFile(false), "Read-only check created a config.");
                    Check(LEConfig.CheckGlobalConfigFile(true), "Default config was not created.");
                    var defaults = LEConfig.GetProfiles();
                    Check(defaults.Length == 2 && !defaults[0].RunAsAdmin && defaults[1].RunAsAdmin
                        && defaults.All(p => p.Location == "ja-JP" && p.Timezone == "Tokyo Standard Time" && p.RedirectRegistry)
                        && defaults[0].Guid != defaults[1].Guid, "Default profiles changed.");
                    File.WriteAllText(LEConfig.GlobalConfigPath, "<invalid>");
                    Check(LEConfig.CheckGlobalConfigFile(true), "Existing config was not detected.");
                    Check(File.ReadAllText(LEConfig.GlobalConfigPath) == "<invalid>", "Existing config was overwritten.");
                }
                finally { LEConfig.GlobalConfigPath = originalPath; }
            });
        }

        private static void SaveFailureIsObservable()
        {
            InTemporaryDirectory(directory =>
            {
                var path = Path.Combine(directory, "locked.xml");
                File.WriteAllText(path, "preserve-me");
                using (File.Open(path, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
                    Throws<IOException>(() => LEConfig.SaveApplicationConfigFile(path, new LEProfile(true)));
                Check(File.ReadAllText(path) == "preserve-me", "Failed save modified the existing config.");
            });
        }

        private sealed class LaunchPlanResult
        {
            internal readonly string ApplicationName;
            internal readonly string CommandLine;
            internal readonly string CurrentDirectory;

            internal LaunchPlanResult(string applicationName, string commandLine, string currentDirectory)
            {
                ApplicationName = applicationName;
                CommandLine = commandLine;
                CurrentDirectory = currentDirectory;
            }
        }

        private static LaunchPlanResult CreateLaunchPlan(string path, string rawParameters, IEnumerable<string> arguments)
        {
            var product = LoadLauncherAssembly();
            var type = product.GetType("LEProc.LaunchPlan", true);
            object plan;
            try
            {
                plan = type.GetMethod("Create", BindingFlags.Static | BindingFlags.NonPublic)
                    .Invoke(null, new object[] { path, rawParameters, arguments });
            }
            catch (TargetInvocationException error)
            {
                if (error.InnerException == null) throw;
                throw error.InnerException;
            }

            var properties = BindingFlags.Instance | BindingFlags.NonPublic;
            return new LaunchPlanResult(
                (string)type.GetProperty("ApplicationName", properties).GetValue(plan, null),
                (string)type.GetProperty("CommandLine", properties).GetValue(plan, null),
                (string)type.GetProperty("CurrentDirectory", properties).GetValue(plan, null));
        }

        private static void InTemporaryDirectory(Action<string> test)
        {
            var directory = Path.Combine(Path.GetTempPath(), "LE managed regression " + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            try { test(directory); }
            finally { Directory.Delete(directory, true); }
        }

        private static string Encode(string value)
        {
            return Convert.ToBase64String(Encoding.Unicode.GetBytes(value));
        }

        private static void EqualArguments(IEnumerable<string> expected, IEnumerable<string> actual)
        {
            Check(expected.SequenceEqual(actual), "Argument values or boundaries changed.");
        }

        private static void Check(bool condition, string message)
        {
            if (!condition) throw new Exception(message);
        }

        private static void Throws<T>(Action action) where T : Exception
        {
            try { action(); }
            catch (T) { return; }
            throw new Exception("Expected " + typeof(T).Name + ".");
        }

        private static string[] Parse(string commandLine)
        {
            int count;
            var memory = CommandLineToArgvW(commandLine, out count);
            if (memory == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
            try
            {
                var arguments = new string[count - 1];
                for (var i = 1; i < count; i++)
                    arguments[i - 1] = Marshal.PtrToStringUni(Marshal.ReadIntPtr(memory, i * IntPtr.Size));
                return arguments;
            }
            finally { LocalFree(memory); }
        }

        [DllImport("shell32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CommandLineToArgvW(string commandLine, out int count);

        [DllImport("kernel32.dll")]
        private static extern IntPtr LocalFree(IntPtr memory);
    }
}

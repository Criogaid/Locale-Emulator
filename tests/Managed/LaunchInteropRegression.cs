using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace ManagedRegression
{
    internal static class LaunchInteropRegression
    {
        private const BindingFlags Instance = BindingFlags.Instance | BindingFlags.NonPublic;
        private const string ChildMode = "--launch-interop-child";
        private const string ChildSuccessPrefix = "PASS launch interop ";
        private const string CommandLine = "mutable command 日本語";
        private const uint StubFailureStatus = 0xC0000001;
        private const int HandleIterations = 200;
        private const int AllocationIterations = 512;
        private const int ChildTimeoutMilliseconds = 30000;
        private const int ShutdownTimeoutMilliseconds = 5000;

        internal static void LoaderSdkBoundaryAndHandleCleanup() { RunIsolated("sdk"); }
        internal static void LoaderMissingDllReleasesAllocation() { RunIsolated("missing-dll"); }

        internal static bool TryRunChild(string[] args, out int exitCode)
        {
            exitCode = 0;
            if (args.Length == 0 || args[0] != ChildMode) return false;
            try
            {
                Check(args.Length == 2, "Expected one launch interop child scenario.");
                Check(IntPtr.Size == 4, "Launch interop checks require the Win32 managed host.");
                Check(GetModuleHandle("LoaderDll.dll") == IntPtr.Zero, "LoaderDll was already loaded before the isolated check.");
                var directory = AppDomain.CurrentDomain.BaseDirectory;
                var product = Assembly.LoadFrom(Path.Combine(directory, "LEProc.exe"));
                var common = Assembly.LoadFrom(Path.Combine(directory, "LECommonLibrary.dll"));
                foreach (var assembly in new[] { product, common })
                    Check(!assembly.GlobalAssemblyCache && string.Equals(Path.GetDirectoryName(assembly.Location),
                        directory.TrimEnd(Path.DirectorySeparatorChar), StringComparison.OrdinalIgnoreCase),
                        "Product assembly resolved outside the isolated directory: " + assembly.Location);
                var type = product.GetType("LEProc.LoaderWrapper", true);
                if (args[1] == "sdk") CheckSdkBoundary(type);
                else if (args[1] == "missing-dll") CheckMissingDllAllocation(type);
                else throw new ArgumentException("Unknown launch interop child scenario: " + args[1]);
                Console.WriteLine(ChildSuccessPrefix + args[1]);
            }
            catch (Exception error)
            {
                Console.Error.WriteLine(error);
                exitCode = 1;
            }
            return true;
        }

        private static void RunIsolated(string scenario)
        {
            var source = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            var directory = Path.Combine(Path.GetTempPath(), "LE launch interop " + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            try
            {
                var host = Path.GetFileName(Assembly.GetExecutingAssembly().Location);
                foreach (var file in new[] { host, host + ".config", "LEProc.exe", "LEProc.exe.config", "LECommonLibrary.dll" })
                    File.Copy(Path.Combine(source, file), Path.Combine(directory, file));
                if (scenario == "sdk")
                    File.Copy(Path.Combine(source, "LoaderInterop", "LoaderDll.dll"), Path.Combine(directory, "LoaderDll.dll"));
                var start = new ProcessStartInfo(Path.Combine(directory, host), ChildMode + " " + scenario)
                {
                    WorkingDirectory = directory, UseShellExecute = false, CreateNoWindow = true,
                    RedirectStandardOutput = true, RedirectStandardError = true
                };
                // 缺 DLL 场景不能通过工作目录或继承的 PATH 找到真实产品 DLL。
                start.EnvironmentVariables["PATH"] = Environment.GetFolderPath(Environment.SpecialFolder.System);
                using (var child = Process.Start(start))
                {
                    var output = child.StandardOutput.ReadToEndAsync();
                    var error = child.StandardError.ReadToEndAsync();
                    if (!child.WaitForExit(ChildTimeoutMilliseconds))
                    {
                        child.Kill();
                        Check(child.WaitForExit(ShutdownTimeoutMilliseconds), "Launch interop child did not terminate after Kill.");
                        throw new Exception("Launch interop child timed out: " + scenario);
                    }
                    Check(Task.WaitAll(new Task[] { output, error }, ShutdownTimeoutMilliseconds), "Launch interop output did not close.");
                    Console.Write(output.Result);
                    Check(child.ExitCode == 0, "Launch interop child failed (" + scenario + "): " + error.Result);
                    Check(output.Result.EndsWith(ChildSuccessPrefix + scenario + Environment.NewLine, StringComparison.Ordinal),
                        "Launch interop child handler did not complete: " + scenario);
                }
            }
            finally { Directory.Delete(directory, true); }
        }

        private static void CheckSdkBoundary(Type type)
        {
            var environmentType = type.Assembly.GetType("LEProc.RuntimeEnvironment", true);
            var environment = Activator.CreateInstance(environmentType, true);
            var wrapper = CreateWrapper(type, environment);
            Check(ReferenceEquals(type.GetProperty("Environment", Instance).GetValue(wrapper, null), environment),
                "LoaderWrapper did not retain the injected runtime environment.");
            try
            {
                CreateWrapper(type, null);
                throw new Exception("Expected ArgumentNullException for a null runtime environment.");
            }
            catch (ArgumentNullException error)
            {
                Check(error.ParamName == "environment", "Null environment rejection identified the wrong parameter.");
            }
            environmentType.GetProperty("AnsiCodePage", Instance).SetValue(environment, (uint) 932, null);
            environmentType.GetProperty("OemCodePage", Instance).SetValue(environment, (uint) 936, null);
            environmentType.GetProperty("LocaleID", Instance).SetValue(environment, (uint) 0x411, null);
            environmentType.GetProperty("FontCharset", Instance).SetValue(environment, (uint) 128, null);
            environmentType.GetProperty("RegistryMode", Instance).SetValue(environment, (uint) 2, null);
            environmentType.GetProperty("HookUILanguageMode", Instance).SetValue(environment, (uint) 2, null);
            type.GetProperty("CurrentDirectory", Instance).SetValue(wrapper, "working directory 中文", null);
            var start = type.GetMethod("Start", Instance);
            type.GetProperty("ApplicationName", Instance).SetValue(wrapper, "missing-timezone", null);
            foreach (var timezone in new string[] { null, "" })
            {
                environmentType.GetProperty("TimezoneId", Instance).SetValue(environment, timezone, null);
                try { Invoke(start, wrapper); }
                catch (ArgumentException) { continue; }
                throw new Exception("Expected an exception for a missing time zone.");
            }
            environmentType.GetProperty("TimezoneId", Instance).SetValue(environment, "Tokyo Standard Time", null);
            using (var process = Process.GetCurrentProcess())
            {
                var processHandle = process.Handle;
                foreach (var scenario in new[] { "success", "failure", "waitfailure" })
                {
                    type.GetProperty("ApplicationName", Instance).SetValue(wrapper, scenario + "-日本語", null);
                    Action call = () =>
                    {
                        // 不把驻留字符串交给旧版只读字符串 P/Invoke 声明，避免污染测试常量。
                        type.GetProperty("CommandLine", Instance).SetValue(wrapper, new string(CommandLine.ToCharArray()), null);
                        try
                        {
                            var result = (uint)Invoke(start, wrapper);
                            Check(scenario != "waitfailure", "A wait without SYNCHRONIZE unexpectedly succeeded.");
                            Check(result == (scenario == "failure" ? StubFailureStatus : 0),
                                "Unexpected stub status for " + scenario + ": " + result.ToString("X8"));
                        }
                        catch (Win32Exception error)
                        {
                            if (scenario != "waitfailure" || error.NativeErrorCode != 5) throw;
                        }
                        Check((string)type.GetProperty("CommandLine", Instance).GetValue(wrapper, null) == CommandLine,
                            "Native command line writes modified the managed input string.");
                    };
                    call();
                    var before = HandleCount(processHandle);
                    for (var iteration = 0; iteration < HandleIterations; iteration++) call();
                    var after = HandleCount(processHandle);
                    Console.WriteLine("SDK " + scenario + " x" + HandleIterations + " handles: " + before + " -> " + after);
                    Check(after == before, "Native handles leaked in " + scenario + ".");
                }
            }
        }

        private static void CheckMissingDllAllocation(Type type)
        {
            Check(!File.Exists(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "LoaderDll.dll")),
                "Missing DLL check unexpectedly contains LoaderDll.dll.");
            var environmentType = type.Assembly.GetType("LEProc.RuntimeEnvironment", true);
            var environment = Activator.CreateInstance(environmentType, true);
            environmentType.GetProperty("RegistryMode", Instance).SetValue(environment, (uint) 1, null);
            environmentType.GetProperty("TimezoneId", Instance).SetValue(environment, "Tokyo Standard Time", null);
            const int nameOverrideCharacters = 16384;
            var nameOverride = new string('x', nameOverrideCharacters);
            environmentType.GetProperty("StandardNameOverride", Instance).SetValue(environment, nameOverride, null);
            environmentType.GetProperty("DaylightNameOverride", Instance).SetValue(environment, nameOverride, null);
            var environmentSize = ((byte[])Invoke(environmentType.GetMethod("Serialize", Instance), environment)).Length;
            // 未释放的环境块必须超过容忍阈值，避免短时区字符串让回归失去检出能力。
            const long maxRetainedBytes = 16L * 1024 * 1024;
            Check((long)environmentSize * AllocationIterations > 2 * maxRetainedBytes,
                "Allocation stress must exceed twice the retained-memory threshold if cleanup is missing.");
            var wrapper = CreateWrapper(type, environment);
            type.GetProperty("ApplicationName", Instance).SetValue(wrapper, "missing-native-boundary", null);
            var start = type.GetMethod("Start", Instance);
            Action call = () =>
            {
                try { Invoke(start, wrapper); }
                catch (DllNotFoundException) { return; }
                throw new Exception("Expected DllNotFoundException from the isolated product.");
            };
            const int warmupIterations = 3;
            for (var iteration = 0; iteration < warmupIterations; iteration++) call();
            var before = PrivateBytes();
            for (var iteration = 0; iteration < AllocationIterations; iteration++) call();
            var retained = PrivateBytes() - before;
            // 16 MiB 留给 CLR 堆提交和异常处理波动。
            Console.WriteLine("Missing DLL x" + AllocationIterations + " private byte delta: " + retained);
            Check(retained < maxRetainedBytes, "Unmanaged launch allocations were retained after missing DLL errors.");
        }

        private static object CreateWrapper(Type type, object environment)
        {
            try { return Activator.CreateInstance(type, Instance, null, new[] { environment }, null); }
            catch (TargetInvocationException error) { throw error.InnerException ?? error; }
        }

        private static object Invoke(MethodInfo method, object target, params object[] arguments)
        {
            try { return method.Invoke(target, arguments); }
            catch (TargetInvocationException error) { throw error.InnerException ?? error; }
        }

        private static uint HandleCount(IntPtr process)
        {
            uint count;
            if (!GetProcessHandleCount(process, out count)) throw new Win32Exception(Marshal.GetLastWin32Error());
            return count;
        }

        private static long PrivateBytes()
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
            using (var process = Process.GetCurrentProcess()) return process.PrivateMemorySize64;
        }

        private static void Check(bool condition, string message) { if (!condition) throw new Exception(message); }
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetProcessHandleCount(IntPtr process, out uint count);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        private static extern IntPtr GetModuleHandle(string name);
    }
}

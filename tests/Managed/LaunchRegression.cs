using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using LECommonLibrary;

namespace ManagedRegression
{
    internal static class LaunchRegression
    {
        private const BindingFlags Instance = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic;
        private const BindingFlags Static = BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;

        internal static void ElevationRequestPreservesRelativePathsAndArguments()
        {
            InDirectory(directory =>
            {
                var original = Environment.CurrentDirectory;
                try
                {
                    Environment.CurrentDirectory = directory;
                    File.WriteAllText(Path.Combine(directory, "relative.txt"), "caller-content");
                    foreach (var prefix in new[] { new string[0], new[] { "-run" }, new[] { "-runas", "profile-guid" }, new[] { "-manage" } })
                    {
                        var args = prefix.Concat(new[] { @"sub folder\target.exe", "", "quoted\"value", @"trailing space\" }).ToArray();
                        var request = ElevationRequest(args);
                        Check((string)Field(request, "lpVerb") == "runas", "Elevation verb changed.");
                        Check((string)Field(request, "lpDirectory") == directory, "Elevation changed the caller working directory.");
                        Check(Parse("launcher " + Field(request, "lpParameters")).SequenceEqual(args), "Elevation changed argument boundaries.");
                        var parsed = Parse("launcher " + Field(request, "lpParameters"));
                        Check(Path.GetFullPath(Path.Combine((string)Field(request, "lpDirectory"), parsed[prefix.Length]))
                            == Path.Combine(directory, @"sub folder\target.exe"), "Relative target resolved against the installation directory.");
                    }
                    var childArgs = new[] { "--child", "", "two words", "a\"b", @"C:\trailing space\" };
                    var childRequest = ElevationRequest(childArgs);
                    var output = RunChild((string)Field(childRequest, "lpFile"),
                        (string)Field(childRequest, "lpParameters"), (string)Field(childRequest, "lpDirectory"));
                    Check(output[0] == directory && output[1] == "caller-content", "Child lost the caller's relative file contract.");
                    Check(output.Skip(2).SequenceEqual(childArgs.Skip(1)), "Child lost elevation request arguments.");
                }
                finally { Environment.CurrentDirectory = original; }
            });
        }

        internal static void AssociationArgumentsReachChild()
        {
            InDirectory(directory =>
            {
                var document = Path.Combine(directory, "document with spaces.test");
                File.WriteAllText(document, "document");
                File.WriteAllText(Path.Combine(directory, "relative.txt"), "target-content");
                var cli = new[] { "", "two words", "embedded\"quote", @"C:\trailing space\", "日本語" };
                foreach (var template in new[] { "--child \"%1\" %*", "--child \"%L\" %*", "--child %1 %*" })
                foreach (var raw in new[] { "", "\"profile value\" \"\"" })
                {
                    var plan = Invoke(ProductType("LEProc.LaunchPlan").GetMethod("CreateAssociated", Static), null,
                        document, "\"" + Assembly.GetExecutingAssembly().Location + "\"", template, raw, cli);
                    var output = RunChild((string)Property(plan, "ApplicationName"),
                        RemoveExecutable((string)Property(plan, "CommandLine")), (string)Property(plan, "CurrentDirectory"));
                    Check(output[0] == directory && output[1] == "target-content", "Associated child working directory changed.");
                    var expected = new[] { document }.Concat(raw.Length == 0 ? cli : new[] { "profile value", "" });
                    Check(output.Skip(2).SequenceEqual(expected), "Association discarded CLI arguments or changed profile precedence.");
                    var exePlan = Invoke(ProductType("LEProc.LaunchPlan").GetMethod("Create", Static), null,
                        Assembly.GetExecutingAssembly().Location, raw, cli);
                    Check(Parse((string)Property(exePlan, "CommandLine")).SequenceEqual(output.Skip(3)),
                        "EXE and association parameter selection differ.");
                }
            });
        }

        internal static void SerializedLanguageRulesUseWindowsLanguageKey()
        {
            var type = ProductType("LEProc.RuntimeEnvironment");
            var environment = Activator.CreateInstance(type, true);
            type.GetProperty("AnsiCodePage", Instance).SetValue(environment, (uint) 932, null);
            type.GetProperty("OemCodePage", Instance).SetValue(environment, (uint) 936, null);
            type.GetProperty("LocaleID", Instance).SetValue(environment, (uint) 0x411, null);
            type.GetProperty("FontCharset", Instance).SetValue(environment, (uint) 128, null);
            type.GetProperty("RegistryMode", Instance).SetValue(environment, (uint) 2, null);
            type.GetProperty("HookUILanguageMode", Instance).SetValue(environment, (uint) 2, null);
            type.GetProperty("TimezoneId", Instance).SetValue(environment, "Tokyo Standard Time", null);
            var bytes = (byte[])Invoke(type.GetMethod("Serialize", Instance), environment);
            Check(BitConverter.ToUInt32(bytes, 0) == bytes.Length, "Runtime environment size is not self describing.");
            Check(BitConverter.ToUInt32(bytes, 4) == 1, "Runtime environment version changed.");
            Check(BitConverter.ToUInt32(bytes, 8) == 932 && BitConverter.ToUInt32(bytes, 12) == 936,
                "Runtime environment code pages changed.");
            Check(BitConverter.ToUInt32(bytes, 16) == 0x411 && BitConverter.ToUInt32(bytes, 20) == 128,
                "Runtime environment locale fields changed.");
            Check(BitConverter.ToUInt32(bytes, 24) == 2 && BitConverter.ToUInt32(bytes, 28) == 2,
                "Registry and UI language modes were not serialized.");
            var timezoneOffset = checked((int) BitConverter.ToUInt32(bytes, 32));
            var timezoneLength = checked((int) BitConverter.ToUInt32(bytes, 36));
            Check(Encoding.Unicode.GetString(bytes, timezoneOffset, timezoneLength) == "Tokyo Standard Time",
                "Runtime environment timezone ID changed.");
            Check(BitConverter.ToUInt32(bytes, 56) == 0 && BitConverter.ToUInt32(bytes, 60) == 0
                && BitConverter.ToUInt32(bytes, 64) == 0,
                "Unused registry entry fields were not zeroed.");
        }

        internal static void TimezoneNamesAndStandardTimeContract()
        {
            var type = ProductType("LEProc.RuntimeEnvironment");
            foreach (var installedZone in TimeZoneInfo.GetSystemTimeZones())
            {
                var environment = Activator.CreateInstance(type, true);
                type.GetProperty("TimezoneId", Instance).SetValue(environment, installedZone.Id, null);
                var bytes = (byte[])Invoke(type.GetMethod("Serialize", Instance), environment);
                var offset = checked((int) BitConverter.ToUInt32(bytes, 32));
                var length = checked((int) BitConverter.ToUInt32(bytes, 36));
                Check(Encoding.Unicode.GetString(bytes, offset, length) == installedZone.Id,
                    "Timezone ID was changed before reaching the native boundary: " + installedZone.Id);
                Check(bytes[offset + length] == 0 && bytes[offset + length + 1] == 0,
                    "Timezone ID is not terminated in the runtime environment block.");
            }
        }

        internal static void LongValidTimezoneIdIsAccepted()
        {
            const string id = "LE regression valid timezone ID longer than thirty two characters";
            var type = ProductType("LEProc.RuntimeEnvironment");
            var environment = Activator.CreateInstance(type, true);
            type.GetProperty("TimezoneId", Instance).SetValue(environment, id, null);
            var bytes = (byte[])Invoke(type.GetMethod("Serialize", Instance), environment);
            var offset = checked((int) BitConverter.ToUInt32(bytes, 32));
            var length = checked((int) BitConverter.ToUInt32(bytes, 36));
            Check(Encoding.Unicode.GetString(bytes, offset, length) == id,
                "A valid long timezone ID was truncated by the managed ABI serializer.");
        }

        internal static void TimezoneNamesAreBoundedAndTerminated()
        {
            var type = ProductType("LEProc.RuntimeEnvironment");
            foreach (var name in new[] { "", new string('x', 31), new string('x', 32768 / 2) })
            {
                var environment = Activator.CreateInstance(type, true);
                type.GetProperty("TimezoneId", Instance).SetValue(environment, "Tokyo Standard Time", null);
                type.GetProperty("StandardNameOverride", Instance).SetValue(environment, name, null);
                type.GetProperty("DaylightNameOverride", Instance).SetValue(environment, name, null);
                var bytes = (byte[])Invoke(type.GetMethod("Serialize", Instance), environment);
                var standardOffset = checked((int) BitConverter.ToUInt32(bytes, 40));
                var standardLength = checked((int) BitConverter.ToUInt32(bytes, 44));
                var daylightOffset = checked((int) BitConverter.ToUInt32(bytes, 48));
                var daylightLength = checked((int) BitConverter.ToUInt32(bytes, 52));
                Check(Encoding.Unicode.GetString(bytes, standardOffset, standardLength) == name
                    && Encoding.Unicode.GetString(bytes, daylightOffset, daylightLength) == name,
                    "Timezone name override was changed during serialization.");
                Check(bytes[standardOffset + standardLength] == 0 && bytes[standardOffset + standardLength + 1] == 0
                    && bytes[daylightOffset + daylightLength] == 0 && bytes[daylightOffset + daylightLength + 1] == 0,
                    "Timezone name override is not terminated.");
            }
            var invalid = Activator.CreateInstance(type, true);
            type.GetProperty("TimezoneId", Instance).SetValue(invalid, "Tokyo Standard Time", null);
            type.GetProperty("StandardNameOverride", Instance).SetValue(invalid, new string('x', 16385), null);
            Throws<ArgumentException>(() => Invoke(type.GetMethod("Serialize", Instance), invalid));
            type.GetProperty("StandardNameOverride", Instance).SetValue(invalid, "bad\0name", null);
            Throws<ArgumentException>(() => Invoke(type.GetMethod("Serialize", Instance), invalid));
        }

        private static object ElevationRequest(string[] args)
        {
            return Invoke(typeof(SystemHelper).GetMethod("CreateElevationRequest", Static), null,
                Assembly.GetExecutingAssembly().Location, args);
        }

        [DllImport("advapi32.dll")]
        private static extern int RegOverridePredefKey(IntPtr key, IntPtr replacement);

        private static Type ProductType(string name)
        {
            return Assembly.LoadFrom(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "LEProc.exe")).GetType(name, true);
        }

        private static object Invoke(MethodInfo method, object target, params object[] args)
        {
            Check(method != null, "Product boundary method is missing.");
            try { return method.Invoke(target, args); }
            catch (TargetInvocationException error) { throw error.InnerException ?? error; }
        }

        private static object Field(object value, string name) { return value.GetType().GetField(name, Instance).GetValue(value); }
        private static object Property(object value, string name) { return value.GetType().GetProperty(name, Instance).GetValue(value, null); }
        private static string Name(object timezone, string field) { return Encoding.Unicode.GetString((byte[])Field(timezone, field)).TrimEnd('\0'); }
        private static string BoundedName(string name)
        {
            var length = Math.Min(31, name.Length);
            if (length > 0 && char.IsHighSurrogate(name[length - 1])) length--;
            return name.Substring(0, length);
        }

        private static string ReadUnicodeString(byte[] bytes, object value, int lebSize)
        {
            return Encoding.Unicode.GetString(bytes, checked((int)(long)Field(value, "Buffer") - lebSize), (ushort)Field(value, "Length"));
        }

        private static string RemoveExecutable(string commandLine)
        {
            Check(commandLine[0] == '"', "Expected a quoted executable path.");
            return commandLine.Substring(commandLine.IndexOf('"', 1) + 1).TrimStart();
        }

        private static string[] RunChild(string executable, string arguments, string directory)
        {
            using (var child = Process.Start(new ProcessStartInfo(executable, arguments)
            {
                UseShellExecute = false, WorkingDirectory = directory, RedirectStandardOutput = true, RedirectStandardError = true
            }))
            {
                if (!child.WaitForExit(10000))
                {
                    child.Kill();
                    child.WaitForExit();
                    throw new Exception("Launch regression child timed out.");
                }
                var output = child.StandardOutput.ReadToEnd();
                Check(child.ExitCode == 0, "Child failed: " + child.StandardError.ReadToEnd());
                var lines = output.Split(new[] { "\r\n", "\n" }, StringSplitOptions.None);
                return lines.Take(lines.Length - 1)
                    .Select(line => Encoding.Unicode.GetString(Convert.FromBase64String(line))).ToArray();
            }
        }

        private static void InDirectory(Action<string> test)
        {
            var directory = Path.Combine(Path.GetTempPath(), "LE launch regression " + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            try { test(directory); }
            finally { Directory.Delete(directory, true); }
        }

        private static string[] Parse(string commandLine)
        {
            int count;
            var memory = CommandLineToArgvW(commandLine, out count);
            if (memory == IntPtr.Zero) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
            try
            {
                return Enumerable.Range(1, count - 1).Select(index => Marshal.PtrToStringUni(Marshal.ReadIntPtr(memory, index * IntPtr.Size))).ToArray();
            }
            finally { LocalFree(memory); }
        }

        private static void Throws<T>(Action action) where T : Exception
        {
            try
            {
                action();
            }
            catch (T)
            {
                return;
            }
            throw new Exception("Expected " + typeof(T).Name + ".");
        }

        private static void Check(bool condition, string message) { if (!condition) throw new Exception(message); }
        [DllImport("shell32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CommandLineToArgvW(string commandLine, out int count);
        [DllImport("kernel32.dll")]
        private static extern IntPtr LocalFree(IntPtr memory);
    }
}

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
                foreach (var raw in new[] { "", "\"profile value\" \"\"" })
                {
                    var plan = Invoke(ProductType("LEProc.LaunchPlan").GetMethod("CreateAssociated", Static), null,
                        document, "\"" + Assembly.GetExecutingAssembly().Location + "\"", "--child \"%1\" %*", raw, cli);
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
            foreach (var culture in new[] { CultureInfo.GetCultureInfo("ja-JP"), CultureInfo.GetCultureInfo("de-DE_phoneb") })
            foreach (var advanced in new[] { false, true })
            {
                var entries = (Array)Invoke(ProductType("LEProc.RegistryEntriesLoader").GetMethod("GetRegistryEntries", Static), null, advanced);
                var redirectorType = ProductType("LEProc.LERegistryRedirector");
                var redirector = Activator.CreateInstance(redirectorType, Instance, null, new object[] { entries.Length }, null);
                foreach (var entry in entries)
                {
                    var getValue = (Delegate)Property(entry, "GetValue");
                    Check((bool)Invoke(redirectorType.GetMethod("AddRegistryEntry", Instance), redirector,
                        Property(entry, "Root"), Property(entry, "Key"), Property(entry, "Name"), Property(entry, "Type"),
                        getValue.DynamicInvoke(culture)), "Registry serialization rejected a product rule.");
                }
                var bytes = (byte[])Invoke(redirectorType.GetMethod("GetBinaryData", Instance), redirector);
                Check(BitConverter.ToUInt64(bytes, 0) == (ulong)entries.Length, "Serialized rule count changed.");
                var entryType = ProductType("LEProc.LERegistryRedirector+REGISTRY_REDIRECTION_ENTRY64");
                var lebSize = Marshal.SizeOf(ProductType("LEProc.LoaderWrapper+LEB"));
                var memory = Marshal.AllocHGlobal(bytes.Length);
                try
                {
                    Marshal.Copy(bytes, 0, memory, bytes.Length);
                    var found = 0;
                    for (var index = 0; index < entries.Length; index++)
                    {
                        var rule = Marshal.PtrToStructure(IntPtr.Add(memory, 8 + index * Marshal.SizeOf(entryType)), entryType);
                        foreach (var side in new[] { "Original", "Redirected" })
                        {
                            var native = Field(rule, side);
                            var name = ReadUnicodeString(bytes, Field(native, "ValueName"), lebSize);
                            var key = ReadUnicodeString(bytes, Field(native, "SubKey"), lebSize);
                            if (name != "InstallLanguage" && name != "Default") continue;
                            Check(key == @"System\CurrentControlSet\Control\Nls\Language", "Language rule targets CodePage instead of Language.");
                            Check((ulong)Field(native, "Root") == 0x80000002 && (uint)Field(native, "DataType") == 1,
                                "Language rule lost HKLM/REG_SZ.");
                            var value = Encoding.Unicode.GetString(bytes, checked((int)(long)Field(native, "Data") - lebSize),
                                checked((int)(ulong)Field(native, "DataSize")));
                            Check(value == (culture.LCID & 0xffff).ToString("X4", CultureInfo.InvariantCulture),
                                "Language value is not a four-digit hexadecimal LANGID.");
                            found++;
                        }
                    }
                    Check(found == 4, "Missing serialized language rules.");
                }
                finally { Marshal.FreeHGlobal(memory); }
            }
        }

        internal static void TimezoneNamesAndStandardTimeContract()
        {
            var zones = TimeZoneInfo.GetSystemTimeZones();
            Check(zones.Any(zone => zone.SupportsDaylightSavingTime) && zones.Any(zone => !zone.SupportsDaylightSavingTime),
                "This host must provide DST and non-DST time zones.");
            foreach (var installedZone in zones)
            {
                var zone = TimeZoneInfo.FindSystemTimeZoneById(installedZone.Id);
                var wrapper = Activator.CreateInstance(ProductType("LEProc.LoaderWrapper"), true);
                ProductType("LEProc.LoaderWrapper").GetProperty("Timezone", Instance).SetValue(wrapper, zone.Id, null);
                var timezone = Field(Field(wrapper, "_leb"), "Timezone");
                Check(Name(timezone, "StandardName") == BoundedName(zone.StandardName), "Standard name differs from Windows for " + zone.Id + ": " + Name(timezone, "StandardName") + " / " + BoundedName(zone.StandardName));
                Check(Name(timezone, "DaylightName") == BoundedName(zone.DaylightName), "Daylight name was replaced by the standard name for " + zone.Id);
                Check((int)Field(timezone, "DaylightBias") == 0, "Existing standard-time profile behavior changed.");
                foreach (var date in new[] { "StandardDate", "DaylightDate" })
                    Check(Field(timezone, date).GetType().GetFields(Instance).All(field => (ushort)field.GetValue(Field(timezone, date)) == 0),
                        "Existing DST suppression changed.");
            }
        }

        internal static void LongValidTimezoneIdIsAccepted()
        {
            const string zonesPath = @"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Time Zones";
            var fixturePath = @"Software\LE Launch Regression\" + Guid.NewGuid().ToString("N");
            var id = "LE regression valid timezone ID longer than thirty two characters";
            var hklm = new IntPtr(unchecked((int)0x80000002));
            try
            {
                using (var original = Microsoft.Win32.Registry.LocalMachine.OpenSubKey(zonesPath + @"\Tokyo Standard Time"))
                using (var fixture = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(fixturePath))
                {
                    foreach (var keyName in new[] { "Tokyo Standard Time", id })
                    using (var key = fixture.CreateSubKey(zonesPath + "\\" + keyName))
                    foreach (var name in new[] { "Display", "Std", "Dlt", "TZI" })
                        key.SetValue(name, original.GetValue(name), original.GetValueKind(name));
                    // Redirect HKLM only in this process; no machine time zones are changed.
                    var status = RegOverridePredefKey(hklm, fixture.Handle.DangerousGetHandle());
                    if (status != 0) throw new System.ComponentModel.Win32Exception(status);
                    try
                    {
                        TimeZoneInfo.ClearCachedData();
                        var zone = TimeZoneInfo.FindSystemTimeZoneById(id);
                        var wrapper = Activator.CreateInstance(ProductType("LEProc.LoaderWrapper"), true);
                        ProductType("LEProc.LoaderWrapper").GetProperty("Timezone", Instance).SetValue(wrapper, id, null);
                        var timezone = Field(Field(wrapper, "_leb"), "Timezone");
                        Check(Name(timezone, "StandardName") == BoundedName(zone.StandardName),
                            "A valid long registry ID was rejected or altered the native name.");
                    }
                    finally
                    {
                        status = RegOverridePredefKey(hklm, IntPtr.Zero);
                        TimeZoneInfo.ClearCachedData();
                        if (status != 0) throw new System.ComponentModel.Win32Exception(status);
                    }
                }
            }
            finally { Microsoft.Win32.Registry.CurrentUser.DeleteSubKeyTree(fixturePath, false); }
        }

        internal static void TimezoneNamesAreBoundedAndTerminated()
        {
            var type = ProductType("LEProc.LoaderWrapper+RTL_TIME_ZONE_INFORMATION");
            foreach (var name in new[] { "", new string('x', 31), new string('x', 32), new string('x', 80), new string('x', 30) + "\ud83d\ude00" })
            foreach (var field in new[] { "StandardName", "DaylightName" })
            {
                var timezone = Activator.CreateInstance(type);
                Invoke(type.GetMethod("Set" + field, Instance), timezone, name);
                var bytes = (byte[])Field(timezone, field);
                Check(bytes.Length == 64 && bytes[62] == 0 && bytes[63] == 0, "WCHAR[32] name has no terminator.");
                Check(Name(timezone, field) == BoundedName(name), "Timezone name was not bounded at the native field.");
            }
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

        private static void Check(bool condition, string message) { if (!condition) throw new Exception(message); }
        [DllImport("shell32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CommandLineToArgvW(string commandLine, out int count);
        [DllImport("kernel32.dll")]
        private static extern IntPtr LocalFree(IntPtr memory);
    }
}

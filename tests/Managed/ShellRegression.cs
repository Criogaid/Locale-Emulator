using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32;

namespace ManagedRegression
{
    internal static class ShellRegression
    {
        internal static readonly Action[] Tests =
        {
            InstallerLockedFirstFilePreservesPayloads,
            InstallerLockedSecondFileRollsBackPayloads,
            InstallerExtractionUsesProductDirectory,
            InstallerIdenticalLockedPayloadsAreRetained,
            InstallerRegistrationFailuresRestoreRegistryOverride,
            ShellMenuHonorsCommandBudgetAndInsertionPosition,
            ShellFinalizationReleasesAllBitmaps,
            ShellLongDropPathIsRead,
            InstallerInstalledProbeClosesRegistryKeys
        };

        internal static void InstallerLockedFirstFilePreservesPayloads() { Run("locked-first"); }
        internal static void InstallerLockedSecondFileRollsBackPayloads() { Run("locked-second"); }
        internal static void InstallerExtractionUsesProductDirectory() { Run("extract"); }
        internal static void InstallerIdenticalLockedPayloadsAreRetained() { Run("identical"); }
        internal static void InstallerRegistrationFailuresRestoreRegistryOverride() { Run("registration"); }
        internal static void ShellMenuHonorsCommandBudgetAndInsertionPosition() { Run("menu"); }
        internal static void ShellFinalizationReleasesAllBitmaps() { Run("gdi"); }
        internal static void InstallerInstalledProbeClosesRegistryKeys() { Run("registry-handles"); }
        internal static void ShellLongDropPathIsRead() { Run("long-path"); }

        private static void Run(string scenario)
        {
            string directory = Path.Combine(Path.GetTempPath(), "LE-ShellRegression-" + Guid.NewGuid());
            Directory.CreateDirectory(directory);
            AppDomain domain = null;
            try
            {
                foreach (var name in new[] { "LEInstaller.exe", "LEContextMenuHandler.dll", "LECommonLibrary.dll", "LEProc.exe" })
                    File.Copy(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, name), Path.Combine(directory, name));
                domain = AppDomain.CreateDomain("ShellRegression", null, new AppDomainSetup { ApplicationBase = directory });
                var worker = (ShellRegressionWorker)domain.CreateInstanceFromAndUnwrap(
                    typeof(ShellRegressionWorker).Assembly.Location, typeof(ShellRegressionWorker).FullName);
                worker.Run(scenario, directory);
            }
            finally
            {
                if (domain != null) AppDomain.Unload(domain);
                Directory.Delete(directory, true);
            }
        }
    }

    public sealed class ShellRegressionWorker : MarshalByRefObject
    {
        private const BindingFlags InstancePrivate = BindingFlags.Instance | BindingFlags.NonPublic;
        private string directory;
        private Type formType;
        private Type shellType;

        public override object InitializeLifetimeService() { return null; }

        public void Run(string scenario, string productDirectory)
        {
            directory = productDirectory;
            formType = Assembly.LoadFrom(Path.Combine(directory, "LEInstaller.exe")).GetType("LEInstaller.Form1", true);
            if (scenario == "registration" || scenario == "registry-handles")
            {
                using (var classes = Registry.CurrentUser.OpenSubKey(@"Software\Classes"))
                    Check(classes != null, "An existing user Classes key is required for registry tests.");
                if (scenario == "registration") RegistrationFailures(); else RegistryHandles();
                return;
            }
            if (scenario == "menu" || scenario == "gdi" || scenario == "long-path")
            {
                shellType = Assembly.LoadFrom(Path.Combine(directory, "LEContextMenuHandler.dll"))
                    .GetType("LEContextMenuHandler.FileContextMenuExt", true);
                if (scenario == "menu") Menus();
                else if (scenario == "gdi") Bitmaps();
                else LongDropPath();
                return;
            }
            Extraction(scenario);
        }

        private void Extraction(string scenario)
        {
            var form = Activator.CreateInstance(formType, new object[] { 0 });
            string originalCwd = Environment.CurrentDirectory;
            string elsewhere = Path.Combine(directory, "unrelated-cwd");
            Directory.CreateDirectory(elsewhere);
            string first = Path.Combine(directory, "LEContextMenuHandler.dll");
            string second = Path.Combine(directory, "LECommonLibrary.dll");
            byte[] oldFirst = File.ReadAllBytes(first), oldSecond = File.ReadAllBytes(second);
            try
            {
                Environment.CurrentDirectory = elsewhere;
                if (scenario == "identical")
                {
                    Check((bool)Invoke(form, "ReplaceDll"), "Initial extraction failed.");
                    byte[] expectedFirst = File.ReadAllBytes(first), expectedSecond = File.ReadAllBytes(second);
                    using (File.Open(first, FileMode.Open, FileAccess.Read, FileShare.Read))
                    using (File.Open(second, FileMode.Open, FileAccess.Read, FileShare.Read))
                        Check((bool)Invoke(form, "ReplaceDll"), "Identical locked payloads should not require replacement.");
                    Check(File.ReadAllBytes(first).SequenceEqual(expectedFirst) && File.ReadAllBytes(second).SequenceEqual(expectedSecond),
                        "Identical payloads changed.");
                }
                else
                {
                    File.WriteAllText(first, "old-handler");
                    File.WriteAllText(second, "old-common");
                    if (scenario.StartsWith("locked-", StringComparison.Ordinal))
                    {
                        using (File.Open(scenario == "locked-first" ? first : second, FileMode.Open, FileAccess.Read, FileShare.Read))
                            Check(!(bool)Invoke(form, "ReplaceDll"), "Locked outdated payload should fail extraction.");
                        Check(File.ReadAllText(first) == "old-handler", "Failed extraction lost or changed the old handler.");
                        Check(File.ReadAllText(second) == "old-common", "Failed extraction lost or changed the old common library.");
                    }
                    else
                    {
                        Check((bool)Invoke(form, "ReplaceDll"), "Extraction from a different working directory failed.");
                        Check(File.ReadAllBytes(first).SequenceEqual(oldFirst),
                            "Extracted handler did not match the current product.");
                        Check(File.ReadAllBytes(second).SequenceEqual(oldSecond),
                            "Extracted common library did not match the current product.");
                    }
                }
                Check(Directory.GetFiles(elsewhere).Length == 0, "Extraction wrote to the working directory.");
                Check(Directory.GetFiles(directory, "*.installer.tmp").Length == 0, "Staged files leaked.");
            }
            finally
            {
                Environment.CurrentDirectory = originalCwd;
                ((IDisposable)form).Dispose();
                File.WriteAllBytes(first, oldFirst);
                File.WriteAllBytes(second, oldSecond);
            }
        }

        private void RegistrationFailures()
        {
            var form = Activator.CreateInstance(formType, new object[] { 0 });
            string handler = Path.Combine(directory, "LEContextMenuHandler.dll");
            var handlerBytes = File.ReadAllBytes(handler);
            File.Delete(handler);
            try
            {
                foreach (string method in new[] { "DoRegister", "DoUnRegister" })
                    foreach (bool allUsers in new[] { false, true })
                        Check(Equals(Invoke(form, method, allUsers), false),
                            "Missing payload did not return an explicit registration failure.");

                // No COM-visible types: RegistrationServices returns false without registering anything.
                var name = new AssemblyName("NoComTypes" + Guid.NewGuid().ToString("N"));
                var assembly = AppDomain.CurrentDomain.DefineDynamicAssembly(name, AssemblyBuilderAccess.Save, directory);
                assembly.SetCustomAttribute(new CustomAttributeBuilder(typeof(ComVisibleAttribute).GetConstructor(new[] { typeof(bool) }), new object[] { false }));
                assembly.DefineDynamicModule(name.Name, "LEContextMenuHandler.dll").DefineType("NoComType", TypeAttributes.Public).CreateType();
                assembly.Save("LEContextMenuHandler.dll");
                const string machineInterface = @"Interface\{00000000-0000-0000-C000-000000000046}";
                bool before;
                using (var key = Registry.ClassesRoot.OpenSubKey(machineInterface)) before = key != null;
                Check(before, "Machine IUnknown interface is required to observe HKCR restoration.");
                foreach (string method in new[] { "DoRegister", "DoUnRegister" })
                {
                    Check(!(bool)Invoke(form, method, false), "SDK false result was reported as success.");
                    using (var key = Registry.ClassesRoot.OpenSubKey(machineInterface))
                        Check(key != null, "HKCR override remained active after registration failure.");
                }
                File.WriteAllBytes(handler, handlerBytes);
                var manager = formType.Assembly.GetType("LEInstaller.ShellExtensionManager", true);
                var keyName = manager.GetField("keyName", BindingFlags.Static | BindingFlags.NonPublic);
                var originalKeyName = (string)keyName.GetValue(null);
                const string clsidPath = @"Software\Classes\CLSID\{C52B9871-E5E9-41FD-B84D-C5ACADBEC7AE}\InprocServer32";
                var shellBefore = (bool)manager.GetMethod("IsInstalled").Invoke(null, new object[] { false });
                bool comBefore;
                using (var comKey = Registry.CurrentUser.OpenSubKey(clsidPath)) comBefore = comKey != null;
                try
                {
                    keyName.SetValue(null, null);
                    Check(!(bool)Invoke(form, "DoRegister", false),
                        "A Shell registration failure was reported as success.");
                }
                finally
                {
                    keyName.SetValue(null, originalKeyName);
                }
                Check((bool)manager.GetMethod("IsInstalled").Invoke(null, new object[] { false }) == shellBefore,
                    "A failed registration changed the Shell registration state.");
                using (var comKey = Registry.CurrentUser.OpenSubKey(clsidPath))
                    Check((comKey != null) == comBefore, "A failed registration changed the COM registration state.");
            }
            finally
            {
                try { Invoke(form, "OverrideHKCR", true); }
                finally { ((IDisposable)form).Dispose(); }
            }
        }

        private void RegistryHandles()
        {
            var manager = formType.Assembly.GetType("LEInstaller.ShellExtensionManager", true);
            var keyName = manager.GetField("keyName", BindingFlags.Static | BindingFlags.NonPublic);
            string original = (string)keyName.GetValue(null);
            string isolated = @"Software\Classes\LE-ShellRegression-" + Guid.NewGuid();
            try
            {
                using (Registry.CurrentUser.CreateSubKey(isolated)) { }
                keyName.SetValue(null, isolated);
                var probe = manager.GetMethod("IsInstalled");
                probe.Invoke(null, new object[] { false });
                int before = Process.GetCurrentProcess().HandleCount;
                for (int i = 0; i < 2000; i++)
                    Check((bool)probe.Invoke(null, new object[] { false }), "Installed key was not found.");
                int delta = Process.GetCurrentProcess().HandleCount - before;
                Console.WriteLine("IsInstalled handle delta=" + delta);
                Check(delta <= 8, "IsInstalled leaked registry handles: " + delta);
            }
            finally
            {
                keyName.SetValue(null, original);
                Registry.CurrentUser.DeleteSubKeyTree(isolated, false);
            }

            var staticKeyName = manager.GetField("staticVerbKeyName", BindingFlags.Static | BindingFlags.NonPublic);
            string staticOriginal = (string)staticKeyName.GetValue(null);
            string staticIsolated = @"Software\Classes\LE-ShellRegression-Static-" + Guid.NewGuid();
            try
            {
                staticKeyName.SetValue(null, staticIsolated);
                var launcher = Path.Combine(directory, "LEProc.exe");
                manager.GetMethod("RegisterStaticExplorerVerb").Invoke(null, new object[] { launcher });
                using (var command = Registry.CurrentUser.OpenSubKey(staticIsolated + @"\command"))
                {
                    Check(command != null, "Static Explorer verb command key was not created.");
                    Check((string)command.GetValue(null) == "\"" + launcher + "\" \"%1\"",
                        "Static Explorer verb command was encoded incorrectly.");
                }
                Check((bool)manager.GetMethod("IsStaticExplorerVerbInstalled").Invoke(null, null),
                    "Static Explorer verb was not detected after registration.");
                manager.GetMethod("UnregisterStaticExplorerVerb").Invoke(null, null);
                Check(!(bool)manager.GetMethod("IsStaticExplorerVerbInstalled").Invoke(null, null),
                    "Static Explorer verb remained after unregistration.");
            }
            finally
            {
                staticKeyName.SetValue(null, staticOriginal);
                Registry.CurrentUser.DeleteSubKeyTree(staticIsolated, false);
            }
        }

        private void LongDropPath()
        {
            var method = shellType.GetMethod("GetDropFileName", BindingFlags.Static | BindingFlags.NonPublic);
            Check(method != null, "Long path helper is missing.");
            var path = Path.Combine(directory, new string('x', 300) + ".exe");
            var bytes = Encoding.Unicode.GetBytes(path + "\0\0");
            var hDrop = GlobalAlloc(0x42, (UIntPtr)(20 + bytes.Length));
            Check(hDrop != IntPtr.Zero, "GlobalAlloc failed.");
            try
            {
                var memory = GlobalLock(hDrop);
                try
                {
                    Check(memory != IntPtr.Zero, "GlobalLock failed.");
                    Marshal.WriteInt32(memory, 0, 20);
                    Marshal.WriteInt32(memory, 16, 1);
                    Marshal.Copy(bytes, 0, IntPtr.Add(memory, 20), bytes.Length);
                }
                finally
                {
                    if (memory != IntPtr.Zero) GlobalUnlock(hDrop);
                }
                var actual = (string)method.Invoke(null, new object[] { hDrop });
                Check(actual == path, "Long shell path was truncated.");
            }
            finally
            {
                GlobalFree(hDrop);
            }
        }

        private void Menus()
        {
            object handler = Activator.CreateInstance(shellType);
            // Build a deterministic mix of main-menu and submenu profiles through the real internal value type.
            var items = (IList)shellType.GetField("menuItems", InstancePrivate).GetValue(handler);
            while (items.Count > 4) items.RemoveAt(items.Count - 1);
            var itemType = shellType.Assembly.GetType("LEContextMenuHandler.LEMenuItem", true);
            for (int i = 0; i < 2; i++)
            {
                object item = Activator.CreateInstance(itemType);
                itemType.GetField("Text", InstancePrivate).SetValue(item, "Profile " + i);
                itemType.GetField("Commands", InstancePrivate).SetValue(item, "-profile " + i);
                itemType.GetField("ShowInMainMenu", InstancePrivate).SetValue(item, (bool?)(i == 0));
                items.Add(item);
            }
            foreach (var range in new[] { new uint[] { 200, 199, 0 }, new uint[] { 200, 200, 0 },
                new uint[] { 200, 201, 2 }, new uint[] { 200, 202, 3 }, new uint[] { 200, 205, 6 },
                new uint[] { 200, 999, 6 }, new uint[] { uint.MaxValue - 1, uint.MaxValue, 2 } })
                CheckMenu(handler, range[0], range[1], (int)range[2], 0);
            CheckMenu(handler, 200, 999, 0, 1); // CMF_DEFAULTONLY
            GC.KeepAlive(handler);
        }

        private void CheckMenu(object handler, uint first, uint last, int expected, uint flags)
        {
            IntPtr menu = CreatePopupMenu();
            Check(menu != IntPtr.Zero, "CreatePopupMenu failed.");
            try
            {
                for (uint i = 0; i < 5; i++) Check(AppendMenu(menu, 0, (UIntPtr)(1000 + i), "Existing " + i), "AppendMenu failed.");
                int result = (int)shellType.GetMethod("QueryContextMenu").Invoke(handler, new object[] { menu, 3u, first, last, flags });
                Check(result == expected, "Wrong command count for " + first + ".." + last + ": " + result);
                for (int i = 0; i < 3; i++) Check(GetMenuItemID(menu, i) == 1000 + i, "Items before iMenu changed.");
                int added = GetMenuItemCount(menu) - 5;
                Check(GetMenuItemID(menu, 3 + added) == 1003 && GetMenuItemID(menu, 4 + added) == 1004, "Existing trailing items moved out of order.");
                var ids = new List<uint>();
                CollectIds(menu, ids);
                var actual = ids.Where(id => id < 1000 || id > 1004).ToArray();
                Check(actual.Length == expected, "Wrong number of assigned IDs.");
                for (int i = 0; i < expected; i++) Check(actual.Contains(first + (uint)i), "Command offset mapping changed.");
                if (expected == 6)
                {
                    Check(GetMenuItemID(menu, 4) == first + 4, "Main-menu profile moved into the submenu.");
                    IntPtr subMenu = GetSubMenu(menu, 5);
                    Check(subMenu != IntPtr.Zero && GetMenuItemID(subMenu, 0) == first + 5,
                        "Submenu profile placement changed.");
                }
                Console.WriteLine("menu " + first + ".." + last + " returned=" + result + " inserted=" + added);
            }
            finally { DestroyMenu(menu); GC.KeepAlive(handler); }
        }

        private static void CollectIds(IntPtr menu, List<uint> ids)
        {
            for (int i = 0; i < GetMenuItemCount(menu); i++)
            {
                IntPtr sub = GetSubMenu(menu, i);
                // GetMenuItemID uses UINT_MAX for popups; inspect the assigned ID and separator type.
                var info = new MenuItemInfo { Size = (uint)Marshal.SizeOf(typeof(MenuItemInfo)), Mask = 0x102 };
                Check(GetMenuItemInfo(menu, (uint)i, true, ref info), "GetMenuItemInfo failed.");
                if ((info.Type & 0x800) == 0) ids.Add(info.Id);
                if (sub != IntPtr.Zero) CollectIds(sub, ids);
            }
        }

        private void Bitmaps()
        {
            MakeHandlers(2);
            uint before = GdiCount();
            MakeHandlers(30);
            uint after = GdiCount();
            Console.WriteLine("GDI after finalization before=" + before + " after=" + after + " delta=" + ((long)after - before));
            Check(after <= before, "Shell handler finalization leaked GDI objects.");
        }

        [MethodImpl(MethodImplOptions.NoInlining)]
        private void MakeHandlers(int count)
        {
            for (int i = 0; i < count; i++) GC.KeepAlive(Activator.CreateInstance(shellType));
        }

        private static uint GdiCount()
        {
            GC.Collect(); GC.WaitForPendingFinalizers(); GC.Collect();
            using (var process = Process.GetCurrentProcess()) return GetGuiResources(process.Handle, 0);
        }

        private static object Invoke(object target, string name, params object[] arguments)
        {
            return target.GetType().GetMethod(name, InstancePrivate).Invoke(target, arguments);
        }

        private static void Check(bool condition, string message)
        {
            if (!condition) throw new InvalidOperationException(message);
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct MenuItemInfo
        {
            internal uint Size, Mask, Type, State, Id;
            internal IntPtr SubMenu, Checked, Unchecked, Data, Text;
            internal uint TextLength;
            internal IntPtr Bitmap;
        }

        [DllImport("kernel32.dll", SetLastError = true)] private static extern IntPtr GlobalAlloc(uint flags, UIntPtr bytes);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern IntPtr GlobalLock(IntPtr memory);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern bool GlobalUnlock(IntPtr memory);
        [DllImport("kernel32.dll", SetLastError = true)] private static extern IntPtr GlobalFree(IntPtr memory);
        [DllImport("user32.dll")] private static extern IntPtr CreatePopupMenu();
        [DllImport("user32.dll")] private static extern bool DestroyMenu(IntPtr menu);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern bool AppendMenu(IntPtr menu, uint flags, UIntPtr id, string text);
        [DllImport("user32.dll")] private static extern int GetMenuItemCount(IntPtr menu);
        [DllImport("user32.dll")] private static extern uint GetMenuItemID(IntPtr menu, int index);
        [DllImport("user32.dll")] private static extern IntPtr GetSubMenu(IntPtr menu, int index);
        [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern bool GetMenuItemInfo(IntPtr menu, uint index, bool byPosition, ref MenuItemInfo info);
        [DllImport("user32.dll")] private static extern uint GetGuiResources(IntPtr process, uint flag);
    }
}

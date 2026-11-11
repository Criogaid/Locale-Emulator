using System;
using System.ComponentModel;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;
using System.Xml.Linq;
using System.Security.Principal;

namespace LEInstaller
{
    public partial class Form1 : Form
    {
        internal const int ModeInstallAll = 1;
        internal const int ModeUninstallAll = 2;
        internal const int ModeInstallUser = 3;
        internal const int ModeUninstallUser = 4;

        private int cmdMode = 0;
        private bool IsHeadlessMode => cmdMode == ModeInstallUser || cmdMode == ModeUninstallUser;
        private readonly string crtDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
        private Exception operationError;
        
        public Form1(int CmdMode=0)
        {
            // We need to remove all ADS first.
            // https://github.com/xupefei/Locale-Emulator/issues/22.
            foreach (var f in Directory.GetFiles(crtDir, "*", SearchOption.AllDirectories))
                RemoveADS(f);

            DisableDPIScale();

            InitializeComponent();
            ApplyLanguage();
            cmdMode = CmdMode;
            if (IsHeadlessMode)
            {
                // Scripted per-user (un)install: no visible window; the exit code reports the result.
                Opacity = 0;
                ShowInTaskbar = false;
            }
        }

        private void ApplyLanguage()
        {
            Text = I18n.GetString("Title");
            label1.Text = I18n.GetString("IntroText");
            buttonInstall.Text = I18n.GetString("CurrentUserInstall");
            buttonUninstall.Text = I18n.GetString("CurrentUserUninstall");
            buttonInstallAllUsers.Text = I18n.GetString("AllUsersInstall");
            buttonUninstallAllUsers.Text = I18n.GetString("AllUsersUninstall");
            buttonEditGlobal.Text = I18n.GetString("EditGlobal");
            groupBox1.Text = I18n.GetString("GroupCurrentUser");
            groupBox2.Text = I18n.GetString("GroupAllUsers");
            groupBox3.Text = I18n.GetString("GroupProfiles");
        }

        private void RunAsAdmin(string Arguments="")
        {
            var startup = new ProcessStartInfo();
            startup.WindowStyle = ProcessWindowStyle.Normal;
            startup.UseShellExecute = true;
            startup.WorkingDirectory = Environment.CurrentDirectory;
            startup.Arguments = Arguments;
            startup.FileName = Application.ExecutablePath;
            startup.Verb = "runas";

            try
            {
                using (var proc = Process.Start(startup))
                {
                    Environment.Exit(0);
                    return;
                }
            }
            catch (SystemException)
            {
                MessageBox.Show(this, I18n.GetString("AdminLaunchError"),
                    I18n.GetString("Title"),
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return;
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            if (Environment.OSVersion.Version.CompareTo(new Version(6, 0)) <= 0)
            {
                MessageBox.Show(this, I18n.GetString("OSOutdated"),
                    I18n.GetString("Title"),
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);

                Environment.Exit(0);
            }

            Text += " - V" + GetLEVersion();

            if (IsAdministrator())
            {
                Text += " (Administrator)";

                buttonInstall.Enabled = false;
            }

            DeleteOldFiles();
            if (!ReplaceDll())
            {
                buttonInstall.Enabled = false;
                buttonInstallAllUsers.Enabled = false;
                ShowOperationError();
                if (IsHeadlessMode)
                    Environment.Exit(1);
                return;
            }

            AddShieldToButton(buttonInstallAllUsers);
            AddShieldToButton(buttonUninstallAllUsers);

            buttonUninstall.Enabled = ShellExtensionManager.IsInstalled(false) || ShellExtensionManager.IsStaticExplorerVerbInstalled();
            buttonUninstallAllUsers.Enabled = ShellExtensionManager.IsInstalled(true);
            buttonEditGlobal.Enabled = ShellExtensionManager.IsInstalled(true) || ShellExtensionManager.IsInstalled(false) || ShellExtensionManager.IsStaticExplorerVerbInstalled();

            switch (cmdMode)
            {
                case ModeInstallAll:
                    buttonInstallAllUsers.PerformClick();
                    break;
                case ModeUninstallAll:
                    buttonUninstallAllUsers.PerformClick();
                    break;
                case ModeInstallUser:
                case ModeUninstallUser:
                    RunHeadless(cmdMode == ModeInstallUser);
                    break;
                default:
                    break;
            }
        }

        private void RunHeadless(bool register)
        {
            var succeeded = ChangeRegistration(false, register);
            if (succeeded)
                NotifyShell();
            else
                ShowOperationError();
            Environment.Exit(succeeded ? 0 : 1);
        }

        private void buttonInstall_Click(object sender, EventArgs e)
        {
            if (!DoRegister(false))
            {
                ShowOperationError();
                return;
            }

            NotifyShell();

            MessageBox.Show(this, I18n.GetString("InstallFinished"),
                I18n.GetString("Title"),
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);

            buttonUninstall.Enabled = true;
            buttonEditGlobal.Enabled = true;
        }

        private void buttonInstallAllUsers_Click(object sender, EventArgs e)
        {
            if (!IsAdministrator())
            {
                
                RunAsAdmin("--InstallAll");
                return;
            }

            if (!DoRegister(true))
            {
                ShowOperationError();
                return;
            }

            NotifyShell();

            MessageBox.Show(this, I18n.GetString("InstallFinished"),
                I18n.GetString("Title"),
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);

            buttonUninstallAllUsers.Enabled = true;
            buttonEditGlobal.Enabled = true;
        }

        private void buttonEditGlobal_Click(object sender, EventArgs e)
        {
            Process.Start(Path.Combine(crtDir, "LEGUI.exe"));
            Application.Exit();
        }

        public static void DisableDPIScale()
        {
            SetProcessDPIAware();
        }

        private void RemoveADS(string s)
        {
            File.SetAttributes(s, FileAttributes.Normal);

            if (DeleteFile(s + ":Zone.Identifier") == 0)
            {
                SetLastError(0);
            }
        }

        private void buttonUninstall_Click(object sender, EventArgs e)
        {
            if (!DoUnRegister(false))
            {
                ShowOperationError();
                return;
            }

            NotifyShell();

            MessageBox.Show(this, I18n.GetString("UninstallFinished"),
                I18n.GetString("Title"),
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);

            buttonUninstall.Enabled = false;
        }

        private void buttonUninstallAllUsers_Click(object sender, EventArgs e)
        {
            if (!IsAdministrator())
            {

                
                RunAsAdmin("--UninstallAll");
                return;
            }

            if (!DoUnRegister(true))
            {
                ShowOperationError();
                return;
            }

            NotifyShell();

            MessageBox.Show(this, I18n.GetString("UninstallFinished"),
                I18n.GetString("Title"),
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);

            buttonUninstallAllUsers.Enabled = false;
        }

        protected override void OnClosing(CancelEventArgs e)
        {
            Environment.Exit(0);
        }

        private bool DoRegister(bool allUsers)
        {
            return ChangeRegistration(allUsers, true);
        }

        private bool DoUnRegister(bool allUsers)
        {
            return ChangeRegistration(allUsers, false);
        }

        private void ShowOperationError()
        {
            MessageBox.Show(this, operationError.Message, I18n.GetString("Title"),
                MessageBoxButtons.OK, MessageBoxIcon.Error);
        }

        private bool ChangeRegistration(bool allUsers, bool register)
        {
            operationError = null;
            var staticVerbTouched = false;
            var staticVerbWasInstalled = false;
            try
            {
                // Load before overriding HKCR so missing payloads have no registry side effects.
                if (allUsers && !IsAdministrator())
                    throw new UnauthorizedAccessException(I18n.GetString("AllUsersAdminRequired"));
                var shellWasInstalled = ShellExtensionManager.IsInstalled(allUsers);
                var assembly = register || shellWasInstalled
                    ? Assembly.LoadFrom(Path.Combine(crtDir, "LEContextMenuHandler.dll"))
                    : null;
                staticVerbWasInstalled = !allUsers && ShellExtensionManager.IsStaticExplorerVerbInstalled();
                var rs = new RegistrationServices();
                var assemblyTouched = false;
                var shellTouched = false;
                try
                {
                    if (!allUsers)
                        OverrideHKCR();
                    try
                    {
                        if (register)
                        {
                            if (!allUsers)
                            {
                                staticVerbTouched = true;
                                ShellExtensionManager.RegisterStaticExplorerVerb(Path.Combine(crtDir, "LEProc.exe"));
                            }
                            assemblyTouched = true;
                            if (!rs.RegisterAssembly(assembly, AssemblyRegistrationFlags.SetCodeBase))
                                throw new InvalidOperationException(I18n.GetString("NoRegisterableComTypes"));
                            shellTouched = true;
                            ShellExtensionManager.RegisterShellExtContextMenuHandler(allUsers);
                        }
                        else
                        {
                            if (!allUsers)
                            {
                                staticVerbTouched = true;
                                ShellExtensionManager.UnregisterStaticExplorerVerb();
                            }
                            if (shellWasInstalled)
                            {
                                shellTouched = true;
                                ShellExtensionManager.UnregisterShellExtContextMenuHandler(allUsers);
                                assemblyTouched = true;
                                if (!rs.UnregisterAssembly(assembly))
                                    throw new InvalidOperationException(I18n.GetString("NoRegisterableComTypes"));
                            }
                        }
                    }
                    catch (Exception error)
                    {
                        var rollbackErrors = new List<Exception>();
                        if (register)
                        {
                            if (!allUsers && staticVerbTouched)
                            {
                                try
                                {
                                    if (staticVerbWasInstalled)
                                        ShellExtensionManager.RegisterStaticExplorerVerb(Path.Combine(crtDir, "LEProc.exe"));
                                    else
                                        ShellExtensionManager.UnregisterStaticExplorerVerb();
                                }
                                catch (Exception rollbackError) { rollbackErrors.Add(rollbackError); }
                            }
                            if (shellTouched)
                            {
                                try { ShellExtensionManager.UnregisterShellExtContextMenuHandler(allUsers); }
                                catch (Exception rollbackError) { rollbackErrors.Add(rollbackError); }
                            }
                            if (assemblyTouched)
                            {
                                try { rs.UnregisterAssembly(assembly); }
                                catch (Exception rollbackError) { rollbackErrors.Add(rollbackError); }
                            }
                        }
                        else
                        {
                            if (!allUsers && staticVerbTouched && staticVerbWasInstalled)
                            {
                                try { ShellExtensionManager.RegisterStaticExplorerVerb(Path.Combine(crtDir, "LEProc.exe")); }
                                catch (Exception rollbackError) { rollbackErrors.Add(rollbackError); }
                            }
                            if (assemblyTouched)
                            {
                                try { rs.RegisterAssembly(assembly, AssemblyRegistrationFlags.SetCodeBase); }
                                catch (Exception rollbackError) { rollbackErrors.Add(rollbackError); }
                            }
                            if (shellTouched)
                            {
                                try { ShellExtensionManager.RegisterShellExtContextMenuHandler(allUsers); }
                                catch (Exception rollbackError) { rollbackErrors.Add(rollbackError); }
                            }
                        }
                        if (rollbackErrors.Count != 0)
                            throw new AggregateException(new[] { error }.Concat(rollbackErrors));
                        throw;
                    }
                }
                finally
                {
                    if (!allUsers)
                        OverrideHKCR(true);
                }
                return true;
            }
            catch (Exception e)
            {
                operationError = e;
                return false;
            }
        }

        private void OverrideHKCR(bool restore = false)
        {
            UIntPtr classesRoot = UIntPtr.Size == 8 ? new UIntPtr(0xFFFFFFFF80000000) : new UIntPtr(0x80000000);
            if (restore)
            {
                CheckRegistryStatus(RegOverridePredefKey(classesRoot, UIntPtr.Zero));
                return;
            }

            // A fresh Windows profile may not have Software\Classes yet.
            using (var classes = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(@"Software\Classes"))
            {
                if (classes == null)
                    throw new InvalidOperationException(I18n.GetString("CannotOpenClassesKey"));
            }
            UIntPtr currentUser = UIntPtr.Size == 8 ? new UIntPtr(0xFFFFFFFF80000001) : new UIntPtr(0x80000001);
            UIntPtr key;
            const uint keyReadWrite = 0x2001F;
            CheckRegistryStatus(RegOpenKeyEx(currentUser, @"Software\Classes", 0, keyReadWrite, out key));
            try
            {
                CheckRegistryStatus(RegOverridePredefKey(classesRoot, key));
            }
            finally
            {
                CheckRegistryStatus(RegCloseKey(key));
            }
        }

        private static void CheckRegistryStatus(int status)
        {
            if (status != 0)
                throw new Win32Exception(status);
        }

        private void DeleteOldFiles()
        {
            foreach (var file in Directory.EnumerateFiles(crtDir, "*.installer.bak", SearchOption.AllDirectories))
            {
                try
                {
                    File.Delete(file);
                }
                catch (Exception)
                {
                }
            }
        }

        private static byte[] ReadEmbeddedPayload(string fileName)
        {
            using (var source = Assembly.GetExecutingAssembly().GetManifestResourceStream(fileName))
            using (var buffer = new MemoryStream())
            {
                if (source == null)
                    throw new FileNotFoundException(I18n.GetString("MissingPayload"), fileName);
                source.CopyTo(buffer);
                return buffer.ToArray();
            }
        }

        private bool ReplaceDll()
        {
            operationError = null;
            var applied = new List<KeyValuePair<string, string>>();
            try
            {
                // Payload DLLs plus their satellite resource assemblies ("<culture>/<name>.resources.dll").
                var names = Assembly.GetExecutingAssembly().GetManifestResourceNames()
                    .Where(name => name.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
                    .ToArray();
                var contents = names.Select(ReadEmbeddedPayload).ToArray();
                for (int i = 0; i < names.Length; i++)
                {
                    var path = Path.Combine(crtDir, names[i].Replace('/', Path.DirectorySeparatorChar));
                    Directory.CreateDirectory(Path.GetDirectoryName(path));
                    if (File.Exists(path) && File.ReadAllBytes(path).SequenceEqual(contents[i]))
                        continue;
                    var staged = Path.Combine(Path.GetDirectoryName(path), Guid.NewGuid() + ".installer.tmp");
                    var backup = File.Exists(path) ? Path.Combine(Path.GetDirectoryName(path), Guid.NewGuid() + ".installer.bak") : null;
                    try
                    {
                        File.WriteAllBytes(staged, contents[i]);
                        if (backup == null)
                            File.Move(staged, path);
                        else
                            File.Replace(staged, path, backup);
                        applied.Add(new KeyValuePair<string, string>(path, backup));
                    }
                    finally
                    {
                        if (File.Exists(staged))
                            File.Delete(staged);
                    }
                }
                return true;
            }
            catch (Exception e)
            {
                operationError = e;
                for (int i = applied.Count - 1; i >= 0; i--)
                {
                    try
                    {
                        if (applied[i].Value == null)
                            File.Delete(applied[i].Key);
                        else
                            File.Replace(applied[i].Value, applied[i].Key, null);
                    }
                    catch (Exception rollbackError)
                    {
                        operationError = new AggregateException(operationError, rollbackError);
                    }
                }
                return false;
            }
        }

        private void NotifyShell()
        {
            const uint SHCNE_ASSOCCHANGED = 0x08000000;
            const ushort SHCNF_IDLIST = 0x0000;

            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, IntPtr.Zero, IntPtr.Zero);
        }

        static internal void AddShieldToButton(Button b)
        {
            const uint BCM_FIRST = 0x1600; //Normal button
            const uint BCM_SETSHIELD = (BCM_FIRST + 0x000C); //Elevated button

            b.FlatStyle = FlatStyle.System;
            SendMessage(b.Handle, BCM_SETSHIELD, 0, 0xFFFFFFFF);
        }

        // We should not use the LECommonLibrary.
        private static string GetLEVersion()
        {
            try
            {
                var versionPath =
                    Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location),
                        "LEVersion.xml");

                var doc = XDocument.Load(versionPath);

                return doc.Descendants("LEVersion").First().Attribute("Version").Value;
            }
            catch
            {
                return "0.0.0.0";
            }
        }

        public static bool IsAdministrator()
        {
            var wp = new WindowsPrincipal(WindowsIdentity.GetCurrent());

            return wp.IsInRole(WindowsBuiltInRole.Administrator);
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern int DeleteFile(string name);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern void SetLastError(int errorCode);

        [DllImport("shell32.dll", SetLastError = true)]
        private static extern void SHChangeNotify(uint wEventId, ushort uFlags, IntPtr dwItem1, IntPtr dwItem2);

        [DllImport("advapi32.dll", CharSet = CharSet.Unicode)]
        private static extern int RegOpenKeyEx(UIntPtr hKey, string subKey, int ulOptions, uint samDesired,
            out UIntPtr hkResult);

        [DllImport("advapi32.dll", SetLastError = true)]
        private static extern int RegOverridePredefKey(UIntPtr hKey, UIntPtr hNewKey);

        [DllImport("advapi32.dll", SetLastError = true)]
        public static extern int RegCloseKey(UIntPtr hKey);


        [DllImport("user32.dll", SetLastError = true)]
        public static extern UInt32 SendMessage(IntPtr hWnd, UInt32 msg, UInt32 wParam, UInt32 lParam);

        [DllImport("user32.dll")]
        private static extern bool SetProcessDPIAware();

    }
}
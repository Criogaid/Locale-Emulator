using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Windows.Forms;
using System.Xml.Linq;

namespace LECommonLibrary
{
    public static class GlobalHelper
    {
        public static readonly string RuntimeDirectory =
            Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
        public static string ProductDirectory
        {
            get
            {
                var runtimeDirectory = RuntimeDirectory;
                var parent = Directory.GetParent(runtimeDirectory);
                return string.Equals(Path.GetFileName(runtimeDirectory), "x86", StringComparison.OrdinalIgnoreCase)
                       && parent != null && File.Exists(Path.Combine(parent.FullName, "LEGUI.exe"))
                    ? parent.FullName
                    : runtimeDirectory;
            }
        }

        public static string GetLauncherPath(string targetPath)
        {
            var targetType = GetTargetPEType(targetPath);
            if (targetType == PEType.Unknown)
                return null;

            var launcher = targetType == PEType.X64
                ? Path.Combine(ProductDirectory, "LEProc.exe")
                : Path.Combine(ProductDirectory, "x86", "LEProc.exe");
            if (targetType == PEType.X32 && !File.Exists(launcher))
                launcher = Path.Combine(ProductDirectory, "LEProc.exe");

            return File.Exists(launcher) && PEFileReader.GetPEType(launcher) == targetType
                ? launcher
                : null;
        }

        private static PEType GetTargetPEType(string targetPath)
        {
            if (string.IsNullOrEmpty(targetPath))
                return PEType.Unknown;

            var executable = targetPath;
            if (!string.Equals(Path.GetExtension(targetPath), ".exe", StringComparison.OrdinalIgnoreCase))
            {
                var associated = AssociationReader.GetAssociatedProgram(Path.GetExtension(targetPath));
                executable = associated == null ? string.Empty : associated[0];
                if (SystemHelper.Is64BitOS())
                    executable = SystemHelper.RedirectToWow64(executable);
            }

            return PEFileReader.GetPEType(executable);
        }
        public static string GlobalVersionPath =
            Path.Combine(ProductDirectory, "LEVersion.xml");

        public static string GetLEVersion()
        {
            try
            {
                var doc = XDocument.Load(GlobalVersionPath);

                return doc.Descendants("LEVersion").First().Attribute("Version").Value;
            }
            catch
            {
                return "0.0.0.0";
            }
        }

        public static void ShowErrorDebugMessageBox(string commandLine, uint errorCode)
        {
            MessageBox.Show(
                string.Format(Localization.GetString("DebugReport"),
                    errorCode,
                    commandLine,
                    $"{Environment.OSVersion} {(SystemHelper.Is64BitOS() ? "x64" : "x86")}",
                    ProductDirectory,
                    Environment.CurrentDirectory,
                    GenerateSystemDllVersionList()),
                string.Format(Localization.GetString("VersionTitle"), GetLEVersion()));
        }

        private static string GenerateSystemDllVersionList()
        {
            string[] dlls = {"NTDLL.DLL", "KERNELBASE.DLL", "KERNEL32.DLL", "USER32.DLL", "GDI32.DLL"};

            var result = new StringBuilder();

            foreach (var dll in dlls)
            {
                var version = FileVersionInfo.GetVersionInfo(
                                                             Path.Combine(
                                                                          Path.GetPathRoot(Environment.SystemDirectory),
                                                                          SystemHelper.Is64BitOS()
                                                                              ? @"Windows\SysWOW64\"
                                                                              : @"Windows\System32\",
                                                                          dll));

                result.Append(dll);
                result.Append(": ");
                result.Append(
                              $"{version.FileMajorPart}.{version.FileMinorPart}.{version.FileBuildPart}.{version.FilePrivatePart}");
                result.Append("\r\n");
            }

            return result.ToString();
        }

        public static bool CheckCoreDLLs()
        {
            string[] dlls = {"LoaderDll.dll", "LocaleEmulator.dll"};

            return
                dlls.Select(dll => Path.Combine(RuntimeDirectory, dll))
                    .All(File.Exists);
        }
    }
}
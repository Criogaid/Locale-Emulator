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
        public static string GlobalVersionPath =
            Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location),
                         "LEVersion.xml");

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
                    Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location),
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
                dlls.Select(dll => Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), dll))
                    .All(File.Exists);
        }
    }
}
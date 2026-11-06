using System;
using Microsoft.Win32;

namespace LECommonLibrary
{
    public class AssociationReader
    {
        public static bool HaveAssociatedProgram(string ext)
        {
            if (GetAssociatedProgram(ext) == null)
                return false;

            return true;
        }

        public static string[] GetAssociatedProgram(string ext)
        {
            var prog = GetAssociatedProgramFromRegistry(ext);

            if (prog == null)
                return null;

            var jb = SplitExecutableAndArgument(prog);

            return jb;
        }

        public static string GetAssociatedIcon(string ext)
        {
            var prog = GetAssociatedIconFromRegistry(ext);

            if (prog == null)
                return null;

            return prog;
        }

        private static string GetAssociatedIconFromRegistry(string ext)
        {
            var d = (string)Registry.GetValue($"HKEY_CLASSES_ROOT\\{ext}", null, null);

            if (string.IsNullOrEmpty(d))
                return null;

            var prog =
                (string)Registry.GetValue($"HKEY_CLASSES_ROOT\\{d}\\DefaultIcon", null, null);

            return prog;
        }

        private static string[] SplitExecutableAndArgument(string line)
        {
            if (string.IsNullOrWhiteSpace(line))
                return null;

            var executableEnd = -1;
            var executable = line[0] == '"';
            if (executable)
            {
                executableEnd = line.IndexOf('"', 1);
                if (executableEnd <= 1)
                    return null;
            }
            else
            {
                var extension = line.IndexOf(".exe", StringComparison.OrdinalIgnoreCase);
                if (extension >= 0)
                    executableEnd = extension + ".exe".Length;
                else
                    executableEnd = line.IndexOf(' ');
            }

            if (executableEnd <= 0 || executableEnd >= line.Length - 1)
                return null;

            var executablePath = executable
                ? line.Substring(1, executableEnd - 1)
                : line.Substring(0, executableEnd);
            var arguments = line.Substring(executable ? executableEnd + 1 : executableEnd).TrimStart();
            return arguments.Length == 0 ? null : new[] { executablePath, arguments };
        }

        private static string GetAssociatedProgramFromRegistry(string ext)
        {
            var d = (string)Registry.GetValue($"HKEY_CLASSES_ROOT\\{ext}", null, null);

            if (string.IsNullOrEmpty(d))
                return null;

            var prog =
                (string)Registry.GetValue($"HKEY_CLASSES_ROOT\\{d}\\Shell\\Open\\Command", null, null);

            return prog;
        }
    }
}
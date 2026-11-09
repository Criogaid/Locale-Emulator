using System;
using System.Globalization;
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

        public static string GetAssociatedIcon(string ext, out int iconIndex)
        {
            iconIndex = 0;
            var value = GetAssociatedIconFromRegistry(ext);
            return value == null ? null : ParseAssociatedIcon(value, out iconIndex);
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
        private static string ParseAssociatedIcon(string value, out int iconIndex)
        {
            iconIndex = 0;
            value = value.Trim();
            if (value.Length == 0)
                return null;

            var separator = value.LastIndexOf(',');
            int parsedIndex;
            if (separator > 0 && int.TryParse(value.Substring(separator + 1).Trim(),
                NumberStyles.Integer, CultureInfo.InvariantCulture, out parsedIndex))
            {
                iconIndex = parsedIndex;
                value = value.Substring(0, separator).Trim();
            }

            if (value.Length >= 2 && value[0] == '"' && value[value.Length - 1] == '"')
                value = value.Substring(1, value.Length - 2);
            return value.Length == 0 ? null : value;
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

            if (executableEnd <= 0)
                return null;

            var executablePath = executable
                ? line.Substring(1, executableEnd - 1)
                : line.Substring(0, executableEnd);
            var arguments = line.Substring(executable ? executableEnd + 1 : executableEnd).TrimStart();
            return new[] { executablePath, arguments };
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
using System;
using System.Collections.Generic;
using System.IO;
using LECommonLibrary;

namespace LEProc
{
    internal sealed class LaunchPlan
    {
        internal string ApplicationName { get; private set; }
        internal string CommandLine { get; private set; }
        internal string CurrentDirectory { get; private set; }

        internal static LaunchPlan Create(string path, string rawParameters, IEnumerable<string> arguments)
        {
            if (string.IsNullOrWhiteSpace(path))
                throw new ArgumentException(Localization.GetString("MissingTargetPath"), "path");

            var absolutePath = Path.GetFullPath(path);
            if (!File.Exists(absolutePath))
                throw new FileNotFoundException(
                    string.Format(Localization.GetString("MissingTargetFile"), absolutePath), absolutePath);

            var parameters = GetParameters(rawParameters, arguments);
            return new LaunchPlan
            {
                ApplicationName = absolutePath,
                CommandLine = WindowsCommandLine.QuoteArgument(absolutePath) + " " + parameters,
                CurrentDirectory = Path.GetDirectoryName(absolutePath)
            };
        }

        internal static LaunchPlan CreateAssociated(string path, string applicationName, string template,
            string rawParameters, IEnumerable<string> arguments)
        {
            var plan = Create(path, rawParameters, arguments);
            plan.ApplicationName = applicationName.Trim('"');
            plan.CommandLine = WindowsCommandLine.QuoteArgument(plan.ApplicationName) + " "
                + ExpandAssociationTemplate(template, Path.GetFullPath(path), GetParameters(rawParameters, arguments));
            return plan;
        }

        private static string ExpandAssociationTemplate(string template, string path, string parameters)
        {
            var result = new System.Text.StringBuilder();
            for (var index = 0; index < template.Length;)
            {
                if (template[index] == '%' && index + 1 < template.Length)
                {
                    var placeholder = template[index + 1];
                    if (placeholder == '1' || placeholder == 'L' || placeholder == 'l')
                    {
                        result.Append(IsInsideQuotes(template, index)
                            ? path
                            : WindowsCommandLine.QuoteArgument(path));
                        index += 2;
                        continue;
                    }
                    if (placeholder == '*')
                    {
                        result.Append(parameters);
                        index += 2;
                        continue;
                    }
                }

                result.Append(template[index++]);
            }
            return result.ToString();
        }

        private static bool IsInsideQuotes(string template, int index)
        {
            var quoted = false;
            for (var cursor = 0; cursor < index; cursor++)
            {
                if (template[cursor] == '"' && (cursor == 0 || template[cursor - 1] != '\\'))
                    quoted = !quoted;
            }
            return quoted;
        }

        private static string GetParameters(string rawParameters, IEnumerable<string> arguments)
        {
            // 配置参数已经是完整命令行，不再分词或转义。
            return string.IsNullOrEmpty(rawParameters)
                ? WindowsCommandLine.JoinArguments(arguments)
                : rawParameters;
        }
    }
}

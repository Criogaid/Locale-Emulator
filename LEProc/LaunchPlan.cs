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
                throw new ArgumentException("请指定目标程序路径。", "path");

            var absolutePath = Path.GetFullPath(path);
            if (!File.Exists(absolutePath))
                throw new FileNotFoundException("找不到目标程序。", absolutePath);

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
                + template.Replace("%1", Path.GetFullPath(path))
                    .Replace("%*", GetParameters(rawParameters, arguments));
            return plan;
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

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

            // 配置参数已经是完整命令行，不再分词或转义。
            var parameters = string.IsNullOrEmpty(rawParameters)
                ? WindowsCommandLine.JoinArguments(arguments)
                : rawParameters;
            return new LaunchPlan
            {
                ApplicationName = absolutePath,
                CommandLine = WindowsCommandLine.QuoteArgument(absolutePath) + " " + parameters,
                CurrentDirectory = Path.GetDirectoryName(absolutePath)
            };
        }
    }
}

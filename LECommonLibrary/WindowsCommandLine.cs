using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace LECommonLibrary
{
    public static class WindowsCommandLine
    {
        /// <summary>
        /// 将单个参数编码为 Windows 命令行参数，保留空字符串、引号和反斜杠。
        /// </summary>
        public static string QuoteArgument(string argument)
        {
            if (argument == null)
                throw new ArgumentNullException("argument");
            if (argument.IndexOf('\0') >= 0)
                throw new ArgumentException(Localization.GetString("NullCharacterArgument"), "argument");

            var result = new StringBuilder("\"");
            var backslashes = 0;
            foreach (var character in argument)
            {
                if (character == '\\')
                {
                    backslashes++;
                    continue;
                }

                // 引号前的反斜杠必须加倍，并转义引号本身。
                result.Append('\\', character == '"' ? backslashes * 2 + 1 : backslashes);
                result.Append(character);
                backslashes = 0;
            }

            result.Append('\\', backslashes * 2);
            result.Append('"');
            return result.ToString();
        }

        /// <summary>
        /// 编码参数序列；配置中的原始命令行不应传入此方法。
        /// </summary>
        public static string JoinArguments(IEnumerable<string> arguments)
        {
            if (arguments == null)
                throw new ArgumentNullException("arguments");
            return string.Join(" ", arguments.Select(QuoteArgument));
        }
    }
}

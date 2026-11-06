using System;
using System.Text;

namespace ManagedRegression
{
    internal static class ShellRegressionHost
    {
        [STAThread]
        private static int Main()
        {
            Console.OutputEncoding = new UTF8Encoding(false);
            Console.WriteLine("Shell regression host: " + (IntPtr.Size * 8) + "-bit");
            var tests = ShellRegression.Tests;
            int failures = 0;
            foreach (var test in tests)
            {
                try
                {
                    test();
                    Console.WriteLine("PASS " + test.Method.Name);
                }
                catch (Exception error)
                {
                    failures++;
                    Console.Error.WriteLine("FAIL " + test.Method.Name + ": " + error);
                }
            }
            Console.WriteLine("Tests: " + tests.Length + ", failures: " + failures);
            return failures == 0 ? 0 : 1;
        }
    }
}

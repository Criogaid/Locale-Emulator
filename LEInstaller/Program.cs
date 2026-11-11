using System;
using System.Windows.Forms;

namespace LEInstaller
{
    internal static class Program
    {
        /// <summary>
        ///     The main entry point for the application.
        /// </summary>
        [STAThread]
        private static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Form1 form1=null;
            if (args.Length == 1)
            {
                if (args[0] == "--InstallAll")
                    form1 = new Form1(Form1.ModeInstallAll);
                else if (args[0] == "--UninstallAll")
                    form1 = new Form1(Form1.ModeUninstallAll);
                else if (args[0] == "--InstallUser")
                    form1 = new Form1(Form1.ModeInstallUser);
                else if (args[0] == "--UninstallUser")
                    form1 = new Form1(Form1.ModeUninstallUser);
                else
                    form1 = new Form1();
            }
            else
                form1 = new Form1();
            Application.Run(form1);

            

            
        }
    }
}
using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Xml;
using System.Xml.Linq;

namespace LECommonLibrary
{
    public static class LEConfig
    {
        public static string GlobalConfigPath =
            Path.Combine(GlobalHelper.ProductDirectory, "LEConfig.xml");


        public static LEProfile[] GetProfiles()
        {
            return GetProfiles(GlobalConfigPath);
        }

        // Runtime readers (launcher, shell menu) keep every profile that parses and skip malformed ones, so one
        // bad entry does not hide the rest. A file that is not valid XML yields no profiles.
        public static LEProfile[] GetProfiles(string configPath)
        {
            try
            {
                var profiles = new System.Collections.Generic.List<LEProfile>();
                foreach (var element in LoadProfileElements(configPath))
                {
                    try
                    {
                        profiles.Add(ParseProfile(element));
                    }
                    catch (Exception error) when (error is NullReferenceException || error is FormatException)
                    {
                    }
                }
                return profiles.ToArray();
            }
            catch
            {
                return new LEProfile[0];
            }
        }

        // Unlike GetProfiles this reports an unreadable file or profile, so callers that rewrite the file can
        // refuse to replace profiles they merely failed to parse.
        public static LEProfile[] ReadProfiles(string configPath)
        {
            return LoadProfileElements(configPath).Select(ParseProfile).ToArray();
        }

        private static XElement[] LoadProfileElements(string configPath)
        {
            var dict = XDocument.Load(configPath);
            return dict.Descendants("LEConfig").Elements("Profiles").Elements().ToArray();
        }

        private static LEProfile ParseProfile(XElement p)
        {
            return new LEProfile(p.Attribute("Name").Value,
                                 p.Attribute("Guid").Value,
                                 bool.Parse(p.Attribute("MainMenu").Value),
                                 p.Element("Parameter").Value,
                                 p.Element("Location").Value,
                                 p.Element("Timezone").Value,
                                 bool.Parse(p.TryGetValue("RunAsAdmin", "false")),
                                 bool.Parse(p.TryGetValue("RedirectRegistry", "true")),
                                 bool.Parse(p.TryGetValue("IsAdvancedRedirection", "false")),
                                 bool.Parse(p.TryGetValue("RunWithSuspend", "false")));
        }

        public static bool CheckGlobalConfigFile(bool buildNewConfig)
        {
            if (buildNewConfig && !File.Exists(GlobalConfigPath))
                BuildGlobalConfigFile();

            return File.Exists(GlobalConfigPath);
        }

        public static void SaveGlobalConfigFile(params LEProfile[] profiles)
        {
            WriteConfig(GlobalConfigPath, profiles);
        }

        public static void SaveApplicationConfigFile(string path, LEProfile profile)
        {
            WriteConfig(path, profile);
        }

        private static void BuildGlobalConfigFile()
        {
            // One profile per language; elevation is a property of the profile, not a second menu entry.
            var defaultProfiles = new[]
                                  {
                                      LEProfile.CreateDefault("Run in Japanese", false)
                                  };
            WriteConfig(GlobalConfigPath, defaultProfiles);
        }

        private static string TryGetValue(this XElement element, string name, string defaultValue = null)
        {
            var found = element.Element(name);
            if (found != null)
            {
                return found.Value;
            }
            return defaultValue;
        }

        private static void WriteConfig(string writeTo, params LEProfile[] profiles)
        {
            var baseNode = new XElement("Profiles");

            foreach (var pro in profiles)
            {
                baseNode.Add(new XElement("Profile",
                                          new XAttribute("Name", pro.Name),
                                          new XAttribute("Guid", pro.Guid),
                                          new XAttribute("MainMenu", pro.ShowInMainMenu),
                                          new XElement("Parameter", pro.Parameter),
                                          new XElement("Location", pro.Location),
                                          new XElement("Timezone", pro.Timezone),
                                          new XElement("RunAsAdmin", pro.RunAsAdmin),
                                          new XElement("RedirectRegistry", pro.RedirectRegistry),
                                          new XElement("IsAdvancedRedirection", pro.IsAdvancedRedirection),
                                          new XElement("RunWithSuspend", pro.RunWithSuspend)
                                 )
                    );
            }

            var tree = new XElement("LEConfig", baseNode);

            var destination = Path.GetFullPath(writeTo);
            var temporary = Path.Combine(Path.GetDirectoryName(destination), Path.GetRandomFileName());
            var created = false;
            try
            {
                using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                {
                    created = true;
                    try
                    {
                        using (var writer = XmlWriter.Create(stream, new XmlWriterSettings
                        {
                            Indent = true,
                            NewLineHandling = NewLineHandling.Entitize
                        }))
                            tree.Save(writer);
                    }
                    catch (ArgumentException error)
                    {
                        throw new InvalidDataException(Localization.GetString("InvalidXmlText"), error);
                    }
                    stream.Flush(true);
                }
                if (File.Exists(destination))
                    File.Replace(temporary, destination, null);
                else
                    File.Move(temporary, destination);
            }
            finally
            {
                if (created) File.Delete(temporary);
            }
        }
    }
}
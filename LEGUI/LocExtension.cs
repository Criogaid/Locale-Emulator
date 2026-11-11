using System;
using System.ComponentModel;
using System.Windows;
using System.Windows.Markup;

namespace LEGUI
{
    /// <summary>
    /// XAML access to the localized strings: <c>{legui:Loc Save}</c>. The value is converted to the
    /// target property's type (e.g. the per-language UI font name becomes a FontFamily).
    /// </summary>
    [MarkupExtensionReturnType(typeof(object))]
    public sealed class LocExtension : MarkupExtension
    {
        public LocExtension()
        {
        }

        public LocExtension(string key)
        {
            Key = key;
        }

        [ConstructorArgument("key")]
        public string Key { get; set; }

        public override object ProvideValue(IServiceProvider serviceProvider)
        {
            var text = I18n.GetString(Key);
            var target = serviceProvider.GetService(typeof(IProvideValueTarget)) as IProvideValueTarget;
            var property = target == null ? null : target.TargetProperty as DependencyProperty;
            if (property == null || property.PropertyType == typeof(object) || property.PropertyType == typeof(string))
                return text;
            return TypeDescriptor.GetConverter(property.PropertyType).ConvertFromInvariantString(text);
        }
    }
}

// Optional offline compatibility check; requires the user's own IRConfigurator.
// Never instantiate FormViewAdapter's constructor or initialize a device transport.
using System;
using System.Collections;
using System.Linq;
using System.Reflection;
using System.Runtime.Serialization;
using System.Windows.Forms;
class InteropCheck {
    static Assembly assembly;
    static Type configType;
    static object Load(string file) {
        var config = Activator.CreateInstance(configType);
        configType.GetMethod("Deserialize").Invoke(config, new object[] { file });
        return config;
    }
    static object Get(object item, string property) { return item.GetType().GetProperty(property).GetValue(item, null); }
    static byte[] Code(object item, int slot) {
        var toggle = assembly.GetType("IRConfigurator.Data.IRCodeToggle");
        return (byte[])item.GetType().GetMethod("GetIRCode").Invoke(item, new object[] { Enum.ToObject(toggle, slot) });
    }
    static byte[] Shortcuts(object config) {
        var field = configType.BaseType.GetField("menuFile", BindingFlags.Instance | BindingFlags.NonPublic);
        var menu = field.GetValue(config);
        return ((IEnumerable)Get(menu, "ShortcutIndex")).Cast<byte>().ToArray();
    }
    [STAThread] static int Main(string[] args) {
        if (args.Length != 3) { Console.Error.WriteLine("Usage: interop_check.exe IRConfigurator.exe before.RnIR3 after.RnIR3"); return 2; }
        try {
            assembly = Assembly.LoadFrom(args[0]);
            assembly.GetType("IRConfigurator.TextManager").GetMethod("Create", Type.EmptyTypes).Invoke(null, null);
            configType = assembly.GetType("IRConfigurator.Data.ProntoMenuConfig");
            var before = Load(args[1]); var after = Load(args[2]);
            var oldItems = (IList)Get(before, "MenuList"); var newItems = (IList)Get(after, "MenuList");
            if (oldItems.Count != newItems.Count || !Shortcuts(before).SequenceEqual(Shortcuts(after))) throw new Exception("Menu or shortcut count changed.");
            for (int i = 0; i < oldItems.Count; ++i) {
                foreach (var p in new[] { "Name", "Parent", "Flags", "TextIndex" }) if (!Equals(Get(oldItems[i], p), Get(newItems[i], p))) throw new Exception("Changed field at item " + i + ": " + p);
                for (int slot = 0; slot < 2; ++slot) if (!Code(oldItems[i], slot).SequenceEqual(Code(newItems[i], slot))) throw new Exception("Changed signal at item " + i);
            }
            var viewType = assembly.GetType("IRConfigurator.FormViewAdapter");
            var view = FormatterServices.GetUninitializedObject(viewType);
            viewType.GetField("currentMenu", BindingFlags.Instance | BindingFlags.NonPublic).SetValue(view, after);
            viewType.GetField("currentVersion", BindingFlags.Instance | BindingFlags.NonPublic).SetValue(view, Get(after, "Version"));
            using (var tree = new TreeView()) using (var shortcuts = new ListView()) viewType.GetMethod("UpdateOmniView").Invoke(view, new object[] { tree, shortcuts });
            Console.WriteLine("PASS: " + newItems.Count + " items, both signal slots and all shortcuts preserved; original GUI tree built offline.");
            return 0;
        } catch (Exception e) { Console.Error.WriteLine(e); return 1; }
    }
}

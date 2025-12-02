using System;
using System.Collections.Generic;
using System.IO;
using Microsoft.Win32;

namespace StartClean;

public interface IStartupProvider
{
    IEnumerable<StartupEntry> GetEntries();
    void Add(string name, string targetPath);
    void Enable(string name);
    void Disable(string name);
    void Delete(string name);
}

/// <summary>
/// Handles Run / RunOnce style registry entries. Uses a shadow subkey to store disabled items.
/// </summary>
public sealed class RegistryStartupProvider : IStartupProvider
{
    private readonly RegistryKey _baseKey;
    private readonly bool _isMachine;
    private const string RunPath = @"SOFTWARE\Microsoft\Windows\CurrentVersion\Run";
    private const string DisabledKeyName = "StartCleanDisabled";

    public RegistryStartupProvider(RegistryKey baseKey, bool isMachine)
    {
        _baseKey = baseKey;
        _isMachine = isMachine;
    }

    public bool IsMachine => _isMachine;

    public IEnumerable<StartupEntry> GetEntries()
    {
        foreach (var entry in ReadEntries(RunPath, enabled: true))
            yield return entry;

        foreach (var entry in ReadEntries($"{RunPath}\\{DisabledKeyName}", enabled: false))
            yield return entry;
    }

    public void Add(string name, string targetPath)
    {
        using var key = _baseKey.CreateSubKey(RunPath, writable: true);
        key?.SetValue(name, targetPath);
    }

    public void Enable(string name) => MoveValue($"{RunPath}\\{DisabledKeyName}", RunPath, name);

    public void Disable(string name) => MoveValue(RunPath, $"{RunPath}\\{DisabledKeyName}", name);

    public void Delete(string name)
    {
        using var enabled = _baseKey.CreateSubKey(RunPath, writable: true);
        enabled?.DeleteValue(name, throwOnMissingValue: false);

        using var disabled = _baseKey.CreateSubKey($"{RunPath}\\{DisabledKeyName}", writable: true);
        disabled?.DeleteValue(name, throwOnMissingValue: false);
    }

    private IEnumerable<StartupEntry> ReadEntries(string subKey, bool enabled)
    {
        using var key = _baseKey.OpenSubKey(subKey, writable: false);
        if (key == null)
            yield break;

        foreach (var valueName in key.GetValueNames())
        {
            var value = key.GetValue(valueName)?.ToString() ?? string.Empty;
            var publisher = PathResolver.GetPublisher(value) ?? string.Empty;
            yield return new StartupEntry
            {
                Name = valueName,
                TargetPath = value,
                Enabled = enabled,
                Source = _isMachine ? StartupSource.RegistryLocalMachine : StartupSource.RegistryCurrentUser,
                Location = $"{_baseKey.Name}\\{subKey}",
                RequiresElevation = _isMachine,
                Publisher = publisher,
                StartupImpact = "None"
            };
        }
    }

    private void MoveValue(string fromSubKey, string toSubKey, string name)
    {
        using var from = _baseKey.CreateSubKey(fromSubKey, writable: true);
        using var to = _baseKey.CreateSubKey(toSubKey, writable: true);
        if (from == null || to == null)
            return;

        var value = from.GetValue(name);
        if (value == null)
            return;

        to.SetValue(name, value);
        from.DeleteValue(name, throwOnMissingValue: false);
    }
}

/// <summary>
/// Handles .lnk/.exe shortcuts in startup folders. Disabled items are renamed with ".disabled".
/// </summary>
public sealed class StartupFolderProvider : IStartupProvider
{
    private readonly string _folderPath;
    private readonly bool _isCommon;
    private const string DisabledSuffix = ".disabled";

    public StartupFolderProvider(string folderPath, bool isCommon)
    {
        _folderPath = folderPath;
        _isCommon = isCommon;
    }

    public bool IsCommon => _isCommon;

    public IEnumerable<StartupEntry> GetEntries()
    {
        if (!Directory.Exists(_folderPath))
            yield break;

        foreach (var file in Directory.GetFiles(_folderPath))
        {
            var enabled = !file.EndsWith(DisabledSuffix, StringComparison.OrdinalIgnoreCase);
            var actualPath = enabled ? file : file[..^DisabledSuffix.Length];
            var ext = Path.GetExtension(actualPath);
            if (!IsSupportedExtension(ext))
                continue;

            var displayName = Path.GetFileNameWithoutExtension(actualPath);
            var fullTarget = actualPath;
            var publisher = PathResolver.GetPublisher(fullTarget) ?? string.Empty;

            yield return new StartupEntry
            {
                Name = displayName,
                TargetPath = fullTarget,
                Enabled = enabled,
                Source = _isCommon ? StartupSource.StartupFolderCommon : StartupSource.StartupFolderUser,
                Location = _folderPath,
                RequiresElevation = _isCommon,
                Publisher = publisher,
                StartupImpact = "None"
            };
        }
    }

    public void Add(string name, string targetPath)
    {
        Directory.CreateDirectory(_folderPath);
        var linkName = Path.Combine(_folderPath, $"{name}.lnk");
        CreateShortcut(linkName, targetPath);
    }

    public void Enable(string name)
    {
        var file = FindFile(name);
        if (file == null)
            return;
        if (!file.EndsWith(DisabledSuffix, StringComparison.OrdinalIgnoreCase))
            return;
        var newName = file[..^DisabledSuffix.Length];
        File.Move(file, newName, overwrite: false);
    }

    public void Disable(string name)
    {
        var file = FindFile(name);
        if (file == null)
            return;
        if (file.EndsWith(DisabledSuffix, StringComparison.OrdinalIgnoreCase))
            return;
        File.Move(file, file + DisabledSuffix, overwrite: false);
    }

    public void Delete(string name)
    {
        var file = FindFile(name);
        if (file != null && File.Exists(file))
            File.Delete(file);
    }

    private string? FindFile(string name)
    {
        if (!Directory.Exists(_folderPath))
            return null;

        foreach (var file in Directory.GetFiles(_folderPath))
        {
            var currentName = Path.GetFileNameWithoutExtension(file);
            if (currentName.Equals(name, StringComparison.OrdinalIgnoreCase))
                return file;

            if (file.EndsWith(DisabledSuffix, StringComparison.OrdinalIgnoreCase))
            {
                var trimmed = Path.GetFileNameWithoutExtension(file[..^DisabledSuffix.Length]);
                if (trimmed.Equals(name, StringComparison.OrdinalIgnoreCase))
                    return file;
            }
        }

        return null;
    }

    private static bool IsSupportedExtension(string ext)
    {
        if (string.IsNullOrEmpty(ext))
            return false;
        return ext.Equals(".lnk", StringComparison.OrdinalIgnoreCase)
               || ext.Equals(".exe", StringComparison.OrdinalIgnoreCase)
               || ext.Equals(".bat", StringComparison.OrdinalIgnoreCase)
               || ext.Equals(".cmd", StringComparison.OrdinalIgnoreCase)
               || ext.Equals(".ps1", StringComparison.OrdinalIgnoreCase);
    }

    private static void CreateShortcut(string shortcutPath, string targetPath)
    {
        // Uses WScript.Shell to avoid COM interop boilerplate elsewhere.
        var shellType = Type.GetTypeFromProgID("WScript.Shell");
        if (shellType == null)
            return;

        dynamic? shell = Activator.CreateInstance(shellType);
        if (shell == null)
            return;

        dynamic shortcut = shell.CreateShortcut(shortcutPath);
        shortcut.TargetPath = targetPath;
        shortcut.Save();
    }
}

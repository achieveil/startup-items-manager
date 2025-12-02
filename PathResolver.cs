using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Diagnostics;

namespace StartClean;

/// <summary>
/// Utility to resolve executable path from command lines or shortcuts.
/// </summary>
public static class PathResolver
{
    public static string? ResolveExecutable(string rawTarget)
    {
        if (string.IsNullOrWhiteSpace(rawTarget))
            return null;

        // If it's a shortcut file, resolve to its target.
        var trimmed = rawTarget.Trim();
        if (File.Exists(trimmed) && Path.GetExtension(trimmed).Equals(".lnk", StringComparison.OrdinalIgnoreCase))
        {
            var resolved = ResolveShortcut(trimmed);
            if (!string.IsNullOrWhiteSpace(resolved))
                return resolved;
        }

        // Use CommandLineToArgvW to reliably split the first token.
        var first = GetFirstArgument(trimmed);
        if (string.IsNullOrWhiteSpace(first))
            return null;

        // Remove surrounding quotes.
        first = first.Trim().Trim('"');
        return first;
    }

    public static string? GetPublisher(string rawTarget)
    {
        try
        {
            var exePath = ResolveExecutable(rawTarget);
            if (string.IsNullOrWhiteSpace(exePath) || !File.Exists(exePath))
                return null;

            var info = FileVersionInfo.GetVersionInfo(exePath);
            return info.CompanyName;
        }
        catch
        {
            return null;
        }
    }

    private static string? ResolveShortcut(string shortcutPath)
    {
        try
        {
            var shellType = Type.GetTypeFromProgID("WScript.Shell");
            if (shellType == null)
                return null;

            dynamic? shell = Activator.CreateInstance(shellType);
            if (shell == null)
                return null;

            dynamic shortcut = shell.CreateShortcut(shortcutPath);
            return shortcut.TargetPath as string;
        }
        catch
        {
            return null;
        }
    }

    private static string? GetFirstArgument(string commandLine)
    {
        IntPtr argv = IntPtr.Zero;
        try
        {
            int argc;
            argv = CommandLineToArgvW(commandLine, out argc);
            if (argv == IntPtr.Zero || argc == 0)
                return null;
            var firstPtr = Marshal.ReadIntPtr(argv);
            return Marshal.PtrToStringUni(firstPtr);
        }
        finally
        {
            if (argv != IntPtr.Zero)
                Marshal.FreeHGlobal(argv);
        }
    }

    [DllImport("shell32.dll", SetLastError = true)]
    private static extern IntPtr CommandLineToArgvW([MarshalAs(UnmanagedType.LPWStr)] string lpCmdLine, out int pNumArgs);
}

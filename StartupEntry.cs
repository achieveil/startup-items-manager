using System;

namespace StartClean;

/// <summary>
/// Represents one startup item from registry or startup folder.
/// </summary>
public class StartupEntry
{
    public required string Name { get; init; }
    public required string TargetPath { get; init; }
    public required StartupSource Source { get; init; }
    public required string Location { get; init; }
    public string Publisher { get; init; } = string.Empty;
    public string StartupImpact { get; init; } = "None";
    public bool Enabled { get; set; }
    public bool RequiresElevation { get; init; }

    public override string ToString() => $"{Name} ({Source})";

    public string StatusText => Enabled ? "Enabled" : "Disabled";
}

public enum StartupSource
{
    RegistryCurrentUser,
    RegistryLocalMachine,
    StartupFolderUser,
    StartupFolderCommon
}

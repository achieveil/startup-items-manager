using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Security.Principal;
using System.Windows.Forms;
using Microsoft.Win32;
using System.ComponentModel;

namespace StartClean;

public partial class MainForm : Form
{
    private readonly List<IStartupProvider> _providers;
    private readonly BindingSource _binding = new();
    private readonly Dictionary<string, Image> _iconCache = new(StringComparer.OrdinalIgnoreCase);

    // --- 配色 ---
    private readonly Color _windowBgColor = Color.FromArgb(243, 243, 243); // 窗体背景
    private readonly Color _cardBgColor = Color.White; // 列表背景
    private readonly Color _primaryColor = Color.FromArgb(0, 103, 192); // 品牌蓝
    private readonly Color _toggleOnColor = Color.FromArgb(16, 124, 16); // 开关绿
    private readonly Color _toggleOffColor = Color.FromArgb(205, 205, 205); // 开关灰
    private readonly Color _dividerColor = Color.FromArgb(230, 230, 230); // 极淡的分割线
    private readonly Color _headerTextColor = Color.FromArgb(120, 120, 120); // 表头文字灰

    private RegistryStartupProvider _userRegistryProvider = null!;
    private RegistryStartupProvider _machineRegistryProvider = null!;
    private StartupFolderProvider _userFolderProvider = null!;
    private StartupFolderProvider _commonFolderProvider = null!;

    public MainForm()
    {
        InitializeComponent();

        _providers = BuildProviders();
        TrySetIconFromExe();

        // 1. 初始化界面结构 
        SetupLayoutAndWrappers();

        // 2. 配置表格列
        SetupGrid();

        // 3. 应用样式
        ApplyModernStyles();

        grid.SelectionChanged += grid_SelectionChanged;
    }

    /// <summary>
    /// 使用 Wrapper Panel 强制实现悬浮卡片效果
    /// </summary>
    private void SetupLayoutAndWrappers()
    {
        this.BackColor = _windowBgColor;
        this.Font = new Font("Segoe UI", 9.5F, FontStyle.Regular);

        // 处理顶部工具栏
        if (toolbarPanel != null)
        {
            toolbarPanel.BackColor = _windowBgColor;
            toolbarPanel.Padding = new Padding(25, 15, 25, 10); // 增加边距
            // 确保面板有足够的高度
            if (toolbarPanel.Height < 65) toolbarPanel.Height = 65;
            int btnHeight = enableButton.Height;
            // 2. 计算居中的 Y 坐标：(面板高度 - 按钮高度) / 2
            int centerY = (toolbarPanel.Height - btnHeight) / 2;
            // 3. 将所有左侧按钮移动到该 Y 坐标
            Control[] leftButtons = { enableButton, disableButton, deleteButton, addButton, openButton };
            foreach (var btn in leftButtons)
            {
                btn.Location = new Point(btn.Location.X, centerY);
            }
            refreshButton.Location = new Point(refreshButton.Location.X, centerY);
            refreshButton.Height = btnHeight; // 确保高度也一致
        }
        // 创建一个容器把 Grid 包起来... (下方代码保持不变)
        if (grid.Parent != null)
        {
            var parent = grid.Parent;
            var wrapper = new Panel
            {
                Dock = grid.Dock,
                Padding = new Padding(25, 0, 25, 25),
                BackColor = _windowBgColor
            };
            int index = parent.Controls.GetChildIndex(grid);
            parent.Controls.Add(wrapper);
            parent.Controls.SetChildIndex(wrapper, index);
            parent.Controls.Remove(grid);
            wrapper.Controls.Add(grid);
            grid.Dock = DockStyle.Fill;
        }

        // 3. 设置 Grid 背景
        grid.BackgroundColor = _cardBgColor;
        grid.BorderStyle = BorderStyle.None;
    }
    private void SetupGrid()
    {
        grid.AutoGenerateColumns = false;
        grid.Columns.Clear();
        // 图标列
        grid.Columns.Add(new DataGridViewImageColumn
        {
            Name = "Icon",
            HeaderText = "",
            Width = 56,
            AutoSizeMode = DataGridViewAutoSizeColumnMode.None,
            ImageLayout = DataGridViewImageCellLayout.Zoom
        });

        // 名称
        grid.Columns.Add(new DataGridViewTextBoxColumn
        {
            DataPropertyName = "Name",
            Name = "Name",
            HeaderText = "名称",
            AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill,
            FillWeight = 45
        });

        // 发布者
        grid.Columns.Add(new DataGridViewTextBoxColumn
        {
            DataPropertyName = "Publisher",
            Name = "Publisher",
            HeaderText = "发布者",
            AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill,
            FillWeight = 40
        });

        // 状态开关
        grid.Columns.Add(new DataGridViewTextBoxColumn
        {
            Name = "StatusToggle",
            HeaderText = "状态",
            Width = 90,
            AutoSizeMode = DataGridViewAutoSizeColumnMode.None,
            ReadOnly = true
        });

        grid.Columns.Add(new DataGridViewTextBoxColumn { DataPropertyName = "Source", Visible = false });
        grid.Columns.Add(new DataGridViewTextBoxColumn { DataPropertyName = "Location", Visible = false });

        grid.DataSource = _binding;
        grid.CellPainting += grid_CellPainting;
        grid.CellFormatting += grid_CellFormatting;
        grid.CellMouseClick += grid_CellMouseClick;
        grid.CellDoubleClick += grid_CellDoubleClick;
        grid.DataError += grid_DataError;
    }

    private void ApplyModernStyles()
    {
        // 禁用所有默认边框
        grid.CellBorderStyle = DataGridViewCellBorderStyle.None; // 关键：禁用所有默认线条
        grid.ColumnHeadersBorderStyle = DataGridViewHeaderBorderStyle.None;
        grid.EnableHeadersVisualStyles = false;

        // 表头样式
        grid.ColumnHeadersHeight = 45;
        grid.ColumnHeadersDefaultCellStyle.BackColor = _cardBgColor;
        grid.ColumnHeadersDefaultCellStyle.ForeColor = _headerTextColor;
        grid.ColumnHeadersDefaultCellStyle.Font = new Font("Segoe UI Semibold", 10F);
        grid.ColumnHeadersDefaultCellStyle.Padding = new Padding(12, 0, 0, 0);
        grid.ColumnHeadersDefaultCellStyle.SelectionBackColor = _cardBgColor; // 选中时不变色

        // 行样式
        grid.RowHeadersVisible = false;
        grid.RowTemplate.Height = 58; // 更大的行高
        grid.DefaultCellStyle.BackColor = _cardBgColor;
        grid.DefaultCellStyle.ForeColor = Color.FromArgb(60, 60, 60);
        grid.DefaultCellStyle.Padding = new Padding(12, 0, 0, 0);
        grid.DefaultCellStyle.Font = new Font("Segoe UI", 10.5F);
        grid.DefaultCellStyle.SelectionBackColor = Color.FromArgb(242, 246, 250); // 极淡的选中背景
        grid.DefaultCellStyle.SelectionForeColor = Color.Black;

        // 按钮圆角化
        StyleButton(enableButton, false);
        StyleButton(disableButton, false);
        StyleButton(deleteButton, false);
        StyleButton(openButton, false);
        StyleButton(refreshButton, false);
        StyleButton(addButton, true);
    }

    // --- 完全接管绘制 (Nuclear Painting) ---

    private void grid_CellPainting(object? sender, DataGridViewCellPaintingEventArgs e)
    {
        if (e.Graphics == null)
            return;

        var style = e.CellStyle ?? grid.DefaultCellStyle;
        var font = style.Font ?? grid.Font;
        var foreColor = style.ForeColor.IsEmpty ? Color.Black : style.ForeColor;

        if (e.RowIndex < 0 && e.ColumnIndex >= 0)
        {
            // --- 绘制表头 (Header) ---
            e.PaintBackground(e.CellBounds, false);

            // 画文字
            var headerText = e.FormattedValue?.ToString();
            if (!string.IsNullOrEmpty(headerText))
            {
                TextRenderer.DrawText(e.Graphics, headerText, font, e.CellBounds, foreColor,
                    TextFormatFlags.VerticalCenter | TextFormatFlags.Left | TextFormatFlags.LeftAndRightPadding);
            }

            // 画底部分割线
            using var p = new Pen(_dividerColor);
            e.Graphics.DrawLine(p, e.CellBounds.Left, e.CellBounds.Bottom - 1, e.CellBounds.Right, e.CellBounds.Bottom - 1);

            e.Handled = true;
        }
        else if (e.RowIndex >= 0 && e.ColumnIndex >= 0)
        {
            if (_binding.Count <= e.RowIndex)
            {
                e.Handled = true;
                return;
            }
            // --- 绘制普通行 ---

            // 1. 绘制背景 (处理选中状态)
            bool isSelected = (e.State & DataGridViewElementStates.Selected) != 0;
            var selectionBack = style.SelectionBackColor.IsEmpty ? grid.DefaultCellStyle.SelectionBackColor : style.SelectionBackColor;
            var normalBack = style.BackColor.IsEmpty ? grid.DefaultCellStyle.BackColor : style.BackColor;
            var backColor = isSelected ? selectionBack : normalBack;
            using (var b = new SolidBrush(backColor))
            {
                e.Graphics.FillRectangle(b, e.CellBounds);
            }

            // 2. 绘制特定的列内容
            if (grid.Columns[e.ColumnIndex].Name == "StatusToggle")
            {
                // 画开关
                DrawSwitch(e.Graphics, e.CellBounds, _binding[e.RowIndex] as StartupEntry);
            }
            else if (grid.Columns[e.ColumnIndex].Name == "Icon")
            {
                // 画图标
                e.PaintContent(e.CellBounds);
            }
            else
            {
                // 画普通文字
                if (e.Value != null)
                {
                    TextRenderer.DrawText(e.Graphics, e.Value.ToString(),
                        font, e.CellBounds, foreColor,
                        TextFormatFlags.VerticalCenter | TextFormatFlags.Left | TextFormatFlags.LeftAndRightPadding);
                }
            }

            // 3. 手动绘制底部分割线
            using (var p = new Pen(_dividerColor))
            {
                e.Graphics.DrawLine(p, e.CellBounds.Left, e.CellBounds.Bottom - 1, e.CellBounds.Right, e.CellBounds.Bottom - 1);
            }

            e.Handled = true; // 告诉系统我们画完了，不要再画那些丑陋的默认线条
        }
    }

    private void DrawSwitch(Graphics g, Rectangle bounds, StartupEntry? entry)
    {
        bool isOn = entry?.Enabled ?? false;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        int w = 40, h = 22;
        var rect = new Rectangle(bounds.X + (bounds.Width - w) / 2, bounds.Y + (bounds.Height - h) / 2, w, h);

        // 轨道
        using (var path = GetRoundedPath(rect, 11))
        using (var brush = new SolidBrush(isOn ? _toggleOnColor : _toggleOffColor))
        {
            g.FillPath(brush, path);
        }

        // 滑块圆点
        int knobSize = 18;
        int knobX = isOn ? (rect.Right - knobSize - 2) : (rect.X + 2);
        int knobY = rect.Y + (rect.Height - knobSize) / 2;
        g.FillEllipse(Brushes.White, knobX, knobY, knobSize, knobSize);
    }

    private GraphicsPath GetRoundedPath(Rectangle rect, int radius)
    {
        GraphicsPath path = new GraphicsPath();
        int d = radius * 2;
        path.AddArc(rect.X, rect.Y, d, d, 180, 90);
        path.AddArc(rect.Right - d, rect.Y, d, d, 270, 90);
        path.AddArc(rect.Right - d, rect.Bottom - d, d, d, 0, 90);
        path.AddArc(rect.X, rect.Bottom - d, d, d, 90, 90);
        path.CloseFigure();
        return path;
    }

    // --- 按钮样式 ---
    private void StyleButton(Button btn, bool isPrimary)
    {
        btn.FlatStyle = FlatStyle.Flat;
        btn.FlatAppearance.BorderSize = 0;
        btn.BackColor = Color.Transparent;
        btn.Text = btn.Text.Trim();
        btn.Font = new Font("Segoe UI", 9F);
        btn.Cursor = Cursors.Hand;

        btn.Paint += (s, e) =>
        {
            var g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            var rect = new Rectangle(0, 0, btn.Width - 1, btn.Height - 1);

            var bg = isPrimary ? _primaryColor : Color.White;
            var border = isPrimary ? _primaryColor : Color.FromArgb(220, 220, 220);
            var text = isPrimary ? Color.White : Color.Black;

            if (btn.ClientRectangle.Contains(btn.PointToClient(Cursor.Position)))
                bg = isPrimary ? Color.FromArgb(0, 90, 170) : Color.FromArgb(248, 248, 248);

            using var path = GetRoundedPath(rect, 6);
            using var brush = new SolidBrush(bg);
            using var pen = new Pen(border, 1);

            g.FillPath(brush, path);
            g.DrawPath(pen, path);
            TextRenderer.DrawText(g, btn.Text, btn.Font, rect, text, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
        };
    }

    // --- 事件处理逻辑 ---
    private void grid_CellMouseClick(object? sender, DataGridViewCellMouseEventArgs e)
    {
        if (e.RowIndex >= 0 && e.ColumnIndex >= 0 && grid.Columns[e.ColumnIndex].Name == "StatusToggle")
        {
            var entry = _binding.Count > e.RowIndex ? _binding[e.RowIndex] as StartupEntry : null;
            if (entry != null)
            {
                if (entry.Enabled) PerformAction("禁用", p => p.Disable(entry.Name));
                else PerformAction("启用", p => p.Enable(entry.Name));
            }
        }
    }

    private List<IStartupProvider> BuildProviders()
    {
        _userRegistryProvider = new RegistryStartupProvider(RegistryKey.OpenBaseKey(RegistryHive.CurrentUser, RegistryView.Registry64), false);
        _machineRegistryProvider = new RegistryStartupProvider(RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64), true);
        _userFolderProvider = new StartupFolderProvider(Environment.GetFolderPath(Environment.SpecialFolder.Startup), false);
        _commonFolderProvider = new StartupFolderProvider(Environment.GetFolderPath(Environment.SpecialFolder.CommonStartup), true);
        return new List<IStartupProvider> { _userRegistryProvider, _machineRegistryProvider, _userFolderProvider, _commonFolderProvider };
    }

    private StartupEntry? CurrentEntry => _binding.Current as StartupEntry;
    private void MainForm_Load(object? sender, EventArgs e) => RefreshEntries();
    private void refreshButton_Click(object? sender, EventArgs e) => RefreshEntries();
    private void enableButton_Click(object? sender, EventArgs e) => PerformAction("启用", p => p.Enable(CurrentEntry!.Name));
    private void disableButton_Click(object? sender, EventArgs e) => PerformAction("禁用", p => p.Disable(CurrentEntry!.Name));
    private void deleteButton_Click(object? sender, EventArgs e)
    {
        if (CurrentEntry != null && MessageBox.Show($"删除启动项 {CurrentEntry.Name}?", "确认", MessageBoxButtons.YesNo) == DialogResult.Yes)
            PerformAction("删除", p => p.Delete(CurrentEntry.Name));
    }

    private void addButton_Click(object? sender, EventArgs e)
    {
        using var dialog = new AddEntryDialog();
        if (dialog.ShowDialog(this) == DialogResult.OK && !string.IsNullOrWhiteSpace(dialog.EntryName))
        {
            TryRun(() => _userRegistryProvider.Add(dialog.EntryName, dialog.EntryPath), "添加");
            RefreshEntries();
        }
    }

    private void openButton_Click(object? sender, EventArgs e)
    {
        if (CurrentEntry == null) return;
        try
        {
            Process.Start(new ProcessStartInfo("explorer.exe", $"/select,\"{PathResolver.ResolveExecutable(CurrentEntry.TargetPath)}\"") { UseShellExecute = true });
        }
        catch { }
    }

    private void grid_CellDoubleClick(object? sender, DataGridViewCellEventArgs e)
    {
        if (e.RowIndex >= 0 && e.ColumnIndex >= 0 && grid.Columns[e.ColumnIndex].Name != "StatusToggle")
            openButton_Click(sender, e);
    }

    private void grid_DataError(object? sender, DataGridViewDataErrorEventArgs e)
    {
        e.ThrowException = false;
        e.Cancel = true;
    }

    private void RefreshEntries()
    {
        int idx = grid.SelectedRows.Count > 0 ? grid.SelectedRows[0].Index : -1;
        TryRun(() =>
        {
            var entries = _providers.SelectMany(p => p.GetEntries()).OrderBy(e => e.Name).ToList();
            _binding.DataSource = new BindingList<StartupEntry>(entries);
            UpdateButtonsState();
        }, "刷新");
        if (idx >= 0 && idx < grid.Rows.Count)
            grid.Rows[idx].Selected = true;
        else
            grid.ClearSelection();
    }

    private void PerformAction(string name, Action<IStartupProvider> action)
    {
        if (CurrentEntry == null) return;
        var p = FindProvider(CurrentEntry);
        if (p != null) { TryRun(() => action(p), name); RefreshEntries(); }
    }

    private void grid_SelectionChanged(object? sender, EventArgs e) => UpdateButtonsState();

    private void UpdateButtonsState()
    {
        var entry = CurrentEntry;
        enableButton.Enabled = entry != null && !entry.Enabled;
        disableButton.Enabled = entry != null && entry.Enabled;
    }

    private void grid_CellFormatting(object? sender, DataGridViewCellFormattingEventArgs e)
    {
        if (e.RowIndex < 0 || e.ColumnIndex < 0)
            return;
        if (_binding.Count <= e.RowIndex)
            return;
        if (grid.Columns[e.ColumnIndex].Name == "Icon" && _binding[e.RowIndex] is StartupEntry entry)
            e.Value = GetIconForEntry(entry);
    }

    private Image? GetIconForEntry(StartupEntry entry)
    {
        var path = PathResolver.ResolveExecutable(entry.TargetPath);
        if (string.IsNullOrWhiteSpace(path) || !File.Exists(path)) return null;
        if (_iconCache.TryGetValue(path, out var cached)) return cached;
        try { using var icon = Icon.ExtractAssociatedIcon(path); if (icon != null) { var bmp = icon.ToBitmap(); _iconCache[path] = bmp; return bmp; } } catch { }
        return null;
    }

    private IStartupProvider? FindProvider(StartupEntry entry) => entry.Source switch
    {
        StartupSource.RegistryCurrentUser => _userRegistryProvider,
        StartupSource.RegistryLocalMachine => _machineRegistryProvider,
        StartupSource.StartupFolderUser => _userFolderProvider,
        StartupSource.StartupFolderCommon => _commonFolderProvider,
        _ => null
    };

    private void TryRun(Action action, string name)
    {
        try
        {
            action();
            SetStatus($"{name}完成");
        }
        catch (UnauthorizedAccessException ex)
        {
            MessageBox.Show($"{ex.Message}\n\n请以管理员身份重新运行后再试。", "权限不足", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            SetStatus($"{name}失败：权限不足");
        }
        catch (Exception ex)
        {
            MessageBox.Show(ex.Message, "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            SetStatus($"{name}失败");
        }
    }
    private void SetStatus(string text) { if (statusLabel != null) statusLabel.Text = text; }

    private void TrySetIconFromExe()
    {
        try
        {
            using var icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
            if (icon != null)
                this.Icon = (Icon)icon.Clone();
        }
        catch
        {
            // ignore icon failures
        }
    }
}

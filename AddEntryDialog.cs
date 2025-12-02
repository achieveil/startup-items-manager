using System;
using System.Drawing;
using System.Windows.Forms;

namespace StartClean;

/// <summary>
/// Minimal dialog to collect startup entry name and target path.
/// </summary>
public sealed class AddEntryDialog : Form
{
    private readonly TextBox _nameTextBox;
    private readonly TextBox _pathTextBox;

    public string EntryName => _nameTextBox.Text.Trim();
    public string EntryPath => _pathTextBox.Text.Trim();

    public AddEntryDialog()
    {
        Text = "添加启动项（当前用户）";
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = false;
        StartPosition = FormStartPosition.CenterParent;
        AutoScaleMode = AutoScaleMode.Font;
        Font = new Font("Segoe UI", 10F);
        Padding = new Padding(14, 12, 14, 12);
        ClientSize = new Size(540, 210);

        var table = new TableLayoutPanel
        {
            ColumnCount = 3,
            RowCount = 2,
            Dock = DockStyle.Top,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Padding = new Padding(0, 0, 0, 14)
        };
        table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 95));
        table.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));
        table.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));

        var nameLabel = new Label { Text = "名称：", AutoSize = true, Anchor = AnchorStyles.Left, TextAlign = ContentAlignment.MiddleLeft, Margin = new Padding(0, 4, 6, 4) };
        _nameTextBox = new TextBox { Dock = DockStyle.Fill, Margin = new Padding(0, 4, 0, 4) };

        var pathLabel = new Label { Text = "目标路径：", AutoSize = true, Anchor = AnchorStyles.Left, TextAlign = ContentAlignment.MiddleLeft, Margin = new Padding(0, 4, 6, 4) };
        _pathTextBox = new TextBox { Dock = DockStyle.Fill, Margin = new Padding(0, 4, 0, 4) };

        var browseButton = new Button
        {
            Text = "浏览...",
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Dock = DockStyle.Fill,
            Margin = new Padding(8, 2, 0, 2),
            MinimumSize = new Size(80, 30)
        };
        browseButton.Click += BrowseButton_Click;

        table.Controls.Add(nameLabel, 0, 0);
        table.Controls.Add(_nameTextBox, 1, 0);
        table.SetColumnSpan(_nameTextBox, 2);
        table.Controls.Add(pathLabel, 0, 1);
        table.Controls.Add(_pathTextBox, 1, 1);
        table.Controls.Add(browseButton, 2, 1);

        var buttonPanel = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.RightToLeft,
            Dock = DockStyle.Bottom,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Padding = new Padding(0),
            Margin = new Padding(0)
        };

        var okButton = new Button { Text = "确定", DialogResult = DialogResult.OK, Width = 96, Height = 32, Margin = new Padding(8, 10, 0, 0) };
        var cancelButton = new Button { Text = "取消", DialogResult = DialogResult.Cancel, Width = 96, Height = 32, Margin = new Padding(8, 10, 0, 0) };
        buttonPanel.Controls.Add(cancelButton);
        buttonPanel.Controls.Add(okButton);

        AcceptButton = okButton;
        CancelButton = cancelButton;

        Controls.AddRange(new Control[]
        {
            table, buttonPanel
        });
    }

    private void BrowseButton_Click(object? sender, EventArgs e)
    {
        using var dialog = new OpenFileDialog
        {
            Filter = "可执行文件|*.exe;*.bat;*.cmd;*.ps1;*.lnk|所有文件|*.*",
            CheckFileExists = true,
            Title = "选择要添加的程序"
        };

        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            _pathTextBox.Text = dialog.FileName;
            if (string.IsNullOrWhiteSpace(_nameTextBox.Text))
                _nameTextBox.Text = System.IO.Path.GetFileNameWithoutExtension(dialog.SafeFileName);
        }
    }
}

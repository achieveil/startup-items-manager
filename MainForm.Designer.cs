namespace StartClean;

partial class MainForm
{
    /// <summary>
    ///  Required designer variable.
    /// </summary>
    private System.ComponentModel.IContainer components = null;

    /// <summary>
    ///  Clean up any resources being used.
    /// </summary>
    /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
    protected override void Dispose(bool disposing)
    {
        if (disposing && (components != null))
        {
            components.Dispose();
        }
        base.Dispose(disposing);
    }

    #region Windows Form Designer generated code

    /// <summary>
    ///  Required method for Designer support - do not modify
    ///  the contents of this method with the code editor.
    /// </summary>
    private void InitializeComponent()
    {
        this.toolbarPanel = new System.Windows.Forms.Panel();
        this.addButton = new System.Windows.Forms.Button();
        this.deleteButton = new System.Windows.Forms.Button();
        this.disableButton = new System.Windows.Forms.Button();
        this.enableButton = new System.Windows.Forms.Button();
        this.refreshButton = new System.Windows.Forms.Button();
        this.openButton = new System.Windows.Forms.Button();
        this.grid = new System.Windows.Forms.DataGridView();
        this.statusStrip = new System.Windows.Forms.StatusStrip();
        this.statusLabel = new System.Windows.Forms.ToolStripStatusLabel();
        this.toolbarPanel.SuspendLayout();
        ((System.ComponentModel.ISupportInitialize)(this.grid)).BeginInit();
        this.statusStrip.SuspendLayout();
        this.SuspendLayout();
        // 
        // toolbarPanel
        // 
        this.toolbarPanel.Controls.Add(this.openButton);
        this.toolbarPanel.Controls.Add(this.addButton);
        this.toolbarPanel.Controls.Add(this.deleteButton);
        this.toolbarPanel.Controls.Add(this.disableButton);
        this.toolbarPanel.Controls.Add(this.enableButton);
        this.toolbarPanel.Controls.Add(this.refreshButton);
        this.toolbarPanel.Dock = System.Windows.Forms.DockStyle.Top;
        this.toolbarPanel.Location = new System.Drawing.Point(0, 0);
        this.toolbarPanel.Name = "toolbarPanel";
        this.toolbarPanel.Padding = new System.Windows.Forms.Padding(8);
        this.toolbarPanel.Size = new System.Drawing.Size(900, 54);
        this.toolbarPanel.TabIndex = 0;
        // 
        // addButton
        // 
        this.addButton.Location = new System.Drawing.Point(266, 12);
        this.addButton.Name = "addButton";
        this.addButton.Size = new System.Drawing.Size(75, 30);
        this.addButton.TabIndex = 4;
        this.addButton.Text = "添加";
        this.addButton.UseVisualStyleBackColor = true;
        this.addButton.Click += new System.EventHandler(this.addButton_Click);
        // 
        // deleteButton
        // 
        this.deleteButton.Location = new System.Drawing.Point(185, 12);
        this.deleteButton.Name = "deleteButton";
        this.deleteButton.Size = new System.Drawing.Size(75, 30);
        this.deleteButton.TabIndex = 3;
        this.deleteButton.Text = "删除";
        this.deleteButton.UseVisualStyleBackColor = true;
        this.deleteButton.Click += new System.EventHandler(this.deleteButton_Click);
        // 
        // disableButton
        // 
        this.disableButton.Location = new System.Drawing.Point(104, 12);
        this.disableButton.Name = "disableButton";
        this.disableButton.Size = new System.Drawing.Size(75, 30);
        this.disableButton.TabIndex = 2;
        this.disableButton.Text = "禁用";
        this.disableButton.UseVisualStyleBackColor = true;
        this.disableButton.Click += new System.EventHandler(this.disableButton_Click);
        // 
        // enableButton
        // 
        this.enableButton.Location = new System.Drawing.Point(23, 12);
        this.enableButton.Name = "enableButton";
        this.enableButton.Size = new System.Drawing.Size(75, 30);
        this.enableButton.TabIndex = 1;
        this.enableButton.Text = "启用";
        this.enableButton.UseVisualStyleBackColor = true;
        this.enableButton.Click += new System.EventHandler(this.enableButton_Click);
        // 
        // refreshButton
        // 
        this.refreshButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
        this.refreshButton.Location = new System.Drawing.Point(805, 12);
        this.refreshButton.Name = "refreshButton";
        this.refreshButton.Size = new System.Drawing.Size(84, 34);
        this.refreshButton.TabIndex = 0;
        this.refreshButton.Text = "刷新";
        this.refreshButton.UseVisualStyleBackColor = true;
        this.refreshButton.Click += new System.EventHandler(this.refreshButton_Click);
        // 
        // openButton
        // 
        this.openButton.Location = new System.Drawing.Point(347, 12);
        this.openButton.Name = "openButton";
        this.openButton.Size = new System.Drawing.Size(108, 30);
        this.openButton.TabIndex = 5;
        this.openButton.Text = "打开所在位置";
        this.openButton.UseVisualStyleBackColor = true;
        this.openButton.Click += new System.EventHandler(this.openButton_Click);
        // 
        // grid
        // 
        this.grid.AllowUserToAddRows = false;
        this.grid.AllowUserToDeleteRows = false;
        this.grid.AllowUserToResizeRows = false;
        this.grid.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
        this.grid.Dock = System.Windows.Forms.DockStyle.Fill;
        this.grid.Location = new System.Drawing.Point(0, 54);
        this.grid.MultiSelect = false;
        this.grid.Name = "grid";
        this.grid.ReadOnly = true;
        this.grid.RowHeadersVisible = false;
        this.grid.RowTemplate.Height = 25;
        this.grid.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
        this.grid.Size = new System.Drawing.Size(900, 456);
        this.grid.TabIndex = 1;
        this.grid.CellDoubleClick += new System.Windows.Forms.DataGridViewCellEventHandler(this.grid_CellDoubleClick);
        // 
        // statusStrip
        // 
        this.statusStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.statusLabel});
        this.statusStrip.Location = new System.Drawing.Point(0, 510);
        this.statusStrip.Name = "statusStrip";
        this.statusStrip.Size = new System.Drawing.Size(900, 22);
        this.statusStrip.TabIndex = 2;
        this.statusStrip.Text = "statusStrip1";
        // 
        // statusLabel
        // 
        this.statusLabel.Name = "statusLabel";
        this.statusLabel.Size = new System.Drawing.Size(0, 17);
        // 
        // MainForm
        // 
        AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
        AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
        ClientSize = new System.Drawing.Size(900, 532);
        Controls.Add(this.grid);
        Controls.Add(this.statusStrip);
        Controls.Add(this.toolbarPanel);
        MinimumSize = new System.Drawing.Size(720, 400);
        Name = "MainForm";
        Text = "StartClean 启动项管理";
        Load += new System.EventHandler(this.MainForm_Load);
        this.toolbarPanel.ResumeLayout(false);
        ((System.ComponentModel.ISupportInitialize)(this.grid)).EndInit();
        this.statusStrip.ResumeLayout(false);
        this.statusStrip.PerformLayout();
        this.ResumeLayout(false);
        this.PerformLayout();
    }

    #endregion

    private System.Windows.Forms.Panel toolbarPanel;
    private System.Windows.Forms.Button refreshButton;
    private System.Windows.Forms.Button enableButton;
    private System.Windows.Forms.Button disableButton;
    private System.Windows.Forms.Button deleteButton;
    private System.Windows.Forms.Button addButton;
    private System.Windows.Forms.Button openButton;
    private System.Windows.Forms.DataGridView grid;
    private System.Windows.Forms.StatusStrip statusStrip;
    private System.Windows.Forms.ToolStripStatusLabel statusLabel;
}

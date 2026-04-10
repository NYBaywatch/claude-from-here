using System;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
using Microsoft.Win32;

namespace ClaudeFromHereConfig
{
    public class MainForm : Form
    {
        // Controls
        private ComboBox modelComboBox;
        private CheckBox verboseCheckBox;
        private TextBox allowedToolsTextBox;
        private TextBox extraFlagsTextBox;
        private Label claudePathLabel;
        private Label wtPathLabel;
        private Button applyButton;
        private Button discardButton;

        // Registry
        private const string RegistryPath = @"Software\ClaudeFromHere";

        public MainForm()
        {
            InitializeComponent();
            LoadSettings();
            DetectPaths();
        }

        private void InitializeComponent()
        {
            // Form properties
            this.Text = "Claude From Here \u2014 Settings";
            this.Font = SystemFonts.MessageBoxFont;
            this.MinimumSize = new Size(400, 320);
            this.ClientSize = new Size(420, 360);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = SystemColors.Control;

            int formPadding = 16; // md
            int sm = 8;
            int lg = 24;
            int xs = 4;
            int buttonWidth = 75;
            int buttonHeight = 23;
            int labelWidth = 90;
            int controlRight = this.ClientSize.Width - formPadding;
            int controlWidth = controlRight - formPadding - labelWidth - xs;

            // ---- GroupBox 1: Claude CLI Flags ----
            var flagsGroup = new GroupBox();
            flagsGroup.Text = "Claude CLI Flags";
            flagsGroup.Location = new Point(formPadding, formPadding);
            flagsGroup.Width = this.ClientSize.Width - formPadding * 2;
            flagsGroup.BackColor = SystemColors.Control;

            int y = sm + xs; // top of content inside groupbox

            // Model label + combo
            var modelLabel = new Label();
            modelLabel.Text = "Model:";
            modelLabel.Location = new Point(sm, y + 2);
            modelLabel.AutoSize = true;
            modelLabel.ForeColor = SystemColors.ControlText;

            modelComboBox = new ComboBox();
            modelComboBox.DropDownStyle = ComboBoxStyle.DropDownList;
            modelComboBox.Location = new Point(sm + labelWidth + xs, y);
            modelComboBox.Width = controlWidth;
            modelComboBox.Items.Add("(default \u2014 no flag)");
            modelComboBox.Items.Add("claude-opus-4-5");
            modelComboBox.Items.Add("claude-sonnet-4-5");
            modelComboBox.Items.Add("claude-haiku-3-5");
            modelComboBox.Items.Add("sonnet");
            modelComboBox.Items.Add("opus");
            modelComboBox.Items.Add("haiku");
            modelComboBox.SelectedIndex = 0;

            y += modelComboBox.Height + sm;

            // Verbose checkbox
            verboseCheckBox = new CheckBox();
            verboseCheckBox.Text = "Enable verbose mode";
            verboseCheckBox.Location = new Point(sm + labelWidth + xs, y);
            verboseCheckBox.AutoSize = true;
            verboseCheckBox.ForeColor = SystemColors.ControlText;

            y += verboseCheckBox.Height + sm;

            // Allowed Tools label + textbox
            var allowedToolsLabel = new Label();
            allowedToolsLabel.Text = "Allowed Tools:";
            allowedToolsLabel.Location = new Point(sm, y + 2);
            allowedToolsLabel.AutoSize = true;
            allowedToolsLabel.ForeColor = SystemColors.ControlText;

            allowedToolsTextBox = new TextBox();
            allowedToolsTextBox.Location = new Point(sm + labelWidth + xs, y);
            allowedToolsTextBox.Width = controlWidth;

            y += allowedToolsTextBox.Height + sm;

            // Extra Flags label + textbox
            var extraFlagsLabel = new Label();
            extraFlagsLabel.Text = "Extra Flags:";
            extraFlagsLabel.Location = new Point(sm, y + 2);
            extraFlagsLabel.AutoSize = true;
            extraFlagsLabel.ForeColor = SystemColors.ControlText;

            extraFlagsTextBox = new TextBox();
            extraFlagsTextBox.Location = new Point(sm + labelWidth + xs, y);
            extraFlagsTextBox.Width = controlWidth;

            y += extraFlagsTextBox.Height + sm;

            flagsGroup.Height = y + xs;
            flagsGroup.Controls.Add(modelLabel);
            flagsGroup.Controls.Add(modelComboBox);
            flagsGroup.Controls.Add(verboseCheckBox);
            flagsGroup.Controls.Add(allowedToolsLabel);
            flagsGroup.Controls.Add(allowedToolsTextBox);
            flagsGroup.Controls.Add(extraFlagsLabel);
            flagsGroup.Controls.Add(extraFlagsTextBox);

            this.Controls.Add(flagsGroup);

            // ---- GroupBox 2: Path Detection ----
            var pathGroup = new GroupBox();
            pathGroup.Text = "Path Detection";
            pathGroup.Location = new Point(formPadding, flagsGroup.Bottom + lg);
            pathGroup.Width = this.ClientSize.Width - formPadding * 2;
            pathGroup.BackColor = SystemColors.Control;

            int py = sm + xs;

            // claude.exe row
            var claudeStaticLabel = new Label();
            claudeStaticLabel.Text = "claude.exe:";
            claudeStaticLabel.Location = new Point(sm, py + 2);
            claudeStaticLabel.AutoSize = true;
            claudeStaticLabel.ForeColor = SystemColors.ControlText;

            claudePathLabel = new Label();
            claudePathLabel.Text = "Detecting...";
            claudePathLabel.Location = new Point(sm + labelWidth + xs, py + 2);
            claudePathLabel.Width = controlWidth;
            claudePathLabel.AutoSize = false;
            claudePathLabel.ForeColor = SystemColors.ControlText;

            py += claudeStaticLabel.Height + sm;

            // wt.exe row
            var wtStaticLabel = new Label();
            wtStaticLabel.Text = "wt.exe:";
            wtStaticLabel.Location = new Point(sm, py + 2);
            wtStaticLabel.AutoSize = true;
            wtStaticLabel.ForeColor = SystemColors.ControlText;

            wtPathLabel = new Label();
            wtPathLabel.Text = "Detecting...";
            wtPathLabel.Location = new Point(sm + labelWidth + xs, py + 2);
            wtPathLabel.Width = controlWidth;
            wtPathLabel.AutoSize = false;
            wtPathLabel.ForeColor = SystemColors.ControlText;

            py += wtStaticLabel.Height + sm;

            pathGroup.Height = py + xs;
            pathGroup.Controls.Add(claudeStaticLabel);
            pathGroup.Controls.Add(claudePathLabel);
            pathGroup.Controls.Add(wtStaticLabel);
            pathGroup.Controls.Add(wtPathLabel);

            this.Controls.Add(pathGroup);

            // ---- Buttons ----
            int buttonY = pathGroup.Bottom + lg;

            applyButton = new Button();
            applyButton.Text = "Apply Settings";
            applyButton.Size = new Size(buttonWidth, buttonHeight);
            applyButton.Location = new Point(this.ClientSize.Width - formPadding - buttonWidth, buttonY);
            applyButton.BackColor = SystemColors.Control;
            applyButton.ForeColor = SystemColors.ControlText;
            applyButton.Click += (s, e) => ApplySettings();

            discardButton = new Button();
            discardButton.Text = "Discard Changes";
            discardButton.Size = new Size(buttonWidth + 10, buttonHeight);
            discardButton.Location = new Point(applyButton.Left - discardButton.Width - sm, buttonY);
            discardButton.BackColor = SystemColors.Control;
            discardButton.ForeColor = SystemColors.ControlText;
            discardButton.Click += (s, e) => this.Close();

            this.Controls.Add(applyButton);
            this.Controls.Add(discardButton);

            // Resize form to fit content
            this.ClientSize = new Size(420, buttonY + buttonHeight + formPadding);
        }

        private void LoadSettings()
        {
            using (var key = Registry.CurrentUser.OpenSubKey(RegistryPath))
            {
                if (key == null) return;

                var model = key.GetValue("Model", "") as string ?? "";
                // Find matching item in ComboBox; if not found, leave at index 0 (default)
                for (int i = 0; i < modelComboBox.Items.Count; i++)
                {
                    if (modelComboBox.Items[i].ToString() == model)
                    {
                        modelComboBox.SelectedIndex = i;
                        break;
                    }
                }

                verboseCheckBox.Checked = ((int)(key.GetValue("Verbose", 0) ?? 0)) != 0;
                allowedToolsTextBox.Text = key.GetValue("AllowedTools", "") as string ?? "";
                extraFlagsTextBox.Text = key.GetValue("ExtraFlags", "") as string ?? "";
            }
        }

        private void ApplySettings()
        {
            // Warn about unsafe Extra Flags (per UI-SPEC On Apply Settings Behavior step 1)
            if (!string.IsNullOrEmpty(extraFlagsTextBox.Text))
            {
                foreach (char c in new[] { '|', '&', '<', '>', '^' })
                {
                    if (extraFlagsTextBox.Text.Contains(c.ToString()))
                    {
                        var result = MessageBox.Show(
                            "The Extra Flags field contains characters that may cause launch failures. Continue saving?",
                            "Claude From Here",
                            MessageBoxButtons.YesNo,
                            MessageBoxIcon.Warning);
                        if (result == DialogResult.No) return;
                        break;
                    }
                }
            }

            using (var key = Registry.CurrentUser.CreateSubKey(RegistryPath))
            {
                // For model, store the raw value (not the display text for the default item)
                string modelValue = modelComboBox.SelectedIndex <= 0 ? "" : modelComboBox.SelectedItem.ToString();
                key.SetValue("Model", modelValue, RegistryValueKind.String);
                key.SetValue("Verbose", verboseCheckBox.Checked ? 1 : 0, RegistryValueKind.DWord);
                key.SetValue("AllowedTools", allowedToolsTextBox.Text ?? "", RegistryValueKind.String);
                key.SetValue("ExtraFlags", extraFlagsTextBox.Text ?? "", RegistryValueKind.String);
            }

            this.DialogResult = DialogResult.OK;
            this.Close();
        }

        private string FindExecutablePath(string exeName, string appPathsSubkey)
        {
            // Stage 1: PATH
            foreach (var dir in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(';'))
            {
                try
                {
                    var full = Path.Combine(dir.Trim(), exeName);
                    if (File.Exists(full)) return full;
                }
                catch { }
            }

            // Stage 2: HKCU App Paths
            using (var key = Registry.CurrentUser.OpenSubKey(appPathsSubkey))
            {
                var path = key?.GetValue(null) as string;
                if (!string.IsNullOrEmpty(path) && File.Exists(path)) return path;
            }

            // Stage 3: HKLM App Paths
            using (var key = Registry.LocalMachine.OpenSubKey(appPathsSubkey))
            {
                var path = key?.GetValue(null) as string;
                if (!string.IsNullOrEmpty(path) && File.Exists(path)) return path;
            }

            // Stage 4: execution alias (wt.exe only)
            if (exeName.Equals("wt.exe", StringComparison.OrdinalIgnoreCase))
            {
                var alias = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    @"Microsoft\WindowsApps\wt.exe");
                if (File.Exists(alias)) return alias;
            }

            return null;
        }

        private void DetectPaths()
        {
            // Detect claude.exe
            var claudePath = FindExecutablePath("claude.exe",
                @"Software\Microsoft\Windows\CurrentVersion\App Paths\claude.exe");
            if (!string.IsNullOrEmpty(claudePath))
            {
                claudePathLabel.Text = "Found at " + claudePath;
                claudePathLabel.ForeColor = SystemColors.ControlText;
            }
            else
            {
                claudePathLabel.Text = "Not found";
                claudePathLabel.ForeColor = Color.Red;
            }

            // Detect wt.exe
            var wtPath = FindExecutablePath("wt.exe",
                @"Software\Microsoft\Windows\CurrentVersion\App Paths\wt.exe");
            if (!string.IsNullOrEmpty(wtPath))
            {
                wtPathLabel.Text = "Found at " + wtPath;
                wtPathLabel.ForeColor = SystemColors.ControlText;
            }
            else
            {
                wtPathLabel.Text = "Not found";
                wtPathLabel.ForeColor = Color.Red;
            }
        }
    }
}

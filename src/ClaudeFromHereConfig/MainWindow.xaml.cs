using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using Microsoft.Win32;

namespace ClaudeFromHereConfig
{
    public partial class MainWindow : Window
    {
        [DllImport("dwmapi.dll", PreserveSig = true)]
        private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);

        private void EnableDarkTitleBar()
        {
            var hwnd = new WindowInteropHelper(this).Handle;
            int darkMode = 1;
            DwmSetWindowAttribute(hwnd, 20, ref darkMode, sizeof(int)); // DWMWA_USE_IMMERSIVE_DARK_MODE
            int borderColor = 0x00000000; // black border
            DwmSetWindowAttribute(hwnd, 34, ref borderColor, sizeof(int)); // DWMWA_BORDER_COLOR
            int captionColor = 0x001e1e1e;
            DwmSetWindowAttribute(hwnd, 35, ref captionColor, sizeof(int)); // DWMWA_CAPTION_COLOR
            int textColor = 0x00d4d4d4; // COLORREF BGR for #d4d4d4
            DwmSetWindowAttribute(hwnd, 36, ref textColor, sizeof(int)); // DWMWA_TEXT_COLOR
        }
        private const string RegistryPath = @"Software\ClaudeFromHere";
        private ObservableCollection<string> _channels = new ObservableCollection<string>();

        private static readonly Dictionary<string, string> _presetMap = new()
        {
            { "Telegram", "plugin:telegram@claude-plugins-official" },
            { "Discord",  "plugin:discord@claude-plugins-official"  },
        };

        public MainWindow()
        {
            InitializeComponent();
            SourceInitialized += (_, _) => EnableDarkTitleBar();
            ChannelListBox.ItemsSource = _channels;
            LoadSettings();
            DetectPaths();
        }

        private void LoadSettings()
        {
            using (var key = Registry.CurrentUser.OpenSubKey(RegistryPath))
            {
                if (key == null) return;

                // Model
                var model = key.GetValue("Model", "") as string ?? "";
                for (int i = 0; i < modelComboBox.Items.Count; i++)
                {
                    var item = modelComboBox.Items[i];
                    string? itemText = null;
                    if (item is System.Windows.Controls.ComboBoxItem cbi)
                        itemText = cbi.Content?.ToString();
                    else
                        itemText = item?.ToString();

                    if (itemText == model)
                    {
                        modelComboBox.SelectedIndex = i;
                        break;
                    }
                }

                verboseCheckBox.IsChecked = ((int)(key.GetValue("Verbose", 0) ?? 0)) != 0;
                allowedToolsTextBox.Text = key.GetValue("AllowedTools", "") as string ?? "";
                extraFlagsTextBox.Text = key.GetValue("ExtraFlags", "") as string ?? "";
                continueCheckBox.IsChecked = ((int)(key.GetValue("Continue", 0) ?? 0)) != 0;
                resumeCheckBox.IsChecked = ((int)(key.GetValue("Resume", 0) ?? 0)) != 0;
                dangerSkipCheckBox.IsChecked = ((int)(key.GetValue("DangerouslySkipPermissions", 0) ?? 0)) != 0;
                allowDangerSkipCheckBox.IsChecked = ((int)(key.GetValue("AllowDangerouslySkipPermissions", 0) ?? 0)) != 0;
                remotePrefixTextBox.Text = key.GetValue("RemoteControlPrefix", "") as string ?? "";

                var channelsRaw = key.GetValue("Channels", "") as string ?? "";
                _channels.Clear();
                foreach (var ch in channelsRaw.Split('|'))
                    if (!string.IsNullOrWhiteSpace(ch)) _channels.Add(ch.Trim());
            }
        }

        private void ApplySettings_Click(object sender, RoutedEventArgs e)
        {
            // Extra flags safety check
            if (!string.IsNullOrEmpty(extraFlagsTextBox.Text))
            {
                foreach (char c in new[] { '|', '&', '<', '>', '^' })
                {
                    if (extraFlagsTextBox.Text.Contains(c.ToString()))
                    {
                        var result = MessageBox.Show(
                            "The Extra Flags field contains characters that may cause launch failures. Continue saving?",
                            "Claude From Here",
                            MessageBoxButton.YesNo,
                            MessageBoxImage.Warning);
                        if (result == MessageBoxResult.No) return;
                        break;
                    }
                }
            }

            using (var key = Registry.CurrentUser.CreateSubKey(RegistryPath))
            {
                // Model — empty string if default selected, otherwise the item text
                string modelValue = "";
                if (modelComboBox.SelectedIndex > 0)
                {
                    var item = modelComboBox.SelectedItem;
                    if (item is System.Windows.Controls.ComboBoxItem cbi)
                        modelValue = cbi.Content?.ToString() ?? "";
                    else
                        modelValue = item?.ToString() ?? "";
                }
                key.SetValue("Model", modelValue, RegistryValueKind.String);
                key.SetValue("Verbose", verboseCheckBox.IsChecked == true ? 1 : 0, RegistryValueKind.DWord);
                key.SetValue("AllowedTools", allowedToolsTextBox.Text ?? "", RegistryValueKind.String);
                key.SetValue("ExtraFlags", extraFlagsTextBox.Text ?? "", RegistryValueKind.String);
                key.SetValue("Continue", continueCheckBox.IsChecked == true ? 1 : 0, RegistryValueKind.DWord);
                key.SetValue("Resume", resumeCheckBox.IsChecked == true ? 1 : 0, RegistryValueKind.DWord);
                key.SetValue("DangerouslySkipPermissions", dangerSkipCheckBox.IsChecked == true ? 1 : 0, RegistryValueKind.DWord);
                key.SetValue("AllowDangerouslySkipPermissions", allowDangerSkipCheckBox.IsChecked == true ? 1 : 0, RegistryValueKind.DWord);
                key.SetValue("RemoteControlPrefix", remotePrefixTextBox.Text ?? "", RegistryValueKind.String);
                key.SetValue("Channels", string.Join("|", _channels), RegistryValueKind.String);
            }

            this.Close();
        }

        private void MinButton_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;

        private void MaxButton_Click(object sender, RoutedEventArgs e) =>
            WindowState = WindowState == WindowState.Maximized ? WindowState.Normal : WindowState.Maximized;

        private void CloseButton_Click(object sender, RoutedEventArgs e) => Close();

        private void DiscardChanges_Click(object sender, RoutedEventArgs e)
        {
            this.Close();
        }

        private void AddChannel_Click(object sender, RoutedEventArgs e)
        {
            var raw = channelComboBox.Text?.Trim();
            if (string.IsNullOrEmpty(raw)) return;
            var value = _presetMap.TryGetValue(raw, out var mapped) ? mapped : raw;
            if (!_channels.Contains(value))
                _channels.Add(value);
            channelComboBox.Text = "";
        }

        private void RemoveChannel_Click(object sender, RoutedEventArgs e)
        {
            var btn = (System.Windows.Controls.Button)sender;
            var channel = (string)btn.DataContext;
            _channels.Remove(channel);
        }

        private string? FindExecutablePath(string exeName, string appPathsSubkey)
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
                claudePathText.Text = "Found at " + claudePath;
                claudePathText.Foreground = (System.Windows.Media.SolidColorBrush)FindResource("SuccessBrush");
            }
            else
            {
                claudePathText.Text = "Not found \u2014 Claude Code may not launch correctly";
                claudePathText.Foreground = (System.Windows.Media.SolidColorBrush)FindResource("DestructiveBrush");
            }

            // Detect wt.exe
            var wtPath = FindExecutablePath("wt.exe",
                @"Software\Microsoft\Windows\CurrentVersion\App Paths\wt.exe");
            if (!string.IsNullOrEmpty(wtPath))
            {
                wtPathText.Text = "Found at " + wtPath;
                wtPathText.Foreground = (System.Windows.Media.SolidColorBrush)FindResource("SuccessBrush");
            }
            else
            {
                wtPathText.Text = "Not found \u2014 Claude Code may not launch correctly";
                wtPathText.Foreground = (System.Windows.Media.SolidColorBrush)FindResource("DestructiveBrush");
            }
        }
    }
}

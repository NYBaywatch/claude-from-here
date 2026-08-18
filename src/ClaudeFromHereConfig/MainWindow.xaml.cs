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
        private const string ProvidersPath = RegistryPath + @"\Providers";
        private const int MaxProviders = 8;
        private ObservableCollection<string> _channels = new ObservableCollection<string>();
        private ObservableCollection<ProviderEntry> _providers = new ObservableCollection<ProviderEntry>();

        public class ProviderEntry
        {
            public string Name { get; set; } = "";
            public string BaseUrl { get; set; } = "";
            public string Model { get; set; } = "";
            public string Effort { get; set; } = "";
            public byte[]? TokenBlob { get; set; } // DPAPI blob; null = no key
            public string Display =>
                $"{Name} — {BaseUrl}" +
                (string.IsNullOrEmpty(Model) ? "" : $" ({Model})") +
                (string.IsNullOrEmpty(Effort) ? "" : $" [{Effort}]") +
                (TokenBlob == null ? "" : "  \U0001F511");
        }

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
            ProviderListBox.ItemsSource = _providers;
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
                var effortLevels = (key.GetValue("EffortLevels", "") as string ?? "").Split('|');
                effortLowCheckBox.IsChecked    = Array.IndexOf(effortLevels, "low") >= 0;
                effortMediumCheckBox.IsChecked = Array.IndexOf(effortLevels, "medium") >= 0;
                effortHighCheckBox.IsChecked   = Array.IndexOf(effortLevels, "high") >= 0;
                effortXhighCheckBox.IsChecked  = Array.IndexOf(effortLevels, "xhigh") >= 0;
                effortMaxCheckBox.IsChecked    = Array.IndexOf(effortLevels, "max") >= 0;

                var channelsRaw = key.GetValue("Channels", "") as string ?? "";
                _channels.Clear();
                foreach (var ch in channelsRaw.Split('|'))
                    if (!string.IsNullOrWhiteSpace(ch)) _channels.Add(ch.Trim());
            }

            LoadProviders();
        }

        private void LoadProviders()
        {
            _providers.Clear();
            using var root = Registry.CurrentUser.OpenSubKey(ProvidersPath);
            if (root == null) return;

            foreach (var subName in root.GetSubKeyNames())
            {
                if (_providers.Count >= MaxProviders) break;
                using var sub = root.OpenSubKey(subName);
                if (sub == null) continue;

                var entry = new ProviderEntry
                {
                    Name = sub.GetValue("Name", "") as string ?? "",
                    BaseUrl = sub.GetValue("BaseUrl", "") as string ?? "",
                    Model = sub.GetValue("Model", "") as string ?? "",
                    Effort = sub.GetValue("Effort", "") as string ?? "",
                    TokenBlob = sub.GetValue("AuthToken", null) as byte[],
                };
                if (entry.Name.Length > 0 || entry.BaseUrl.Length > 0)
                    _providers.Add(entry);
            }
        }

        // Characters that would break the cmd `set "VAR=value"` chain the shell
        // extension builds at launch.
        private static readonly char[] _cmdUnsafeChars = { '"', '&', '|', '<', '>', '^', '%', ';' };

        private static string? FirstUnsafeField(params (string label, string value)[] fields)
        {
            foreach (var (label, value) in fields)
                if (value.IndexOfAny(_cmdUnsafeChars) >= 0)
                    return label;
            return null;
        }

        private void ProviderPreset_Changed(object sender, System.Windows.Controls.SelectionChangedEventArgs e)
        {
            if (providerNameTextBox == null) return; // fires during InitializeComponent
            var preset = (providerPresetComboBox.SelectedItem as System.Windows.Controls.ComboBoxItem)?.Content?.ToString();
            switch (preset)
            {
                case "OpenRouter (Kimi K3)":
                    providerNameTextBox.Text = "Kimi K3 (OpenRouter)";
                    providerBaseUrlTextBox.Text = "https://openrouter.ai/api";
                    providerModelTextBox.Text = "moonshotai/kimi-k3";
                    break;
                case "Kimi K2 (Moonshot)":
                    providerNameTextBox.Text = "Kimi K2";
                    providerBaseUrlTextBox.Text = "https://api.moonshot.ai/anthropic";
                    providerModelTextBox.Text = "kimi-k2-0905-preview";
                    break;
                case "Qwen (DashScope)":
                    providerNameTextBox.Text = "Qwen";
                    providerBaseUrlTextBox.Text = "https://dashscope-intl.aliyuncs.com/api/v2/apps/claude-code-proxy";
                    providerModelTextBox.Text = "qwen3-coder-plus";
                    break;
                case "Local proxy (claude-code-router)":
                    providerNameTextBox.Text = "Local";
                    providerBaseUrlTextBox.Text = "http://127.0.0.1:3456";
                    providerModelTextBox.Text = "";
                    break;
            }
        }

        private void AddProvider_Click(object sender, RoutedEventArgs e)
        {
            var name = providerNameTextBox.Text?.Trim() ?? "";
            var baseUrl = providerBaseUrlTextBox.Text?.Trim() ?? "";
            var model = providerModelTextBox.Text?.Trim() ?? "";
            var apiKey = providerKeyBox.Password ?? "";

            if (name.Length == 0 || baseUrl.Length == 0)
            {
                MessageBox.Show("Provider needs at least a name and a base URL.",
                    "Claude From Here", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            var badField = FirstUnsafeField(("Name", name), ("Base URL", baseUrl),
                ("Model", model), ("API key", apiKey));
            if (badField != null)
            {
                MessageBox.Show(
                    $"The {badField} field contains a character (\" & | < > ^ % ;) that would break the launch command.",
                    "Claude From Here", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            if (_providers.Count >= MaxProviders)
            {
                MessageBox.Show($"At most {MaxProviders} providers are supported.",
                    "Claude From Here", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            var effort = (providerEffortComboBox.SelectedItem as System.Windows.Controls.ComboBoxItem)?.Content?.ToString() ?? "";
            if (effort == "(default)") effort = "";

            var entry = new ProviderEntry { Name = name, BaseUrl = baseUrl, Model = model, Effort = effort };
            if (apiKey.Length > 0)
            {
                entry.TokenBlob = System.Security.Cryptography.ProtectedData.Protect(
                    System.Text.Encoding.Unicode.GetBytes(apiKey),
                    null,
                    System.Security.Cryptography.DataProtectionScope.CurrentUser);
            }
            _providers.Add(entry);

            providerNameTextBox.Text = "";
            providerBaseUrlTextBox.Text = "";
            providerModelTextBox.Text = "";
            providerKeyBox.Clear();
            providerEffortComboBox.SelectedIndex = 0;
            providerPresetComboBox.SelectedIndex = 0;
        }

        private void RemoveProvider_Click(object sender, RoutedEventArgs e)
        {
            var btn = (System.Windows.Controls.Button)sender;
            var entry = (ProviderEntry)btn.DataContext;
            _providers.Remove(entry);
        }

        private void SaveProviders()
        {
            // Rewrite the whole Providers subtree from the in-memory list.
            using (var root = Registry.CurrentUser.CreateSubKey(RegistryPath))
                root.DeleteSubKeyTree("Providers", throwOnMissingSubKey: false);

            if (_providers.Count == 0) return;

            using var providersKey = Registry.CurrentUser.CreateSubKey(ProvidersPath);
            for (int i = 0; i < _providers.Count; i++)
            {
                using var sub = providersKey.CreateSubKey(i.ToString());
                sub.SetValue("Name", _providers[i].Name, RegistryValueKind.String);
                sub.SetValue("BaseUrl", _providers[i].BaseUrl, RegistryValueKind.String);
                sub.SetValue("Model", _providers[i].Model, RegistryValueKind.String);
                sub.SetValue("Effort", _providers[i].Effort, RegistryValueKind.String);
                if (_providers[i].TokenBlob != null)
                    sub.SetValue("AuthToken", _providers[i].TokenBlob!, RegistryValueKind.Binary);
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
                var enabledEfforts = new List<string>();
                if (effortLowCheckBox.IsChecked == true)    enabledEfforts.Add("low");
                if (effortMediumCheckBox.IsChecked == true) enabledEfforts.Add("medium");
                if (effortHighCheckBox.IsChecked == true)   enabledEfforts.Add("high");
                if (effortXhighCheckBox.IsChecked == true)  enabledEfforts.Add("xhigh");
                if (effortMaxCheckBox.IsChecked == true)    enabledEfforts.Add("max");
                key.SetValue("EffortLevels", string.Join("|", enabledEfforts), RegistryValueKind.String);
                key.DeleteValue("ShowEffortLevels", throwOnMissingValue: false);
            }

            SaveProviders();

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

# 📋 Clipboard SharePoint URL Cleaner

This is a small Windows utility that monitors your clipboard for SharePoint links and automatically replaces long, messy redirection URLs with a cleaner, direct format.

---

## ✨ What It Does

If you copy a SharePoint URL like this:

```
https://yourtenant.sharepoint.com/:u:/g/personal/user_name_company_com/...
```

Or even a longer version like:

```
https://yourtenant.sharepoint.com/personal/user_name_company_com/_layouts/15/onedrive.aspx?id=%2Fsites%2Fproject%2Fdocuments%2Ffile.docx
```

It will **automatically replace it in your clipboard** with a cleaner, decoded direct link like:

```
https://yourtenant.sharepoint.com/sites/project/documents/file.docx
```

---

## 🔧 How to Build (For Developers)

### 🧱 Requirements

- Windows 10 or later
- Visual Studio (any recent version with C++ support)
- `json.hpp` (from [nlohmann/json](https://github.com/nlohmann/json), single header)

### 📂 Folder Structure

```
project/
├── main.cpp
├── json.hpp
├── config.json
├── README.md
```

### 🛠️ Build Steps (Visual Studio)

1. Open Visual Studio.
2. Create a new **Empty C++ Project**.
3. Add `main.cpp` and `json.hpp` to the project.
4. Set **Character Set** to `Use Multi-Byte Character Set` (Project Properties → Advanced).
5. Link `wininet.lib`:
   - Project → Properties → Linker → Input → Additional Dependencies → add: `wininet.lib`
6. Build the solution (`Ctrl+Shift+B`).

### ⚙️ Config

Customize `config.json`:

```json
{
  "baseUrl": "https://yourtenant.sharepoint.com",
  "longUrlPath": "/personal/user_name_company_com/_layouts/15/onedrive.aspx"
}
```

> The base URL and long path can be customized for different SharePoint tenants and user accounts.

---

## 💻 How to Use (For End Users)

### ✅ Features

- Runs in the background (invisible window).
- Automatically modifies SharePoint URLs in your clipboard.
- Only modifies matching SharePoint links.
- Doesn’t modify any other clipboard content.

### 📦 How to Run

1. Download the binary: [`ClipboardCleaner.exe`](#) (add link when available).
2. Place it in a folder along with `config.json`.
3. Double-click `ClipboardCleaner.exe` to launch it.

> You'll see no window, but it’s running! To test, just copy a SharePoint URL.

### 🛑 To Stop It

- Open Task Manager.
- Find and end `ClipboardCleaner.exe`.

---

## ⚠️ Notes

- This app does **not** require admin privileges.
- Works only on Windows.
- This is a background clipboard tool — no GUI.

---

## 🛠️ License

MIT License — free to use, modify, and distribute.

---

## ✉️ Contact / Issues

Have issues or suggestions? Open an issue or reach out.
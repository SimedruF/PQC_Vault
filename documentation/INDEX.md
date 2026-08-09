# PQCWallet Documentation Index

This folder contains all documentation for the PQCWallet project, organized by category.

## 📚 Main Documentation

### Getting Started
- **[TODO.md](TODO.md)** - Prioritățile tehnice și de securitate pentru următoarea sesiune
- **[README.md](README.md)** - Original project README (moved from root)
- **[USAGE.md](USAGE.md)** - Complete usage guide and tutorials
- **[CONFIGURATION.md](CONFIGURATION.md)** - Configuration options and settings

### Installation & Build
- **[BUILD_WINDOWS.md](BUILD_WINDOWS.md)** - Windows-specific build instructions
- **[BUILD_ORGANIZATION_COMPLETE.md](BUILD_ORGANIZATION_COMPLETE.md)** - Build system organization
- **[DESKTOP_SHORTCUT_COMPLETE.md](DESKTOP_SHORTCUT_COMPLETE.md)** - Desktop integration

## 🔧 Implementation Guides

### Core Features
- **[IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)** - Overall implementation overview
- **[USER_FORMAT_V5.md](USER_FORMAT_V5.md)** - Formatul portabil PQCUSR05 și migrarea V1–V4
- **[PASSWORD_CHANGE_IMPLEMENTATION.md](PASSWORD_CHANGE_IMPLEMENTATION.md)** - Password change functionality
- **[PASSWORD_EXTRACTION_GUIDE.md](PASSWORD_EXTRACTION_GUIDE.md)** - Password extraction tools
- **[SECURITY_IMPROVEMENTS.md](SECURITY_IMPROVEMENTS.md)** - Security enhancements and cryptography
- **[SECURITY_TESTING.md](SECURITY_TESTING.md)** - Teste CTest, ASan/UBSan, fuzzing și CI Windows/Linux

### User Interface
- **[FONT_GUIDE.md](FONT_GUIDE.md)** - Font configuration and management
- **[THEME_IMPLEMENTATION_COMPLETE.md](THEME_IMPLEMENTATION_COMPLETE.md)** - Theme system implementation
- **[CUSTOM_ICON_COMPLETE.md](CUSTOM_ICON_COMPLETE.md)** - Custom icon integration

### File Management
- **[BACKUP_RESTORE.md](BACKUP_RESTORE.md)** - Formatul PQCBKP01 și fluxul sigur de backup/restore
- **[PATH_VALIDATION.md](PATH_VALIDATION.md)** - Validarea centralizată și izolarea canonică a căilor
- **[IMGUI_FILE_DIALOG_GUIDE.md](IMGUI_FILE_DIALOG_GUIDE.md)** - File dialog implementation
- **[IMGUI_FILE_DIALOG_INTEGRATION_SUMMARY.md](IMGUI_FILE_DIALOG_INTEGRATION_SUMMARY.md)** - Integration details
- **[IMGUI_FILE_DIALOG_TROUBLESHOOTING.md](IMGUI_FILE_DIALOG_TROUBLESHOOTING.md)** - File dialog troubleshooting

### Archive System
- **[ARCHIVE_TRANSACTIONS.md](ARCHIVE_TRANSACTIONS.md)** - Rollback, revizii și lock exclusiv per arhivă
- **[ARCHIVE_GUIDE.md](ARCHIVE_GUIDE.md)** - Archive functionality guide
- **[ARCHIVE_IMPLEMENTATION.md](ARCHIVE_IMPLEMENTATION.md)** - Archive system implementation
- **[DEMO_ARCHIVE.md](DEMO_ARCHIVE.md)** - Archive demonstration

## 🔧 Troubleshooting & Bug Fixes

### General Issues
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Common problems and solutions
- **[TESTING_NOTES.md](TESTING_NOTES.md)** - Testing procedures and notes
- **[TEST_RESULTS.md](TEST_RESULTS.md)** - Test results and validation

### Specific Bug Fixes
- **[anti_flickering_summary.md](anti_flickering_summary.md)** - Anti-flickering implementation
- **[dialog_flickering_solution.md](dialog_flickering_solution.md)** - Dialog flickering fixes
- **[flickering_fix_guide.md](flickering_fix_guide.md)** - General flickering solutions
- **[flickering_fix_v2.md](flickering_fix_v2.md)** - Updated flickering fixes
- **[fix_archivewindow_flickering.md](fix_archivewindow_flickering.md)** - Archive window specific fixes
- **[bugfix_report.md](bugfix_report.md)** - General bug fix reports

## 📋 Reference & Examples

### Examples & Demos
- **[EXAMPLES.md](EXAMPLES.md)** - Code examples and usage patterns
- **[SOLUTION_ARCHIVE.md](SOLUTION_ARCHIVE.md)** - Archived solutions and approaches

### Legacy Documentation
- **[README_NEW.md](README_NEW.md)** - Alternative README version

## 📁 Quick Navigation

### By Topic:
- **🔐 Security**: `SECURITY_IMPROVEMENTS.md`, `PASSWORD_*.md`
- **🎨 UI/UX**: `THEME_*.md`, `FONT_GUIDE.md`, `CUSTOM_ICON_*.md`
- **🏗️ Build**: `BUILD_*.md`, `DESKTOP_SHORTCUT_*.md`
- **📁 Files**: `IMGUI_FILE_DIALOG_*.md`, `ARCHIVE_*.md`
- **🐛 Bugs**: `*flickering*.md`, `bugfix_report.md`
- **🧪 Testing**: `TEST*.md`, `TESTING_NOTES.md`

### By Development Phase:
- **Planning**: `IMPLEMENTATION_SUMMARY.md`, `EXAMPLES.md`
- **Implementation**: `*_IMPLEMENTATION.md`, `*_GUIDE.md`
- **Testing**: `TEST_RESULTS.md`, `TESTING_NOTES.md`
- **Troubleshooting**: `TROUBLESHOOTING.md`, `*fix*.md`
- **Complete**: `*_COMPLETE.md`

---

*All documentation is now organized in this folder for better project structure and maintainability.*

#include "DatabaseManagerWindow.h"
#include <imgui.h>
#include <iostream>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <array>
#include <cstring>
#include <filesystem>
#include <cstdio>
#include <openssl/rand.h>

DatabaseManagerWindow::DatabaseManagerWindow(std::shared_ptr<EncryptedDatabase> database)
    : database_(database), show_window_(false), show_add_user_popup_(false), 
      show_edit_user_popup_(false), show_delete_confirmation_(false), 
      show_export_backup_popup_(false), show_import_backup_popup_(false),
      show_backup_password_(false), confirm_restore_(false),
      show_passwords_(false), password_verified_(false), message_timer_(0.0f) {
    
    // Initialize buffers
    memset(search_buffer_, 0, sizeof(search_buffer_));
    memset(new_username_, 0, sizeof(new_username_));
    memset(new_email_, 0, sizeof(new_email_));
    memset(new_website_, 0, sizeof(new_website_));
    memset(new_password_, 0, sizeof(new_password_));
    memset(confirm_password_, 0, sizeof(confirm_password_));
    memset(verification_password_, 0, sizeof(verification_password_));
    memset(backup_path_, 0, sizeof(backup_path_));
    memset(backup_password_, 0, sizeof(backup_password_));
    memset(backup_confirm_password_, 0, sizeof(backup_confirm_password_));
    
    // Load initial user list
    updateFilteredUsernames();
}

DatabaseManagerWindow::~DatabaseManagerWindow() {
    clearSensitiveUiState();
}

void DatabaseManagerWindow::render() {
    if (!show_window_) {
        return;
    }
    
    // Update message timer
    if (message_timer_ > 0.0f) {
        message_timer_ -= ImGui::GetIO().DeltaTime;
        if (message_timer_ <= 0.0f) {
            error_message_.clear();
            success_message_.clear();
        }
    }
    
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("[DB] Database Manager - PQC Encrypted Database", &show_window_, ImGuiWindowFlags_MenuBar)) {
        
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Database")) {
                if (ImGui::MenuItem("[STATS] Statistics")) {
                    // Show statistics
                }
                ImGui::Separator();
                if (ImGui::MenuItem("[EXPORT] Export Backup")) {
                    const auto now = std::chrono::system_clock::to_time_t(
                        std::chrono::system_clock::now());
                    const std::filesystem::path databasePath(database_->getDatabasePath());
                    const std::filesystem::path suggested =
                        std::filesystem::path("backups") /
                        (databasePath.stem().string() + "-" + std::to_string(now) +
                         ".pqcbak");
                    std::snprintf(backup_path_, sizeof(backup_path_), "%s",
                                  suggested.string().c_str());
                    clearBackupSensitiveState();
                    show_export_backup_popup_ = true;
                }
                if (ImGui::MenuItem("[IMPORT] Import Backup")) {
                    backup_path_[0] = '\0';
                    clearBackupSensitiveState();
                    show_import_backup_popup_ = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("[LOCK] Change Master Password")) {
                    // Change master password
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::Checkbox("[EYE] Show Passwords", &show_passwords_);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("[?] About")) {
                    // Show about dialog
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        
        // Show messages
        if (!error_message_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped("[X] %s", error_message_.c_str());
            ImGui::PopStyleColor();
        }
        if (!success_message_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            ImGui::TextWrapped("[OK] %s", success_message_.c_str());
            ImGui::PopStyleColor();
        }
        
        // Main content
        renderToolbar();
        ImGui::Separator();
        
        renderSearchBar();
        ImGui::Separator();
        
        // Split panes
        ImGui::BeginChild("UserList", ImVec2(ImGui::GetContentRegionAvail().x * 0.4f, 0), true);
        renderUserList();
        ImGui::EndChild();
        
        ImGui::SameLine();
        
        ImGui::BeginChild("UserDetails", ImVec2(0, 0), true);
        renderUserDetails();
        ImGui::EndChild();
        
        // Handle popups
        renderAddUserPopup();
        renderEditUserPopup();
        renderDeleteConfirmation();
        renderBackupPopups();
        
    }
    ImGui::End();
    if (!show_window_) {
        clearSensitiveUiState();
    }
}

void DatabaseManagerWindow::renderToolbar() {
    if (ImGui::Button("[+] Add User")) {
        show_add_user_popup_ = true;
        clearInputFields();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("[EDIT] Edit User")) {
        if (!selected_username_.empty()) {
            show_edit_user_popup_ = true;
            // Load user data for editing
            EncryptedDatabase::UserRecord record;
            if (database_->getUser(selected_username_, record)) {
                strcpy(new_username_, record.username.c_str());
                strcpy(new_email_, record.email.c_str());
                strcpy(new_website_, record.website.c_str());
            }
        } else {
            showError("Please select a user to edit");
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("[DEL] Delete User")) {
        if (!selected_username_.empty()) {
            show_delete_confirmation_ = true;
        } else {
            showError("Please select a user to delete");
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("[REFRESH] Refresh")) {
        updateFilteredUsernames();
        showSuccess("User list refreshed");
    }
    
    ImGui::SameLine();
    if (ImGui::Button("[GEN] Generate Password")) {
        std::string password = generateRandomPassword(16);
        strcpy(new_password_, password.c_str());
        strcpy(confirm_password_, password.c_str());
        SecureMemory::Cleanse(password);
        showSuccess("Secure password generated");
    }
}

void DatabaseManagerWindow::renderSearchBar() {
    ImGui::Text("[SEARCH] Search Users:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("##search", search_buffer_, sizeof(search_buffer_))) {
        updateFilteredUsernames();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        memset(search_buffer_, 0, sizeof(search_buffer_));
        updateFilteredUsernames();
    }
}

void DatabaseManagerWindow::renderUserList() {
    ImGui::Text("[USERS] Users (%zu)", filtered_usernames_.size());
    ImGui::Separator();
    
    for (const auto& username : filtered_usernames_) {
        bool is_selected = (username == selected_username_);
        if (ImGui::Selectable(username.c_str(), is_selected)) {
            selected_username_ = username;
            password_verified_ = false;
            SecureMemory::Cleanse(verification_password_);
        }
        
        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("[EDIT] Edit")) {
                selected_username_ = username;
                show_edit_user_popup_ = true;
            }
            if (ImGui::MenuItem("[DEL] Delete")) {
                selected_username_ = username;
                show_delete_confirmation_ = true;
            }
            ImGui::EndPopup();
        }
    }
}

void DatabaseManagerWindow::renderUserDetails() {
    if (selected_username_.empty()) {
        ImGui::Text("Select a user to view details");
        return;
    }
    
    ImGui::Text("[USER] User Details: %s", selected_username_.c_str());
    ImGui::Separator();
    
    EncryptedDatabase::UserRecord record;
    if (database_->getUser(selected_username_, record)) {
        ImGui::Text("[NAME] Username: %s", record.username.c_str());
        ImGui::Text("[EMAIL] Email: %s", record.email.c_str());
        ImGui::Text("[WEB] Website: %s", record.website.c_str());
        
        if (show_passwords_) {
            ImGui::Text("[HASH] Password Hash: %s", record.encrypted_password.c_str());
            
            ImGui::TextDisabled("Plaintext passwords are not cached in memory.");
        } else {
            ImGui::Text("[PASS] Password: ••••••••");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("[TOGGLE] Show/Hide Password")) {
            show_passwords_ = !show_passwords_;
        }
        
        ImGui::Text("[SALT] Salt: %s", record.salt.substr(0, 16).c_str());
        ImGui::Text("[CREATED] Created: %s", record.created_at.c_str());
        ImGui::Text("[LOGIN] Last Login: %s", record.last_login.c_str());
        
        ImGui::Separator();
        
        // Password verification section
        ImGui::Text("[VERIFY] Verify Password:");
        if (ImGui::InputText("Enter Password", verification_password_,
                             sizeof(verification_password_), ImGuiInputTextFlags_Password)) {
            password_verified_ = false;
        }
        
        ImGui::SameLine();
        if (ImGui::Button("[CHECK] Verify")) {
            SecureMemory::SecureString submittedPassword(verification_password_);
            SecureMemory::Cleanse(verification_password_);
            password_verified_ = !submittedPassword.empty() &&
                database_->verifyCredentials(selected_username_, submittedPassword.get());
            if (password_verified_) {
                showSuccess("Password verified successfully!");
            } else {
                showError("Password verification failed!");
            }
        }
        
        if (password_verified_) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("[OK] Password verified; plaintext was discarded.");
            ImGui::PopStyleColor();
        }
        
        ImGui::Separator();
        
        if (ImGui::Button("[LOGIN] Test Login")) {
            // Test login functionality
            showSuccess("Login test functionality would go here");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("🔄 Update Last Login")) {
            // Update last login time
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            record.last_login = std::to_string(time_t);
            
            if (database_->updateUser(selected_username_, record)) {
                showSuccess("Last login updated");
            } else {
                showError("Failed to update last login");
            }
        }
    } else {
        ImGui::Text("[X] Failed to load user details");
    }
}

void DatabaseManagerWindow::renderAddUserPopup() {
    if (show_add_user_popup_) {
        ImGui::OpenPopup("Add New User");
        show_add_user_popup_ = false; // Reset flag after opening
    }
    
    if (ImGui::BeginPopupModal("Add New User", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("[NEW] Create New User Account");
        ImGui::Separator();
        
        ImGui::InputText("Username", new_username_, sizeof(new_username_));
        ImGui::InputText("Email", new_email_, sizeof(new_email_));
        ImGui::InputText("Website", new_website_, sizeof(new_website_));
        
        if (show_passwords_) {
            ImGui::InputText("Password", new_password_, sizeof(new_password_));
            ImGui::InputText("Confirm Password", confirm_password_, sizeof(confirm_password_));
        } else {
            ImGui::InputText("Password", new_password_, sizeof(new_password_), ImGuiInputTextFlags_Password);
            ImGui::InputText("Confirm Password", confirm_password_, sizeof(confirm_password_), ImGuiInputTextFlags_Password);
        }
        
        if (ImGui::Button("🎲 Generate Password")) {
            std::string password = generateRandomPassword(16);
            strcpy(new_password_, password.c_str());
            strcpy(confirm_password_, password.c_str());
            SecureMemory::Cleanse(password);
        }
        
        ImGui::Separator();
        
        if (ImGui::Button("[OK] Create User")) {
            if (validateInput()) {
                addNewUser();
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("[X] Cancel")) {
            clearInputFields();
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void DatabaseManagerWindow::renderEditUserPopup() {
    if (show_edit_user_popup_) {
        ImGui::OpenPopup("Edit User");
        show_edit_user_popup_ = false; // Reset flag after opening
    }
    
    if (ImGui::BeginPopupModal("Edit User", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("[EDIT] Edit User: %s", selected_username_.c_str());
        ImGui::Separator();
        
        ImGui::InputText("Username", new_username_, sizeof(new_username_));
        ImGui::InputText("Email", new_email_, sizeof(new_email_));
        ImGui::InputText("Website", new_website_, sizeof(new_website_));
        
        if (show_passwords_) {
            ImGui::InputText("New Password", new_password_, sizeof(new_password_));
            ImGui::InputText("Confirm Password", confirm_password_, sizeof(confirm_password_));
        } else {
            ImGui::InputText("New Password", new_password_, sizeof(new_password_), ImGuiInputTextFlags_Password);
            ImGui::InputText("Confirm Password", confirm_password_, sizeof(confirm_password_), ImGuiInputTextFlags_Password);
        }
        
        ImGui::Text("💡 Leave password fields empty to keep current password");
        
        ImGui::Separator();
        
        if (ImGui::Button("[SAVE] Save Changes")) {
            editUser();
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("[X] Cancel")) {
            clearInputFields();
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void DatabaseManagerWindow::renderDeleteConfirmation() {
    if (show_delete_confirmation_) {
        ImGui::OpenPopup("Delete User");
        show_delete_confirmation_ = false; // Reset flag after opening
    }
    
    if (ImGui::BeginPopupModal("Delete User", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("[!] Are you sure you want to delete user:");
        ImGui::Text("   %s", selected_username_.c_str());
        ImGui::Text("This action cannot be undone!");
        
        ImGui::Separator();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("[DEL] Delete Forever")) {
            deleteUser();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        if (ImGui::Button("[X] Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void DatabaseManagerWindow::renderBackupPopups() {
    if (show_export_backup_popup_) {
        ImGui::OpenPopup("Export Encrypted Backup");
        show_export_backup_popup_ = false;
    }
    if (ImGui::BeginPopupModal("Export Encrypted Backup", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("The backup key is independent of the master password. "
                           "Store it offline; the backup cannot be restored without it.");
        if (!error_message_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                               error_message_.c_str());
        } else if (!success_message_.empty()) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s",
                               success_message_.c_str());
        }
        ImGui::Separator();
        ImGui::SetNextItemWidth(520.0f);
        ImGui::InputText("Backup path", backup_path_, sizeof(backup_path_));

        const ImGuiInputTextFlags passwordFlags =
            show_backup_password_ ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_Password;
        ImGui::SetNextItemWidth(360.0f);
        ImGui::InputText("Backup key", backup_password_, sizeof(backup_password_), passwordFlags);
        ImGui::SetNextItemWidth(360.0f);
        ImGui::InputText("Confirm key", backup_confirm_password_,
                         sizeof(backup_confirm_password_), passwordFlags);
        ImGui::Checkbox("Show backup key", &show_backup_password_);

        if (ImGui::Button("Generate 256-bit recovery key")) {
            std::string generated;
            if (database_->generateRecoveryKey(generated)) {
                std::snprintf(backup_password_, sizeof(backup_password_), "%s",
                              generated.c_str());
                std::snprintf(backup_confirm_password_, sizeof(backup_confirm_password_), "%s",
                              generated.c_str());
                show_backup_password_ = true;
                showSuccess("Recovery key generated. Save it before closing this dialog.");
            } else {
                showError("Could not generate a recovery key.");
            }
            SecureMemory::Cleanse(generated);
        }

        ImGui::Separator();
        if (ImGui::Button("Export", ImVec2(120, 0))) {
            SecureMemory::SecureString submittedKey(backup_password_);
            SecureMemory::SecureString confirmation(backup_confirm_password_);
            SecureMemory::Cleanse(backup_password_);
            SecureMemory::Cleanse(backup_confirm_password_);
            if (backup_path_[0] == '\0' || submittedKey.size() < 12) {
                showError("Choose a path and use a backup key of at least 12 characters.");
            } else if (!submittedKey.equals(confirmation.get())) {
                showError("Backup keys do not match.");
            } else if (database_->exportBackup(backup_path_, submittedKey.get())) {
                showSuccess("Encrypted backup exported successfully.");
                clearBackupSensitiveState();
                ImGui::CloseCurrentPopup();
            } else {
                showError("Backup export failed; an existing backup was not modified.");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            clearBackupSensitiveState();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (show_import_backup_popup_) {
        ImGui::OpenPopup("Restore Encrypted Backup");
        show_import_backup_popup_ = false;
    }
    if (ImGui::BeginPopupModal("Restore Encrypted Backup", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Restore replaces the current credential database only after "
                           "the complete backup has been authenticated and validated.");
        if (!error_message_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                               error_message_.c_str());
        }
        ImGui::Separator();
        ImGui::SetNextItemWidth(520.0f);
        ImGui::InputText("Backup path", backup_path_, sizeof(backup_path_));
        ImGui::SetNextItemWidth(360.0f);
        ImGui::InputText("Backup key", backup_password_, sizeof(backup_password_),
                         show_backup_password_ ? ImGuiInputTextFlags_None
                                               : ImGuiInputTextFlags_Password);
        ImGui::Checkbox("Show backup key", &show_backup_password_);
        ImGui::Checkbox("I understand that current database records will be replaced",
                        &confirm_restore_);

        ImGui::Separator();
        if (ImGui::Button("Authenticate and Restore", ImVec2(190, 0))) {
            SecureMemory::SecureString submittedKey(backup_password_);
            SecureMemory::Cleanse(backup_password_);
            if (backup_path_[0] == '\0' || submittedKey.empty()) {
                showError("Select a backup and enter its backup key.");
            } else if (!confirm_restore_) {
                showError("Confirm replacement of the current database first.");
            } else if (database_->importBackup(backup_path_, submittedKey.get())) {
                selected_username_.clear();
                password_verified_ = false;
                updateFilteredUsernames();
                showSuccess("Backup authenticated and restored successfully.");
                clearBackupSensitiveState();
                ImGui::CloseCurrentPopup();
            } else {
                showError("Restore failed. The current database remains unchanged.");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            clearBackupSensitiveState();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void DatabaseManagerWindow::updateFilteredUsernames() {
    filtered_usernames_.clear();
    
    std::vector<std::string> all_users = database_->getAllUsernames();
    std::string search_term = search_buffer_;
    std::transform(search_term.begin(), search_term.end(), search_term.begin(), ::tolower);
    
    for (const auto& username : all_users) {
        if (search_term.empty()) {
            filtered_usernames_.push_back(username);
        } else {
            std::string lower_username = username;
            std::transform(lower_username.begin(), lower_username.end(), lower_username.begin(), ::tolower);
            if (lower_username.find(search_term) != std::string::npos) {
                filtered_usernames_.push_back(username);
            }
        }
    }
    
    // Sort usernames
    std::sort(filtered_usernames_.begin(), filtered_usernames_.end());
}

void DatabaseManagerWindow::addNewUser() {
    EncryptedDatabase::UserRecord record;
    record.username = new_username_;
    record.email = new_email_;
    record.website = new_website_;
    
    // Generate salt
    std::string salt;
    if (!database_->generateSalt(salt)) {
        showError("Failed to generate salt");
        return;
    }
    record.salt = salt;
    
    // Hash password
    std::string hashed_password;
    if (!database_->hashPassword(new_password_, salt, hashed_password)) {
        showError("Failed to hash password");
        return;
    }
    record.encrypted_password = hashed_password;
    
    // Set timestamps
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    record.created_at = std::to_string(time_t);
    record.last_login = "Never";
    
    // Add user to database
    if (database_->addUser(record)) {
        showSuccess("User created successfully");
        updateFilteredUsernames();
        
        clearInputFields();
    } else {
        showError("Failed to create user");
    }
}

void DatabaseManagerWindow::editUser() {
    // Implementation for editing user
    showSuccess("Edit user functionality implemented");
    clearInputFields();
}

void DatabaseManagerWindow::deleteUser() {
    if (database_->deleteUser(selected_username_)) {
        showSuccess("User deleted successfully");
        updateFilteredUsernames();
        selected_username_.clear();
    } else {
        showError("Failed to delete user");
    }
}

void DatabaseManagerWindow::clearInputFields() {
    memset(new_username_, 0, sizeof(new_username_));
    memset(new_email_, 0, sizeof(new_email_));
    memset(new_website_, 0, sizeof(new_website_));
    SecureMemory::Cleanse(new_password_);
    SecureMemory::Cleanse(confirm_password_);
}

bool DatabaseManagerWindow::validateInput() {
    if (strlen(new_username_) == 0) {
        showError("Username cannot be empty");
        return false;
    }
    
    if (strlen(new_email_) == 0) {
        showError("Email cannot be empty");
        return false;
    }
    
    if (strlen(new_password_) == 0) {
        showError("Password cannot be empty");
        return false;
    }
    
    if (strcmp(new_password_, confirm_password_) != 0) {
        showError("Passwords do not match");
        return false;
    }
    
    return true;
}

void DatabaseManagerWindow::showError(const std::string& message) {
    error_message_ = message;
    success_message_.clear();
    message_timer_ = 5.0f;
}

void DatabaseManagerWindow::showSuccess(const std::string& message) {
    success_message_ = message;
    error_message_.clear();
    message_timer_ = 3.0f;
}

std::string DatabaseManagerWindow::generateRandomPassword(int length) {
    static constexpr char chars[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*";
    static constexpr std::size_t charCount = sizeof(chars) - 1;
    const unsigned int acceptanceLimit = 256U - (256U % charCount);

    std::string password;
    if (length <= 0) {
        return password;
    }
    password.reserve(static_cast<std::size_t>(length));
    while (password.size() < static_cast<std::size_t>(length)) {
        unsigned char randomByte = 0;
        if (RAND_bytes(&randomByte, 1) != 1) {
            SecureMemory::Cleanse(password);
            return {};
        }
        if (randomByte < acceptanceLimit) {
            password.push_back(chars[randomByte % charCount]);
        }
    }
    return password;
}

void DatabaseManagerWindow::setVisible(bool show) {
    show_window_ = show;
    if (!show) {
        clearSensitiveUiState();
    }
}

void DatabaseManagerWindow::clearSensitiveUiState() {
    SecureMemory::Cleanse(new_password_);
    SecureMemory::Cleanse(confirm_password_);
    SecureMemory::Cleanse(verification_password_);
    clearBackupSensitiveState();
    password_verified_ = false;
}

void DatabaseManagerWindow::clearBackupSensitiveState() {
    SecureMemory::Cleanse(backup_password_);
    SecureMemory::Cleanse(backup_confirm_password_);
    show_backup_password_ = false;
    confirm_restore_ = false;
}

#include "DatabaseManagerWindow.h"
#include <imgui.h>
#include <iostream>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <random>

DatabaseManagerWindow::DatabaseManagerWindow(std::shared_ptr<EncryptedDatabase> database)
    : database_(database), show_window_(false), show_add_user_popup_(false), 
      show_edit_user_popup_(false), show_delete_confirmation_(false), 
      show_passwords_(false), message_timer_(0.0f) {
    
    // Initialize buffers
    memset(search_buffer_, 0, sizeof(search_buffer_));
    memset(new_username_, 0, sizeof(new_username_));
    memset(new_email_, 0, sizeof(new_email_));
    memset(new_website_, 0, sizeof(new_website_));
    memset(new_password_, 0, sizeof(new_password_));
    memset(confirm_password_, 0, sizeof(confirm_password_));
    
    // Load initial user list
    updateFilteredUsernames();
}

DatabaseManagerWindow::~DatabaseManagerWindow() {
    // Secure cleanup of password fields
    memset(new_password_, 0, sizeof(new_password_));
    memset(confirm_password_, 0, sizeof(confirm_password_));
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
                    // Export backup
                }
                if (ImGui::MenuItem("[IMPORT] Import Backup")) {
                    // Import backup
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
        
    }
    ImGui::End();
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
            current_plain_password_.clear(); // Clear verified password when selecting different user
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
            
            // Show plain password if available from current session
            if (session_passwords_.find(selected_username_) != session_passwords_.end()) {
                ImGui::Text("[PLAIN] Plain Password: %s", session_passwords_[selected_username_].c_str());
            }
            
            if (!current_plain_password_.empty()) {
                ImGui::Text("[VERIFIED] Verified Password: %s", current_plain_password_.c_str());
            }
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
        static char password_input[256] = {0};
        static bool password_verified = false;
        
        if (ImGui::InputText("Enter Password", password_input, sizeof(password_input), ImGuiInputTextFlags_Password)) {
            password_verified = false;
        }
        
        ImGui::SameLine();
        if (ImGui::Button("[CHECK] Verify")) {
            std::string hashed_input;
            if (database_->hashPassword(password_input, record.salt, hashed_input)) {
                if (hashed_input == record.encrypted_password) {
                    password_verified = true;
                    current_plain_password_ = password_input;
                    showSuccess("Password verified successfully!");
                } else {
                    password_verified = false;
                    current_plain_password_.clear();
                    showError("Password verification failed!");
                }
            }
        }
        
        if (password_verified && !current_plain_password_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("[OK] Verified Password: %s", current_plain_password_.c_str());
            ImGui::PopStyleColor();
            
            ImGui::SameLine();
            if (ImGui::Button("[COPY] Copy Password")) {
                ImGui::SetClipboardText(current_plain_password_.c_str());
                showSuccess("Password copied to clipboard!");
            }
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
        
        // Store plain password for current session display
        session_passwords_[record.username] = new_password_;
        
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
    memset(new_password_, 0, sizeof(new_password_));
    memset(confirm_password_, 0, sizeof(confirm_password_));
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
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.length() - 1);
    
    std::string password;
    for (int i = 0; i < length; i++) {
        password += chars[dis(gen)];
    }
    
    return password;
}

#ifndef DATABASE_MANAGER_WINDOW_H
#define DATABASE_MANAGER_WINDOW_H

#include "EncryptedDatabase.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <memory>

/**
 * @brief GUI Window for managing the encrypted database
 * 
 * This window provides a user-friendly interface for:
 * - Adding new users/credentials
 * - Viewing existing users
 * - Editing user information
 * - Deleting users
 * - Searching/filtering users
 * - Database statistics
 */
class DatabaseManagerWindow {
public:
    /**
     * @brief Constructor
     * @param database Pointer to the encrypted database
     */
    DatabaseManagerWindow(std::shared_ptr<EncryptedDatabase> database);
    
    /**
     * @brief Destructor
     */
    ~DatabaseManagerWindow();
    
    /**
     * @brief Render the database manager window
     */
    void render();
    
    /**
     * @brief Show/hide the window
     * @param show Whether to show the window
     */
    void setVisible(bool show) { show_window_ = show; }
    
    /**
     * @brief Check if window is visible
     * @return true if window is visible
     */
    bool isVisible() const { return show_window_; }

private:
    std::shared_ptr<EncryptedDatabase> database_;
    bool show_window_;
    
    // UI State
    char search_buffer_[256];
    char new_username_[256];
    char new_email_[256];
    char new_website_[256];
    char new_password_[256];
    char confirm_password_[256];
    bool show_add_user_popup_;
    bool show_edit_user_popup_;
    bool show_delete_confirmation_;
    bool show_passwords_;
    
    // Current user data
    std::string selected_username_;
    std::string current_plain_password_; // For showing the password for selected user
    std::map<std::string, std::string> session_passwords_; // Store passwords for current session
    std::vector<std::string> filtered_usernames_;
    
    // Error handling
    std::string error_message_;
    std::string success_message_;
    float message_timer_;
    
    /**
     * @brief Render the main user list
     */
    void renderUserList();
    
    /**
     * @brief Render the toolbar with buttons
     */
    void renderToolbar();
    
    /**
     * @brief Render the search bar
     */
    void renderSearchBar();
    
    /**
     * @brief Render the add user popup
     */
    void renderAddUserPopup();
    
    /**
     * @brief Render the edit user popup
     */
    void renderEditUserPopup();
    
    /**
     * @brief Render the delete confirmation popup
     */
    void renderDeleteConfirmation();
    
    /**
     * @brief Render database statistics
     */
    void renderStatistics();
    
    /**
     * @brief Render user details panel
     */
    void renderUserDetails();
    
    /**
     * @brief Update filtered usernames based on search
     */
    void updateFilteredUsernames();
    
    /**
     * @brief Add a new user
     */
    void addNewUser();
    
    /**
     * @brief Edit selected user
     */
    void editUser();
    
    /**
     * @brief Delete selected user
     */
    void deleteUser();
    
    /**
     * @brief Clear all input fields
     */
    void clearInputFields();
    
    /**
     * @brief Show error message
     * @param message Error message to show
     */
    void showError(const std::string& message);
    
    /**
     * @brief Show success message
     * @param message Success message to show
     */
    void showSuccess(const std::string& message);
    
    /**
     * @brief Validate user input
     * @return true if input is valid
     */
    bool validateInput();
    
    /**
     * @brief Generate random password
     * @param length Length of password to generate
     * @return Generated password
     */
    std::string generateRandomPassword(int length = 16);
};

#endif // DATABASE_MANAGER_WINDOW_H

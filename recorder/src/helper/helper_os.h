const char* get_os_name();
const char* get_arch();
const char* get_os_version();

#ifdef _WIN32
bool is_safe_mode();
#endif
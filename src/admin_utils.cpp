#include "admin_utils.h"

#include <Windows.h>
#include <shellapi.h>

namespace admin {

bool is_elevated() {
    BOOL elevated = FALSE;
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID administrators = nullptr;
    if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administrators)) {
        CheckTokenMembership(nullptr, administrators, &elevated);
        FreeSid(administrators);
    }
    return elevated == TRUE;
}

bool restart_elevated() {
    wchar_t executable[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, executable, MAX_PATH) == 0) {
        return false;
    }

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.lpVerb = L"runas";
    info.lpFile = executable;
    info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info) == TRUE;
}

} // namespace admin


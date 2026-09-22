#include <exception>
#include <regex>
#include <string>
#include <unordered_map>

#include <kuai/runtime/KuModuleManager.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>
#else
#error Unsupported platform
#endif

namespace {
std::string validateLibraryName(const std::string &fullName) // NOLINT
{
    std::regex  reg;
    std::string libName = fullName.substr(0, fullName.find_last_of('.'));
#ifdef _WIN32
    reg = std::regex(R"(.+\.dll$)");
    if (std::regex_match(fullName, reg) == true)
        return libName;
#elif defined(__MACOSX__) || defined(__APPLE__)
    reg = std::regex(R"(^lib.+\.dylib$)");
    if (std::regex_match(fullName, reg) == true) {
        return libName.substr(3, fullName.find_last_of('.') - 3);
    }
#elif defined(__linux__)
    reg = std::regex(R"(^lib.+\.so$)");
    if (std::regex_match(fullName, reg) == true) {
        return libName.substr(3, fullName.find_last_of('.') - 3);
    }
#endif /*defined(__MACOSX__) || defined(__APPLE__)*/
    return "";
}
} // namespace

namespace kuai {

class KuLibrary::Impl {
    KU_DECL_API(KuLibrary);

public:
    Impl(KuLibrary *api) : q_ptr(api) {
    }
    void *m_lib = nullptr;
};

bool KuLibrary::valid() const {
    if (d_ptr->m_lib == nullptr) {
        return false;
    }
    return true;
}

void *KuLibrary::symbol(const char *name) const {
    if (valid() == false) {
        return nullptr;
    }

#ifdef _WIN32
    return GetProcAddress((HMODULE)m_lib, name);
#else
    return dlsym(d_ptr->m_lib, name);
#endif
}

KuLibrary::KuLibrary() : d_ptr(new Impl(this)) {
}

KuLibrary::KuLibrary(const std::string &path) : d_ptr(new Impl(this)) {
    std::string errorMsg;
#ifdef _WIN32
    m_lib = LoadLibrary(path.c_str());
    if (!m_lib) {
        DWORD  err = GetLastError();
        LPTSTR lpMsgBuf;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                          | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0,
                      NULL);

        errorMsg = lpMsgBuf;
        LocalFree(lpMsgBuf);
    }

#elif defined(__MACOSX__) || defined(__APPLE__)
    d_ptr->m_lib = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!d_ptr->m_lib)
        errorMsg = dlerror();
#elif defined(__linux__)
    d_ptr->m_lib = dlopen(path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (!d_ptr->m_lib)
        errorMsg = dlerror();
#endif /*defined(__MACOSX__) || defined(__APPLE__)*/
    if (!d_ptr->m_lib) {
        throw std::runtime_error(errorMsg);
    }
}

KuLibrary::~KuLibrary() {
    // unload dynamic library is a dangerous operation

    // #ifdef _WIN32
    //     FreeLibrary((HMODULE)d_ptr->lib);
    // #else
    //     dlclose(d_ptr->lib);
    // #endif
}

class KuModuleManager::Impl {
    KU_DECL_API(KuModuleManager);

public:
    Impl(KuModuleManager *api) : q_ptr(api) {
    }
    std::unordered_map<std::string, std::shared_ptr<KuLibrary>> m_libraries;
};

KuModuleManager::KuModuleManager() : d_ptr(new Impl(this)) {
}

std::shared_ptr<KuLibrary> KuModuleManager::loadLibrary(const std::string &name) {
    auto it = d_ptr->m_libraries.find(name);
    if (it != d_ptr->m_libraries.end()) {
        return it->second;
    }
    auto lib = std::make_shared<KuLibrary>(name);
    if (lib->valid() == false) {
        return nullptr;
    }
    d_ptr->m_libraries[name] = lib;
    return lib;
}

KuModuleManager::~KuModuleManager() {
}

} // namespace kuai

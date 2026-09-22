#pragma once
#include <filesystem>
#include <memory>
#include <string>

#include <kuai/core/KuCore.h>
namespace kuai {

class KuLibrary final : public std::enable_shared_from_this<KuLibrary> {
    KU_DECL_IMPL()
public:
    KuLibrary();
    KuLibrary(const std::string &path);
    bool  valid() const;
    void *symbol(const char *name) const;
    ~KuLibrary();
};

class KuModuleManager final {
    KU_DECL_IMPL()
public:
    std::shared_ptr<KuLibrary>   loadLibrary(const std::string &path);
    static std::filesystem::path getKuInstanceModulePath();
    static KuModuleManager      &getKuModuleManager() {
        static KuModuleManager instance;
        return instance;
    }
    ~KuModuleManager();

private:
    KuModuleManager();
};

} // namespace kuai

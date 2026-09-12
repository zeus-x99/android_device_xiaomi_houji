// Loads libraries in an isolated process; never constructs a display object.
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

int checkBindings(int argc, char** argv) {
    if (argc != 3 && !(argc == 4 && std::strcmp(argv[3], "client") == 0)) {
        std::fprintf(stderr, "usage: %s adapter.so original.so [client]\n", argv[0]);
        return 2;
    }
    void* hook = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
    if (!hook) {
        std::fprintf(stderr, "hook: %s\n", dlerror());
        return 3;
    }
    void* core = dlopen(argv[2], RTLD_NOW | RTLD_GLOBAL);
    if (!core) {
        std::fprintf(stderr, "core: %s\n", dlerror());
        return 4;
    }
    if (argc == 4) {
        constexpr char timer[] = "_ZN7android13DisplayEffect13SetQsyncTimerEj";
        void* replacement = dlsym(hook, timer);
        void* real = dlsym(core, timer);
        Dl_info info{};
        if (!replacement || !real || replacement == real ||
            dlsym(RTLD_DEFAULT, timer) != replacement || !dladdr(real, &info)) {
            std::fprintf(stderr, "DisplayEffect timer symbol lookup failed\n");
            return 5;
        }
        // Fixed houji displayfeature.default.so vtable relocation for the timer.
        void* slot;
        std::memcpy(&slot, static_cast<char*>(info.dli_fbase) + 0x5f3d8, sizeof(slot));
        if (slot != replacement) {
            std::fprintf(stderr, "DisplayEffect timer vtable binding failed\n");
            return 6;
        }
        std::puts("PASS: DisplayEffect timer lookup and vtable bind to client adapter");
        std::puts("No DisplayEffect objects created; no display requests made.");
        return 0;
    }
    constexpr const char* names[] = {
        "_ZN3sdm14DisplayBuiltIn18SetQSyncModeLockedENS_9QSyncModeE",
        "_ZN3sdm14DisplayBuiltIn15SetDisplayStateENS_12DisplayStateEbPNSt3__110shared_ptrINS_5FenceEEE",
        "_ZN3sdm8CoreImpl14DestroyDisplayEPNS_16DisplayInterfaceE",
        "_ZN3sdm11DisplayBase15SetDisplayStateENS_12DisplayStateEbPNSt3__110shared_ptrINS_5FenceEEE",
    };
    for (const char* name : names) {
        const void* replacement = dlsym(hook, name);
        const void* real = dlsym(core, name);
        if (!replacement || !real || replacement == real ||
            dlsym(RTLD_DEFAULT, name) != replacement) {
            std::fprintf(stderr, "symbol binding failed: %s\n", name);
            return 5;
        }
    }
    auto table = reinterpret_cast<void**>(dlsym(core, "_ZTVN3sdm14DisplayBuiltInE"));
    if (!table || table[2 + 0x60 / sizeof(void*)] != dlsym(hook, names[1])) {
        if (table) {
            Dl_info actual{};
            dladdr(table[2 + 0x60 / sizeof(void*)], &actual);
            std::fprintf(stderr, "actual power slot: %s %s\n",
                         actual.dli_fname ? actual.dli_fname : "?",
                         actual.dli_sname ? actual.dli_sname : "?");
        }
        std::fprintf(stderr, "power vtable interposition failed\n");
        return 6;
    }
    // This pinned ELF's PLT relocation location for SetQSyncModeLocked.
    Dl_info info{};
    if (!dladdr(dlsym(core, names[0]), &info)) return 7;
    auto slot = reinterpret_cast<void**>(static_cast<char*>(info.dli_fbase) + 0xab8d0);
    if (*slot != dlsym(hook, names[0])) {
        std::fprintf(stderr, "locked-mode PLT interposition failed\n");
        return 8;
    }
    auto powerSlot = reinterpret_cast<void**>(static_cast<char*>(info.dli_fbase) + 0xab3c0);
    if (*powerSlot != dlsym(hook, names[3])) {
        std::fprintf(stderr, "base-power PLT interposition failed\n");
        return 9;
    }
    std::puts("PASS: global/original lookup, power vtable, locked-mode and base-power PLT");
    if (auto abi = reinterpret_cast<int (*)()>(dlsym(hook, "houji_qsync_abi_supported"))) {
        if (abi() != 1) {
            std::fprintf(stderr, "runtime ABI/startup gate rejected binding\n");
            return 10;
        }
        std::puts("PASS: runtime ELF build ID and mode/power/init/deinit binding gate");
    }
    std::puts("No display objects created; no modes requested. Not a runtime/CFI test.");
    // Keep dependencies loaded until process exit, as in the actual service.
    return 0;
}

int main(int argc, char** argv) {
    const int status = checkBindings(argc, argv);
    std::fprintf(stdout, "Binding check status=%d\n", status);
    std::fflush(nullptr);
    // These HAL libraries run for the daemon's lifetime. Some original vendor
    // static destructors double-destroy a mutex at normal process exit. This
    // probe tests relocation binding, not those unrelated shutdown callbacks.
    _exit(status);
}

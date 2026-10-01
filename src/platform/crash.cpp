// Windows crash reports: an unhandled exception writes crash.txt (the
// exception, the registers and the code addresses found on the stack, as
// offsets into ancientevil.exe) to the save folder, and says where it is.
// With the unstripped exe of the same build the offsets map to source lines
// (addr2line -e ancientevil.exe 0x140000000+offset).
#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <string>
#include "fileio.h"

static char gCrashPath[MAX_PATH * 2];

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS *ep)
{
    HMODULE exe = GetModuleHandleA(nullptr);
    uintptr_t base = (uintptr_t)exe;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)exe)->e_lfanew);
    uintptr_t end = base + nt->OptionalHeader.SizeOfImage;
    FILE *f = fopen(gCrashPath, "w");
    if (f) {
        EXCEPTION_RECORD *er = ep->ExceptionRecord;
        CONTEXT *c = ep->ContextRecord;
        fprintf(f, "Open AncientEvil crash report\n");
        fprintf(f, "exception %08lx at %p", (unsigned long)er->ExceptionCode, er->ExceptionAddress);
        uintptr_t a = (uintptr_t)er->ExceptionAddress;
        if (a >= base && a < end) fprintf(f, " (exe+0x%llx)", (unsigned long long)(a - base));
        fprintf(f, "\n");
        if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
            fprintf(f, "%s address %p\n", er->ExceptionInformation[0] ? "writing" : "reading",
                    (void *)er->ExceptionInformation[1]);
        fprintf(f, "exe base %p size %llx thread %lu\n", (void *)base, (unsigned long long)(end - base),
                GetCurrentThreadId());
#ifdef _WIN64
        fprintf(f, "rip %llx rsp %llx rbp %llx\nrax %llx rbx %llx rcx %llx rdx %llx\n"
                   "rsi %llx rdi %llx r8 %llx r9 %llx\n",
                (unsigned long long)c->Rip, (unsigned long long)c->Rsp, (unsigned long long)c->Rbp,
                (unsigned long long)c->Rax, (unsigned long long)c->Rbx, (unsigned long long)c->Rcx,
                (unsigned long long)c->Rdx, (unsigned long long)c->Rsi, (unsigned long long)c->Rdi,
                (unsigned long long)c->R8, (unsigned long long)c->R9);
        // code addresses on the stack (return addresses, mostly)
        fprintf(f, "stack:\n");
        uintptr_t *sp = (uintptr_t *)c->Rsp;
        int n = 0;
        for (int i = 0; i < 4096 && n < 64; i++) {
            MEMORY_BASIC_INFORMATION mi;
            if ((i & 511) == 0 && (!VirtualQuery(sp + i, &mi, sizeof mi) || mi.State != MEM_COMMIT)) break;
            uintptr_t v = sp[i];
            if (v > base + 0x1000 && v < end) {
                fprintf(f, "  exe+0x%llx\n", (unsigned long long)(v - base));
                n++;
            }
        }
#endif
        fclose(f);
    }
    std::string msg = "Open AncientEvil has crashed.\n\nA crash report was written to:\n";
    msg += gCrashPath;
    msg += "\n\nPlease send it along with what you were doing.";
    MessageBoxA(nullptr, msg.c_str(), "Open AncientEvil", MB_OK | MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

void plat_install_crash_handler()
{
    std::string p = fileio_save_dir();
    if (p.empty()) p = ".";
    p += "\\crash.txt";
    snprintf(gCrashPath, sizeof gCrashPath, "%s", p.c_str());
    SetUnhandledExceptionFilter(CrashFilter);
}
#else
void plat_install_crash_handler() {}
#endif

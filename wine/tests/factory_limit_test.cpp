// Run the EXE's actual access-limit instructions in isolation, without loading
// the Programmer, initializing its transport, or opening CAN.
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <cstdint>

bool read_at(const char* path, long offset, void* out, std::size_t size) {
    FILE* file=std::fopen(path,"rb");
    if(!file) return false;
    const bool ok=std::fseek(file,offset,SEEK_SET)==0 && std::fread(out,1,size,file)==size;
    std::fclose(file);
    return ok;
}
int main(int argc,char** argv) {
    if(argc!=3) return 1;
    auto* state=static_cast<unsigned char*>(VirtualAlloc(nullptr,
        4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    auto* code=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,
        MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!state || !code) {
        std::printf("Allocation failed: error=%lu state=%p code=%p\n",GetLastError(),state,code);
        return 2;
    }
    auto* limit=reinterpret_cast<unsigned*>(state);
    using Run=unsigned (*)();
    Run run;
    std::memcpy(&run,&code,sizeof(run));
    for(int app=1;app<=2;++app) {
        DWORD old=0;
        VirtualProtect(code,4096,PAGE_READWRITE,&old);
        if(!read_at(argv[app],0x18b2ef,code,10)) return 3;
        // Relocate only the absolute address of the global to test-owned
        // memory; the EXE instruction/opcode and limit immediate stay intact.
        const auto globalAddress=reinterpret_cast<std::uintptr_t>(limit);
        std::memcpy(code+2,&globalAddress,4);
        code[10]=0xc3; // ret after mov [0x006ce33c], immediate
        if(!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)) return 4;
        FlushInstructionCache(GetCurrentProcess(),code,11);
        run();
        const unsigned expected=app==1?2:3;
        if(*limit!=expected) return 5;
        // Original min(application limit, dongle level) block, VA 0x005dd643.
        VirtualProtect(code,4096,PAGE_READWRITE,&old);
        const unsigned char prologue[]={0x53,0x55,0x89,0xe5,0x83,0xec,0x20,0xbb};
        std::memcpy(code,prologue,sizeof(prologue));
        std::array<unsigned char,0x16ec> dongle{};
        const auto pointer=reinterpret_cast<std::uintptr_t>(dongle.data());
        std::memcpy(code+8,&pointer,4);
        if(!read_at(argv[app],0x1dca43,code+12,29)) return 6;
        std::memcpy(code+13,&globalAddress,4); // mov eax,[same test global]
        const unsigned char epilogue[]={0x89,0xec,0x5d,0x5b,0xc3};
        std::memcpy(code+41,epilogue,sizeof(epilogue));
        if(!VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)) return 7;
        FlushInstructionCache(GetCurrentProcess(),code,46);
        for(unsigned level=1;level<=3;++level) {
            std::memcpy(dongle.data()+0x16e8,&level,4);
            const unsigned effective=run();
            const unsigned expectedEffective=level<expected?level:expected;
            std::printf("%s: dongle=%u effective=%u\n",app==1?"OEM original":"Factory copy",level,effective);
            if(effective!=expectedEffective) return 8;
        }
    }
    VirtualFree(code,0,MEM_RELEASE);
    VirtualFree(state,0,MEM_RELEASE);
    return 0;
}

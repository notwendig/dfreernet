#include <windows.h>
#include "DongleMessages.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
using namespace dongle_recovered;
using Status = unsigned long;
using Open = Status (WINAPI*)(int, void**);
using Write = Status (WINAPI*)(void*, void*, DWORD, DWORD*);
using Read = Status (WINAPI*)(void*, void*, DWORD, DWORD*);
using Close = Status (WINAPI*)(void*);
std::array<unsigned char,24> wire(const CRawMsg& raw) {
    std::array<unsigned char,24> f{};
    f[0]=0x10; f[1]=2; f[22]=0x10; f[23]=0xfe;
    for(int i=0;i<19;++i) { f[2+i]=raw[i]; f[21]^=raw[i]; }
    return f;
}
template<class T> T api(HMODULE dll, const char* name) {
    auto address=GetProcAddress(dll,name);
    T fn;
    static_assert(sizeof(fn)==sizeof(address));
    std::memcpy(&fn,&address,sizeof(fn));
    return fn;
}
LONG CALLBACK continue_test_exception(EXCEPTION_POINTERS* e) {
    return e->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION
        ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}
int main(int argc, char** argv) {
    auto dll=LoadLibraryA("ftd2xx.dll");
    if(!dll) return 1;
    auto open=api<Open>(dll,"FT_Open");
    auto write=api<Write>(dll,"FT_Write");
    auto read=api<Read>(dll,"FT_Read");
    auto close=api<Close>(dll,"FT_Close");
    if(!open || !write || !read || !close) return 2;
    void* h=nullptr;
    if(argc==2 && std::strcmp(argv[1],"--expect-rejected")==0) {
        const auto status=open(0,&h);
        return status==2 && h==nullptr ? 0 : 16; // FT_DEVICE_NOT_FOUND
    }
    if((argc!=3 && argc!=4) || open(0,&h)) return 3;
    if(argc==4 && std::strcmp(argv[3],"--diagnostic-probe")==0) {
        // A continuable exception verifies diagnostics without crashing Wine
        // or replacing the Programmer's normal exception handling.
        auto handler=AddVectoredExceptionHandler(0,continue_test_exception);
        if(!handler) return 17;
        RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
        RemoveVectoredExceptionHandler(handler);
    }
    DWORD n=0;
    CUSBMsg usb; usb.SetCmdReadFW();
    auto f=wire(CRawMsg(usb));
    if(write(h,f.data(),3,&n) || n!=3) return 4;
    if(write(h,f.data()+3,21,&n) || n!=21) return 5;
    unsigned char rx[4096]{};
    if(read(h,rx,24,&n) || n!=24 || rx[3]!=5 || rx[4]!=3 || rx[5]!=4 || rx[6]!=0x4b) return 6;
    // Read the identity through the same CUSBMsg commands as the Programmer.
    // Expected words check USB word byte order and the selected profile CRC.
    const unsigned short identity[12] = {
        0x000c,0x0101,0x05b0,0xb014,0,0,static_cast<unsigned short>(std::strtoul(argv[1],nullptr,10)),
        0,0,0,0,static_cast<unsigned short>(std::strtoul(argv[2],nullptr,16))
    };
    for(unsigned i=0;i<12;++i) {
        usb.SetCmdReadEEPROM(0x80+i);
        f=wire(CRawMsg(usb));
        if(write(h,f.data(),24,&n) || read(h,rx,24,&n) || n!=24 ||
           rx[2]!=0 || rx[3]!=7 || rx[4]!=0) return 14;
        const unsigned value=(unsigned(rx[5])<<8)|rx[6];
        if(value!=identity[i]) return 15;
    }
    f=wire(CRawMsg(usb));
    // Invalid checksum and invalid DLC must not reach the proxy.
    f[21]^=1; write(h,f.data(),24,&n);
    CRawMsg bad; bad[0]=1; bad[13]=15; f=wire(bad); write(h,f.data(),24,&n);
    for (bool extended : {false,true}) for (bool remote : {false,true}) {
        CCANMsg can; can.SetID(extended ? 0x123 : 0x790);
        can.SetExtended(extended); can.SetRemote(remote); can.SetDLC(remote ? 0 : 8);
        for(int i=0;i<8;++i) can[i]=i+1;
        f=wire(CRawMsg(can));
        if(write(h,f.data(),24,&n) || n!=24) return 7;
    }
    // Three-frame compact transmission, fragmented across FT_Write calls.
    std::array<unsigned char,47> burst{};
    burst[0]=0x10; burst[1]=1; burst[45]=0x10; burst[46]=0xfe;
    for(int j=0;j<3;++j) {
        CCANMsg can; can.SetID(0x180+j); can.SetDLC(0);
        CRawMsg raw(can);
        for(int i=0;i<14;++i) { burst[2+j*14+i]=raw[i]; burst[44]^=raw[i]; }
    }
    write(h,burst.data(),17,&n); write(h,burst.data()+17,30,&n);
    // Wait for a fake proxy response: extended low ID + RTR must survive RX.
    bool found=false;
    bool unknownRepository=false;
    std::vector<unsigned char> stream;
    for(int tries=0;tries<500 && (!found || !unknownRepository);++tries) {
        if(read(h,rx,sizeof(rx),&n)) return 8;
        stream.insert(stream.end(),rx,rx+n);
        while(stream.size()>=24) {
            CRawMsg raw; for(int i=0;i<19;++i) raw.AddByte(stream[2+i]);
            if(raw.IsCANMsg()) {
                CCANMsg can(raw);
                found=can.GetID()==0x123 && can.IsExtended() && can.IsRemote() && can.GetDLC()==0;
            }
            if(raw.IsUSBMsg()) {
                CUSBMsg status(raw);
                if(status.IsStatus_0()) unknownRepository=status.GetByteData(2)==0xff;
            }
            stream.erase(stream.begin(),stream.begin()+24);
        }
        Sleep(10);
    }
    if(!found || !unknownRepository) return 9;
    // Filter initialization must keep the unknown-node marker in its immediate
    // and periodic status packets, rather than pinning the Programmer to 0.
    usb.SetCmdWriteFilterMode(1);
    f=wire(CRawMsg(usb));
    if(write(h,f.data(),24,&n)) return 10;
    bool immediate=false, periodic=false;
    for(int tries=0;tries<100 && !periodic;++tries) {
        if(read(h,rx,sizeof(rx),&n)) return 11;
        stream.insert(stream.end(),rx,rx+n);
        while(stream.size()>=24) {
            CRawMsg raw; for(int i=0;i<19;++i) raw.AddByte(stream[2+i]);
            if(raw.IsUSBMsg()) {
                CUSBMsg status(raw);
                if(status.IsStatus_0()) {
                    if(status.GetByteData(2)!=0xff) return 12;
                    immediate |= status.GetByteData(0)==1;
                    periodic |= status.GetByteData(0)==3;
                }
            }
            stream.erase(stream.begin(),stream.begin()+24);
        }
        Sleep(10);
    }
    close(h);
    if(!immediate || !periodic) return 13;
    std::puts("DLL USB, fragmented/burst CAN TX and CAN RX passed");
    return 0;
}

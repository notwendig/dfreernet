#include "device.hpp"
#include <array>
#include <algorithm>
#include <cstring>
namespace rnet {
DeviceScan DeviceDiscovery::scan(const std::filesystem::path& original_library)const{
    DeviceScan r;
    // Optional legacy analysis action only. The CAN path never loads this DLL.
    HMODULE library=LoadLibraryExW(original_library.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    if(!library){r.library_status=r.status=1;return r;}
    struct Release {HMODULE handle;~Release(){FreeLibrary(handle);}} release{library};
    ULONG(WINAPI* FT_ListDevices)(PVOID,PVOID,DWORD)=nullptr;
    ULONG(WINAPI* FT_GetLibraryVersion)(LPDWORD)=nullptr;
    auto list=GetProcAddress(library,"FT_ListDevices"),version=GetProcAddress(library,"FT_GetLibraryVersion");
    static_assert(sizeof(list)==sizeof(FT_ListDevices)&&sizeof(version)==sizeof(FT_GetLibraryVersion));
    std::memcpy(&FT_ListDevices,&list,sizeof(list));std::memcpy(&FT_GetLibraryVersion,&version,sizeof(version));
    if(!FT_ListDevices||!FT_GetLibraryVersion){r.library_status=r.status=1;return r;}
    auto st=FT_GetLibraryVersion(&r.library_version);r.library_status=st;
    if(st){r.status=st;return r;}
    DWORD n=0;r.enumeration_attempted=true;
    r.status=FT_ListDevices(&n,nullptr,0x80000000UL);
    if(r.status)return r;
    if(n>1024){r.status=6;return r;}
    for(DWORD i=0;i<n;++i){
        std::array<char,64> serial{},description{};
        auto index=reinterpret_cast<void*>(static_cast<ULONG_PTR>(i));
        st=FT_ListDevices(index,serial.data(),0x40000001UL);
        if(st){r.status=st;return r;}
        st=FT_ListDevices(index,description.data(),0x40000002UL);
        if(st){r.status=st;return r;}
        serial.back()=description.back()=0;
        r.devices.push_back({serial.data(),description.data()});
    }
    return r;
}
}

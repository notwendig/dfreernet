#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
BOOL CALLBACK children(HWND,LPARAM);
void text_of(HWND window,wchar_t* text,unsigned capacity){DWORD_PTR result=0;SendMessageTimeoutW(window,WM_GETTEXT,capacity,reinterpret_cast<LPARAM>(text),SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&result);}
struct Search {HWND main=nullptr,save=nullptr,warning=nullptr,open=nullptr;};
BOOL CALLBACK windows(HWND h,LPARAM data){
 wchar_t title[1024]{},cls[256]{};GetWindowTextW(h,title,1024);GetClassNameW(h,cls,256);
 if(!IsWindowVisible(h))return TRUE;
 std::wcout<<L"WINDOW "<<h<<L" class="<<cls<<L" title="<<title<<L'\n';
 if(std::wstring(title)==L"Open")reinterpret_cast<Search*>(data)->open=h;
 if(std::wstring(title)==L"R-net Warning"||std::wstring(title)==L"Warning")reinterpret_cast<Search*>(data)->warning=h;
 if(std::wstring(title)==L"Save As"&&IsWindowEnabled(h))reinterpret_cast<Search*>(data)->save=h;
 if(std::wstring(cls)==L"#32770")EnumChildWindows(h,children,0);
 if(std::wstring(title).find(L"PGDT Rnet Programmer")!=std::wstring::npos ||
    (std::wstring(cls).rfind(L"Afx:00400000:b:",0)==0&&!GetWindow(h,GW_OWNER)))reinterpret_cast<Search*>(data)->main=h;
 return TRUE;
}
BOOL CALLBACK children(HWND h,LPARAM){wchar_t t[1024]{},c[256]{};text_of(h,t,1024);GetClassNameW(h,c,256);std::wcout<<L" CHILD "<<h<<L" id="<<GetDlgCtrlID(h)<<L" class="<<c<<L" text="<<t<<L'\n';return TRUE;}
int main(int argc,char**argv){Search s;EnumWindows(windows,reinterpret_cast<LPARAM>(&s));if(argc==2&&std::string(argv[1])=="continue-warning"){if(!s.warning)return 5;PostMessageW(s.warning,WM_COMMAND,IDOK,0);return 0;}if(!s.main)return 2;
 if(argc==3&&(std::string(argv[1])=="open-path"||std::string(argv[1])=="save-path")){
  auto dialog=std::string(argv[1])=="open-path"?s.open:s.save;
  if(!dialog)return 6;int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[2],-1,nullptr,0);if(!length)return 7;
  std::wstring path(length,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[2],-1,path.data(),length);
  SetDlgItemTextW(dialog,1152,path.c_str());PostMessageW(dialog,WM_COMMAND,IDOK,0);return 0;
 }
 if(argc==2&&std::string(argv[1])=="open-roundtrip"){if(!s.open)return 6;SetDlgItemTextW(s.open,1152,L"C:\\cpp-roundtrip.R-net");PostMessageW(s.open,WM_COMMAND,IDOK,0);return 0;}
 if(argc==2&&std::string(argv[1])=="save-reference"){if(!s.save)return 4;SetDlgItemTextW(s.save,1152,L"C:\\reference-default.R-net");PostMessageW(s.save,WM_COMMAND,IDOK,0);return 0;}
 if(argc==2){unsigned command=std::stoul(argv[1]);switch(command){case 32803:{
   wchar_t enabled[8]{},prefix[4096]{};GetEnvironmentVariableW(L"RNET_ISOLATED_EMULATOR_TEST",enabled,8);GetEnvironmentVariableW(L"WINEPREFIX",prefix,4096);
   if(std::wstring(enabled)!=L"1"||std::wstring(prefix).rfind(L"/tmp/rnet-emulator-",0)!=0)return 8;break;
  }case 32800:case 57601:case 57603:case 57604:case 57636:case 32844:case 32831:case 57664:case 57665:break;default:return 3;}PostMessageW(s.main,WM_COMMAND,command,0);}
 EnumChildWindows(s.main,children,0);return 0;}

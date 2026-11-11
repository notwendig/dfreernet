#include "pe_image.hpp"
#include "rnd_decoder.hpp"
#include "database.hpp"
#include "document.hpp"
#include "values.hpp"
#include "device.hpp"
#include "can_udp.hpp"
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <algorithm>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <thread>
#include <cwctype>
#include <set>
#include <chrono>
#include <iomanip>
namespace {
constexpr UINT ID_RESOURCES=40001,ID_DATABASE=40002,ID_SCAN=40003,ID_REPORT=40004,ID_FILTER=40005,ID_SHOW_DETAILS=40006,ID_HIDE_DETAILS=40007,ID_PARAMETERS=40008;
constexpr UINT ID_VALUES=40009,ID_REPOSITORY=40010,ID_EDIT_VALUE=40011;
constexpr UINT ID_DONGLE_FIRST=40020,ID_DONGLE_LAST=40023;
constexpr UINT ID_CAN_READ=40024,CAN_READ_DONE=WM_APP+3,CAN_PROGRESS=WM_APP+4,CAN_TEST=WM_APP+5;
constexpr UINT ID_CAN_MONITOR=40025,CAN_MONITOR_DONE=WM_APP+6,CAN_MONITOR_PROGRESS=WM_APP+7,CAN_MONITOR_START=WM_APP+8;
constexpr UINT SCAN_DONE=WM_APP+1,SELF_TEST=WM_APP+2;
const wchar_t* CLASS_NAME=L"RNetReconstructionDevelopment";
bool restore_client_cursor(HWND target,LPARAM position){
    if(LOWORD(position)!=HTCLIENT)return false; // Keep native resize/title-bar cursors.
    wchar_t name[32]{};GetClassNameW(target,name,32);
    auto cursor=LoadCursorW(nullptr,_wcsicmp(name,L"Edit")==0?IDC_IBEAM:IDC_ARROW);
    if(!cursor)return false;
    SetCursor(cursor);
    CURSORINFO info{};info.cbSize=sizeof(info);
    if(GetCursorInfo(&info)&&!(info.flags&CURSOR_SHOWING)){
        // Repair a hidden cursor without increasing the counter on every move.
        for(int attempt=0;attempt<16&&ShowCursor(TRUE)<0;++attempt){}
    }
    return true;
}
struct MonitorChoice {HMONITOR monitor=nullptr;RECT work{};bool secondary=false;};
BOOL CALLBACK choose_monitor(HMONITOR monitor,HDC,LPRECT,LPARAM parameter){
    auto& choice=*reinterpret_cast<MonitorChoice*>(parameter);
    MONITORINFO info{};info.cbSize=sizeof(info);if(!GetMonitorInfoW(monitor,&info))return TRUE;
    bool secondary=(info.dwFlags&MONITORINFOF_PRIMARY)==0;
    if(!choice.monitor||(secondary&&!choice.secondary)||(secondary==choice.secondary&&info.rcWork.left<choice.work.left))
        choice={monitor,info.rcWork,secondary};
    return TRUE;
}
MonitorChoice preferred_monitor(){
    MonitorChoice choice;EnumDisplayMonitors(nullptr,nullptr,choose_monitor,reinterpret_cast<LPARAM>(&choice));
    if(!choice.monitor)throw std::runtime_error("Cannot determine application monitor");
    return choice;
}
void center_owned_dialog(HWND dialog){
    MONITORINFO info{};info.cbSize=sizeof(info);
    if(!GetMonitorInfoW(MonitorFromWindow(GetParent(dialog),MONITOR_DEFAULTTONEAREST),&info))return;
    RECT rectangle{};if(!GetWindowRect(dialog,&rectangle))return;
    int width=rectangle.right-rectangle.left,height=rectangle.bottom-rectangle.top;
    SetWindowPos(dialog,nullptr,info.rcWork.left+std::max(0L,(info.rcWork.right-info.rcWork.left-width)/2),info.rcWork.top+std::max(0L,(info.rcWork.bottom-info.rcWork.top-height)/2),0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
}
std::wstring wide(const std::u16string&s){return std::wstring(s.begin(),s.end());}
std::wstring wide8(const std::string&s){
    if(s.empty())return {};
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);
    if(!n)return L"[Text nicht UTF-8]";
    std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),w.data(),n);return w;
}
std::string narrow(const std::wstring&s){
    if(s.empty())return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);
    std::string b(n,0);WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),b.data(),n,nullptr,nullptr);return b;
}
std::wstring lower(std::wstring s){for(auto&c:s)c=wchar_t(towlower(c));return s;}
void write_new_file(const std::filesystem::path& path,const rnet::Bytes& bytes){
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create new destination; existing files are preserved");
    DWORD written=0;bool ok=bytes.size()<=MAXDWORD&&WriteFile(file,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file);
    if(!CloseHandle(file))ok=false;
    if(!ok){DeleteFileW(path.c_str());throw std::runtime_error("Document write failed");}
}
struct Row {std::wstring id,text,details,group;std::wstring stored{},location{};};
struct ValueDialog {
    std::wstring label,text;
    bool automated=false;
    std::function<void(const std::u16string&)> apply;
    static INT_PTR CALLBACK procedure(HWND h,UINT m,WPARAM w,LPARAM l){
        auto context=reinterpret_cast<ValueDialog*>(GetWindowLongPtrW(h,DWLP_USER));
        if(m==WM_INITDIALOG){context=reinterpret_cast<ValueDialog*>(l);SetWindowLongPtrW(h,DWLP_USER,l);center_owned_dialog(h);SetDlgItemTextW(h,100,context->label.c_str());SetDlgItemTextW(h,101,context->text.c_str());SendDlgItemMessageW(h,101,EM_SETSEL,0,-1);SetFocus(GetDlgItem(h,101));if(context->automated){SetDlgItemTextW(h,101,L"87");PostMessageW(h,WM_COMMAND,IDOK,0);}return FALSE;}
        if(m==WM_SETCURSOR&&restore_client_cursor(reinterpret_cast<HWND>(w),l))return TRUE;
        if(m==WM_COMMAND&&LOWORD(w)==IDCANCEL){EndDialog(h,IDCANCEL);return TRUE;}
        if(m==WM_COMMAND&&LOWORD(w)==IDOK&&context){
            auto length=GetWindowTextLengthW(GetDlgItem(h,101));std::wstring input(length+1,0);GetDlgItemTextW(h,101,input.data(),length+1);input.resize(length);
            try{context->apply(std::u16string(input.begin(),input.end()));EndDialog(h,IDOK);}catch(const std::exception& e){MessageBoxW(h,wide8(e.what()).c_str(),L"Wert nicht übernommen",MB_OK|MB_ICONERROR);SetFocus(GetDlgItem(h,101));}return TRUE;
        }
        return FALSE;
    }
};
class Application {
    HINSTANCE instance_;
    HACCEL accelerators_=nullptr;
    HWND window_=nullptr,list_=nullptr,filter_=nullptr,details_=nullptr,status_=nullptr,banner_=nullptr,tree_=nullptr;
    HWND edit_button_=nullptr,undo_button_=nullptr;
    std::filesystem::path installation_,output_,startup_document_;
    std::unique_ptr<rnet::PeImage> original_;
    rnet::ParameterCatalog catalog_;
    std::unique_ptr<rnet::RnetDocument> document_;
    std::filesystem::path document_path_;
    std::vector<rnet::DocumentValue> values_;
    std::vector<std::string> value_warnings_;
    bool value_view_=false;
    rnet::AccessLevel dongle_level_=rnet::AccessLevel::OEM;
    std::wstring status_text_;
    std::vector<rnet::Bytes> undo_;
    rnet::Bytes saved_document_;
    bool owned_document_=false;
    std::vector<Row> rows_;
    std::map<std::wstring,HTREEITEM> group_handles_;
    std::vector<std::wstring> group_paths_;
    std::wstring selected_group_;
    HTREEITEM group_root_=nullptr;
    bool parameter_view_=false;
    std::vector<std::size_t> displayed_;
    std::ofstream log_;
    std::thread scan_thread_;
    rnet::DeviceScan scan_result_;
    bool scanning_=false,self_test_=false,self_test_scan_pending_=false;
    int exit_code_=0;
    unsigned commands_=0;
    std::thread can_thread_;
    std::atomic<bool> can_cancelled_{false};
    bool can_busy_=false,can_test_=false;
    std::filesystem::path can_plan_,can_target_;
    std::vector<rnet::RepositoryFile> can_files_;
    std::string can_error_;
    struct CanObservation {unsigned count=0;rnet::Bytes latest;};
    std::map<std::uint32_t,CanObservation> can_observed_;
    unsigned can_frame_count_=0;
    std::filesystem::path can_monitor_target_;
public:
    Application(HINSTANCE h,std::filesystem::path original,std::filesystem::path output,bool selftest,std::filesystem::path document={})
        :instance_(h),installation_(std::move(original)),output_(std::move(output)),startup_document_(std::move(document)),self_test_(selftest){
        std::filesystem::create_directories(output_);
        log_.open(output_/"runtime.log",std::ios::app);
        if(!log_)throw std::runtime_error("Cannot create runtime log");
        record("START C++17 PE32 development reconstruction; implementation incomplete");
        original_=std::make_unique<rnet::PeImage>(rnet::read_file(installation_/L"RNet Programmer6 OEM.exe"));
        record("Original resources parsed, entry="+std::to_string(original_->entry_point()));
    }
    ~Application(){can_cancelled_=true;if(can_thread_.joinable())can_thread_.join();if(scan_thread_.joinable())scan_thread_.join();if(accelerators_)DestroyAcceleratorTable(accelerators_);}
    void emulator_test(std::filesystem::path plan,std::filesystem::path target){can_plan_=std::move(plan);can_target_=std::move(target);can_test_=true;}
    void monitor_on_start(std::filesystem::path target){can_monitor_target_=std::move(target);}
    void record(const std::string&s){log_<<GetTickCount64()<<" "<<s<<'\n';log_.flush();}
    void show_error(const std::exception&e){record(std::string("ERROR ")+e.what());MessageBoxW(window_,wide8(e.what()).c_str(),L"R-net Rekonstruktion",MB_ICONERROR);}
    void set_status(const std::wstring&s){
        status_text_=s;
        auto text=L"Dongle: "+wide8(rnet::access_level_name(dongle_level_))+L" | "+s;
        SendMessageW(status_,SB_SETTEXTW,0,reinterpret_cast<LPARAM>(text.c_str()));
    }
    void select_dongle_level(rnet::AccessLevel level){
        if(unsigned(level)>unsigned(rnet::AccessLevel::Factory))throw std::runtime_error("Unknown access level");
        dongle_level_=level;
        CheckMenuRadioItem(GetMenu(window_),ID_DONGLE_FIRST,ID_DONGLE_LAST,ID_DONGLE_FIRST+unsigned(level),MF_BYCOMMAND);
        update_edit_controls();set_status(status_text_);DrawMenuBar(window_);
        record(std::string("DONGLE_ACCESS_SELECTED ")+rnet::access_level_name(level)+"; local document editing only");
    }
    void update_edit_controls(){
        int selected=ListView_GetNextItem(list_,-1,LVNI_SELECTED);
        bool editable=false;
        if(value_view_&&document_&&selected>=0&&std::size_t(selected)<displayed_.size()){
            auto index=displayed_[selected];
            if(index<values_.size())editable=rnet::access_allows(catalog_.parameters.at(values_[index].parameter).metadata[1]>>5,dongle_level_);
        }
        EnableWindow(edit_button_,editable);
        EnableMenuItem(GetMenu(window_),ID_EDIT_VALUE,MF_BYCOMMAND|(editable?MF_ENABLED:MF_GRAYED));
    }
    bool enabled(UINT id)const{
        return id==57665||id==57664||id==57636||id==57601||id==57603||id==57643||id==32844||id==32831;
    }
    HMENU create_menu(const std::vector<rnet::MenuItem>& entries,bool root=false){
        HMENU menu=root?CreateMenu():CreatePopupMenu();
        if(!menu)throw std::runtime_error("CreateMenu failed");
        for(const auto&e:entries){
            if(!e.children.empty())AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(create_menu(e.children)),wide(e.text).c_str());
            else if(e.text.empty())AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
            else AppendMenuW(menu,MF_STRING|(enabled(e.id)?MF_ENABLED:MF_GRAYED),e.id,wide(e.text).c_str());
        }
        return menu;
    }
    HWND control(const wchar_t* cls,const wchar_t* text,DWORD style,UINT id){
        auto w=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,window_,reinterpret_cast<HMENU>(UINT_PTR(id)),instance_,nullptr);
        if(!w)throw std::runtime_error("Control creation failed");
        SendMessageW(w,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return w;
    }
    void created(){
        auto resources=original_->resources();
        auto it=std::find_if(resources.begin(),resources.end(),[](auto&r){return r.type=="4"&&r.name=="675";});
        if(it==resources.end())throw std::runtime_error("Original OEM menu resource 675 missing");
        auto menu=create_menu(rnet::PeImage::menu(it->bytes),true);
        for(const auto& r:resources)if(r.type=="9"&&r.name=="675"){
            if(r.bytes.size()%8)throw std::runtime_error("Invalid original accelerator resource");
            auto word=[&](std::size_t p){return std::uint16_t(r.bytes.at(p))|(std::uint16_t(r.bytes.at(p+1))<<8);};
            std::vector<ACCEL> entries;
            for(std::size_t p=0;p<r.bytes.size();p+=8){auto command=word(p+4);if(enabled(command))entries.push_back({BYTE(word(p)&0x7f),WORD(word(p+2)),WORD(command)});}
            if(!entries.empty())accelerators_=CreateAcceleratorTableW(entries.data(),int(entries.size()));
            if(!accelerators_)throw std::runtime_error("Original accelerator table creation failed");
        }
        auto analysis=CreatePopupMenu();
        AppendMenuW(analysis,MF_STRING,ID_PARAMETERS,L"Parameterkatalog anzeigen");
        AppendMenuW(analysis,MF_STRING,ID_VALUES,L"Gespeicherte Parameterwerte anzeigen");
        AppendMenuW(analysis,MF_STRING,ID_REPOSITORY,L"Repository-Dateien anzeigen");
        AppendMenuW(analysis,MF_STRING,ID_EDIT_VALUE,L"Ausgewählten Rohwert bearbeiten...");
        AppendMenuW(analysis,MF_STRING,ID_RESOURCES,L"Originaltexte anzeigen");
        AppendMenuW(analysis,MF_STRING,ID_DATABASE,L"RND-Datei entschlüsseln / prüfen...");
        AppendMenuW(analysis,MF_STRING,ID_SHOW_DETAILS,L"Textdetails anzeigen");
        AppendMenuW(analysis,MF_STRING,ID_HIDE_DETAILS,L"Textdetails ausblenden");
        AppendMenuW(analysis,MF_STRING,ID_SCAN,L"FTDI-Geräte auflisten (ohne Verbindung)");
        AppendMenuW(analysis,MF_STRING,ID_REPORT,L"Ansicht als CSV exportieren...");
        auto dongle=CreatePopupMenu();
        for(unsigned level=0;level<=unsigned(rnet::AccessLevel::Factory);++level){
            auto name=wide8(rnet::access_level_name(static_cast<rnet::AccessLevel>(level)));
            AppendMenuW(dongle,MF_STRING,ID_DONGLE_FIRST+level,name.c_str());
        }
        AppendMenuW(dongle,MF_SEPARATOR,0,nullptr);
        AppendMenuW(dongle,MF_STRING,ID_CAN_READ,L"Vom CAN-Emulator lesen...");
        AppendMenuW(dongle,MF_STRING,ID_CAN_MONITOR,L"Echten CAN mitlesen (10 Sekunden)...");
        AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(dongle),L"&Dongle");
        CheckMenuRadioItem(dongle,ID_DONGLE_FIRST,ID_DONGLE_LAST,ID_DONGLE_FIRST+unsigned(dongle_level_),MF_BYCOMMAND);
        AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(analysis),L"Rekonstruktions&analyse");SetMenu(window_,menu);
        EnableMenuItem(menu,57643,MF_BYCOMMAND|MF_GRAYED);
        EnableMenuItem(menu,57603,MF_BYCOMMAND|MF_GRAYED);
        banner_=control(L"STATIC",L"Dongle-Menü: Zugriffsstufe für die Dokumentbearbeitung. Gespeicherte Rohwerte aus R-net-Dokumenten; unbekannte Dateiversionen bleiben erhalten. Controller nicht verbunden.",SS_LEFT,0);
        filter_=control(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,ID_FILTER);
        edit_button_=control(L"BUTTON",L"Wert bearbeiten",WS_TABSTOP|BS_PUSHBUTTON,ID_EDIT_VALUE);
        undo_button_=control(L"BUTTON",L"Rückgängig",WS_TABSTOP|BS_PUSHBUTTON,57643);
        SendMessageW(filter_,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Originaltexte durchsuchen"));
        tree_=control(WC_TREEVIEWW,L"",WS_BORDER|WS_TABSTOP|TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS,13);
        list_=control(WC_LISTVIEWW,L"",WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,10);
        ListView_SetExtendedListViewStyle(list_,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        LVCOLUMNW column{};column.mask=LVCF_TEXT|LVCF_WIDTH;column.cx=110;column.pszText=const_cast<wchar_t*>(L"Ressourcen-ID");ListView_InsertColumn(list_,0,&column);
        column.cx=800;column.pszText=const_cast<wchar_t*>(L"Originaltext (keine Parameterwerte)");ListView_InsertColumn(list_,1,&column);
        column.cx=0;column.pszText=const_cast<wchar_t*>(L"Gespeicherter Rohwert");ListView_InsertColumn(list_,2,&column);
        column.pszText=const_cast<wchar_t*>(L"Profil / Element");ListView_InsertColumn(list_,3,&column);
        details_=control(L"EDIT",L"",WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY,11);
        status_=control(STATUSCLASSNAMEW,L"",SBARS_SIZEGRIP,12);
        auto plain=rnet::RndDecoder(*original_).decode(rnet::read_file(installation_/L"Generic_V33_1_1245.rnd"));
        catalog_=rnet::ParameterCatalog::read(plain);
        build_groups();
        display_parameters();
        if(!startup_document_.empty())open_document(startup_document_);
        record_modules();record("WINDOW_CREATED");
        if(self_test_)PostMessageW(window_,SELF_TEST,0,0);
        if(can_test_)PostMessageW(window_,CAN_TEST,0,0);
        if(!can_monitor_target_.empty())PostMessageW(window_,CAN_MONITOR_START,0,0);
    }
    void build_groups(){
        parameter_view_=false;group_paths_.clear();group_handles_.clear();TreeView_DeleteAllItems(tree_);
        auto insert=[&](const std::wstring& path,const std::wstring& label,HTREEITEM parent){
            TVINSERTSTRUCTW t{};t.hParent=parent;t.hInsertAfter=TVI_LAST;t.item.mask=TVIF_TEXT|TVIF_PARAM;
            t.item.pszText=const_cast<wchar_t*>(label.c_str());t.item.lParam=LPARAM(group_paths_.size());group_paths_.push_back(path);
            auto h=TreeView_InsertItem(tree_,&t);if(!h)throw std::runtime_error("Tree node creation failed");group_handles_[path]=h;return h;
        };
        group_root_=insert(L"",L"Alle Parameter",TVI_ROOT);
        std::set<std::wstring> paths;for(const auto& p:catalog_.parameters)paths.insert(wide(p.second.text88));
        for(const auto& path:paths){if(path.empty())continue;HTREEITEM parent=group_root_;std::size_t from=0;
            while(from<path.size()){
                auto next=path.find(L'~',from);if(next==std::wstring::npos)next=path.size();
                auto full=path.substr(0,next);auto it=group_handles_.find(full);
                if(it==group_handles_.end())parent=insert(full,path.substr(from,next-from),parent);else parent=it->second;
                from=next+1;
            }
        }
        for(const auto& entry:group_handles_)TreeView_SortChildren(tree_,entry.second,FALSE);
        TreeView_Expand(tree_,group_root_,TVE_EXPAND);
    }
    void catalog_view(bool show){
        parameter_view_=show;if(!show)selected_group_.clear();ShowWindow(tree_,show?SW_SHOW:SW_HIDE);
        for(UINT id:{32844U,32831U})EnableMenuItem(GetMenu(window_),id,MF_BYCOMMAND|(show?MF_ENABLED:MF_GRAYED));
        DrawMenuBar(window_);layout();
    }
    void expand_groups(bool expand){
        if(!parameter_view_)return;
        for(const auto& entry:group_handles_)if(entry.second!=group_root_)TreeView_Expand(tree_,entry.second,expand?TVE_EXPAND:TVE_COLLAPSE);
        TreeView_Expand(tree_,group_root_,TVE_EXPAND);
    }
    void columns(const wchar_t* text,const wchar_t* id_text){
        ListView_SetColumnWidth(list_,2,value_view_?150:0);ListView_SetColumnWidth(list_,3,value_view_?140:0);
        LVCOLUMNW c{};c.mask=LVCF_TEXT;c.pszText=const_cast<wchar_t*>(id_text);ListView_SetColumn(list_,0,&c);
        c.pszText=const_cast<wchar_t*>(text);ListView_SetColumn(list_,1,&c);
    }
    void display_parameters(){
        value_view_=false;
        selected_group_.clear();catalog_view(true);rows_.clear();
        for(const auto& entry:catalog_.parameters){const auto& p=entry.second;rows_.push_back({std::to_wstring(p.key),wide(p.name()),wide(p.help()),wide(p.text88)});}
        std::stable_sort(rows_.begin(),rows_.end(),[](const auto&a,const auto&b){return lower(a.text)<lower(b.text);});
        TreeView_SelectItem(tree_,group_root_);
        columns(L"Parameter (Katalog, keine Controllerwerte)",L"Parameter-ID");
        SendMessageW(filter_,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Parameter nach Name oder ID suchen"));SetWindowTextW(filter_,L"");refresh();
        SetWindowTextW(details_,L"Parameter auswählen, um den Original-Hilfetext zu lesen.");
        set_status(L"2.154 Parameterdefinitionen aus RND gelesen — Controller nicht geöffnet");
        record("PARAMETER_CATALOG count="+std::to_string(rows_.size())+" table_offset="+std::to_string(catalog_.parameter_offset));
    }
    void open_document(const std::filesystem::path& path){
        auto bytes=rnet::read_file(path);auto next=std::make_unique<rnet::RnetDocument>(rnet::RnetDocument::read(bytes));
        std::vector<std::string> warnings;auto values=rnet::read_values(*next,catalog_,&warnings);
        if(!finish_document_change())return;
        document_=std::move(next);document_path_=path;values_=std::move(values);value_warnings_=std::move(warnings);undo_.clear();saved_document_=bytes;owned_document_=false;
        display_document_values();
        bool can_snapshot=std::any_of(document_->comments.begin(),document_->comments.end(),[](const auto& comment){return comment.rfind(u"CAN read backup;",0)==0;});
        SetWindowTextW(banner_,can_snapshot?L"Aus echtem CAN gelesene Konfigurationssicherung. Alle empfangenen Dateien sind erhalten; die verschlüsselten Dokument-Metadaten stammen aus der Original-Defaultvorlage.":L"Dongle-Menü: Zugriffsstufe für die Dokumentbearbeitung. Gespeicherte Rohwerte aus R-net-Dokumenten; unbekannte Dateiversionen bleiben erhalten. Controller nicht verbunden.");
        EnableMenuItem(GetMenu(window_),57604,MF_BYCOMMAND|MF_ENABLED);EnableMenuItem(GetMenu(window_),32939,MF_BYCOMMAND|MF_ENABLED);DrawMenuBar(window_);
        EnableMenuItem(GetMenu(window_),57643,MF_BYCOMMAND|MF_GRAYED);
        EnableMenuItem(GetMenu(window_),57603,MF_BYCOMMAND|MF_ENABLED);
        SetWindowTextW(window_,(path.filename().wstring()+L" — R-net C++ Rekonstruktion").c_str());
        record("DOCUMENT_OPEN bytes="+std::to_string(bytes.size())+" files="+std::to_string(document_->files.size())+" values="+std::to_string(values_.size()));
        for(const auto& warning:value_warnings_)record("DOCUMENT_VALUE_WARNING "+warning);
    }
    void display_document_values(){
        if(!document_)return;
        value_view_=true;selected_group_.clear();catalog_view(true);rows_.clear();
        for(const auto& v:values_){
            const auto& p=catalog_.parameters.at(v.parameter);const auto& file=document_->files[v.file];
            std::wostringstream location,detail;
            location<<(v.group?L"Profil "+std::to_wstring(v.profile+1):L"Allgemein");
            location<<L" / "<<v.element+1;
            detail<<wide(p.help())<<L"\r\n\r\nGespeicherter Rohwert; keine Einheitenumrechnung.\r\nZugriffsstufe: "<<wide8(rnet::access_level_name(static_cast<rnet::AccessLevel>(p.metadata[1]>>5)))<<L"\r\nDatei: 0x"<<std::hex<<file.id<<std::dec<<L", Version "<<file.version<<L"\r\nAbschnitt "<<v.section+1<<L", Element "<<v.element+1<<L", Byteposition "<<v.offset<<L", Breite "<<v.width;
            rows_.push_back({std::to_wstring(v.parameter),wide(p.name()),detail.str(),wide(p.text88),v.numeric?std::to_wstring(v.number):wide(v.text),location.str()});
        }
        TreeView_SelectItem(tree_,group_root_);columns(L"Parameter",L"Parameter-ID");
        SendMessageW(filter_,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Gespeicherte Parameter nach Name oder ID suchen"));SetWindowTextW(filter_,L"");refresh();layout();
        std::wstring details=document_path_.wstring();
        if(!value_warnings_.empty())details+=L"\r\n\r\n"+std::to_wstring(value_warnings_.size())+L" Datenblock-Version(en) werden unverändert erhalten und noch nicht als Werte interpretiert.";
        SetWindowTextW(details_,details.c_str());
        set_status(std::to_wstring(values_.size())+L" gespeicherte Werte; "+std::to_wstring(value_warnings_.size())+L" unbekannte Dateiversion(en) — Controller nicht geöffnet");
    }
    void display_repository(){
        if(!document_)return;
        value_view_=false;catalog_view(false);rows_.clear();
        for(const auto& f:document_->files){
            std::wostringstream id,description;id<<L"0x"<<std::hex<<f.id;
            description<<L"Version "<<std::dec<<f.version<<L" — "<<f.data.size()<<L" Bytes";
            rows_.push_back({id.str(),description.str(),L"Originaler Repository-Datensatz. Unterstützte Parameterwerte stehen in der Werteansicht zur Verfügung; unbekannte Versionen bleiben unverändert erhalten.",L""});
        }
        columns(L"Repository-Datei (keine interpretierten Parameterwerte)",L"Datei-ID");
        SendMessageW(filter_,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Repository-Dateien durchsuchen"));
        SetWindowTextW(filter_,L"");refresh();SetWindowTextW(details_,document_path_.wstring().c_str());
        set_status(L"R-net-Dokument geöffnet — Speichern unter anderem Namen möglich");
    }
    void apply_value_edit(std::size_t index,const std::u16string& input){
        if(!document_||index>=values_.size())throw std::runtime_error("Select a stored parameter first");
        auto before=document_->write();const auto parameter=values_[index].parameter;
        if(!rnet::edit_value(*document_,catalog_,values_[index],input,dongle_level_))return;
        undo_.push_back(std::move(before));if(undo_.size()>100)undo_.erase(undo_.begin());
        value_warnings_.clear();values_=rnet::read_values(*document_,catalog_,&value_warnings_);
        EnableMenuItem(GetMenu(window_),57643,MF_BYCOMMAND|MF_ENABLED);DrawMenuBar(window_);
        display_document_values();set_status(L"Wert im Dokument geändert — Speichern unter neuem Namen möglich");record("DOCUMENT_VALUE_EDIT parameter="+std::to_string(parameter));
    }
    void edit_selected_value(){
        auto selected=ListView_GetNextItem(list_,-1,LVNI_SELECTED);
        if(!document_||!value_view_||selected<0||std::size_t(selected)>=displayed_.size())return;
        const auto index=displayed_[selected];const auto& value=values_.at(index);const auto& p=catalog_.parameters.at(value.parameter);
        if(!rnet::access_allows(p.metadata[1]>>5,dongle_level_)){
            set_status(L"Bearbeitung gesperrt: Parameter benötigt eine höhere Zugriffsstufe im Dongle-Menü.");return;
        }
        ValueDialog dialog;dialog.label=wide(p.name())+L"\r\nGespeicherter Rohwert; ";
        if(value.numeric)dialog.label+=L"Bereich "+std::to_wstring(p.signed_storage?std::int64_t(std::int32_t(p.metadata[4])):p.metadata[4])+L" bis "+std::to_wstring(p.signed_storage?std::int64_t(std::int32_t(p.metadata[5])):p.metadata[5]);
        else dialog.label+=L"maximal "+std::to_wstring(value.width)+L" Zeichen";
        dialog.text=rows_[index].stored;dialog.apply=[this,index](const std::u16string& text){apply_value_edit(index,text);};
        dialog.automated=self_test_;
        if(DialogBoxParamW(instance_,MAKEINTRESOURCEW(101),window_,ValueDialog::procedure,reinterpret_cast<LPARAM>(&dialog))==-1)throw std::runtime_error("Cannot create value editor dialog");
    }
    void undo_value_edit(){
        if(!document_||undo_.empty())return;
        auto previous=rnet::RnetDocument::read(undo_.back());document_=std::make_unique<rnet::RnetDocument>(std::move(previous));undo_.pop_back();
        value_warnings_.clear();values_=rnet::read_values(*document_,catalog_,&value_warnings_);display_document_values();
        EnableMenuItem(GetMenu(window_),57643,MF_BYCOMMAND|(undo_.empty()?MF_GRAYED:MF_ENABLED));DrawMenuBar(window_);record("DOCUMENT_UNDO");
    }
    void save_document_as(const std::filesystem::path& path){
        if(!document_)throw std::runtime_error("No document is open");
        if(lower(path.extension().wstring())!=L".r-net")throw std::runtime_error("R-net documents require .R-net extension");
        if(std::filesystem::weakly_canonical(path.parent_path())==std::filesystem::weakly_canonical(installation_))throw std::runtime_error("Writing into the original installation root is disabled");
        if(std::filesystem::exists(path))throw std::runtime_error("Choose a new destination name; existing documents are preserved");
        const auto bytes=document_->write();write_new_file(path,bytes);saved_document_=bytes;document_path_=path;owned_document_=true;
        SetWindowTextW(window_,(path.filename().wstring()+L" — R-net C++ Rekonstruktion").c_str());
        set_status(L"R-net-Dokument unter anderem Namen gespeichert — Quelldatei erhalten");record("DOCUMENT_SAVED_AS bytes="+std::to_string(bytes.size()));
    }
    void save_document(){
        if(!document_)return;
        if(!owned_document_){auto path=select_file(true,L"R-net-Dateien\0*.R-net\0\0",L"R-net");if(!path.empty())save_document_as(path);return;}
        if(rnet::read_file(document_path_)!=saved_document_)throw std::runtime_error("Destination changed externally; use Save As with a new name");
        const auto bytes=document_->write();auto temporary=document_path_;temporary+=L".saving-"+std::to_wstring(GetTickCount64());write_new_file(temporary,bytes);
        if(!MoveFileExW(temporary.c_str(),document_path_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary.c_str());throw std::runtime_error("Cannot replace saved working copy");}
        saved_document_=bytes;set_status(L"Arbeitskopie gespeichert");record("DOCUMENT_SAVED bytes="+std::to_string(bytes.size()));
    }
    void close_document(){
        if(!finish_document_change())return;
        document_.reset();document_path_.clear();values_.clear();value_warnings_.clear();undo_.clear();saved_document_.clear();owned_document_=false;EnableMenuItem(GetMenu(window_),57603,MF_BYCOMMAND|MF_GRAYED);EnableMenuItem(GetMenu(window_),57643,MF_BYCOMMAND|MF_GRAYED);EnableMenuItem(GetMenu(window_),57604,MF_BYCOMMAND|MF_GRAYED);EnableMenuItem(GetMenu(window_),32939,MF_BYCOMMAND|MF_GRAYED);DrawMenuBar(window_);display_parameters();record("DOCUMENT_CLOSED");
        SetWindowTextW(window_,L"R-net C++ Rekonstruktion — unvollständiger Entwicklungsstand");
    }
    bool finish_document_change(){
        if(!document_||document_->write()==saved_document_)return true;
        auto answer=MessageBoxW(window_,owned_document_?L"Geänderte Parameter in der Arbeitskopie speichern?":L"Geänderte Parameter unter einem neuen Dateinamen speichern?",L"Nicht gespeicherte Änderungen",MB_YESNOCANCEL|MB_ICONQUESTION);
        if(answer==IDCANCEL)return false;
        if(answer==IDYES){if(owned_document_)save_document();else{auto path=select_file(true,L"R-net-Dateien\0*.R-net\0\0",L"R-net");if(path.empty())return false;save_document_as(path);}}
        return true;
    }
    void display_resources(){
        value_view_=false;
        catalog_view(false);rows_.clear();for(const auto&s:original_->strings())rows_.push_back({std::to_wstring(s.id),wide(s.text),wide(s.text),L""});
        std::stable_sort(rows_.begin(),rows_.end(),[](auto&a,auto&b){return std::stoul(a.id)<std::stoul(b.id);});
        columns(L"Originaltext (keine Parameterwerte)",L"Ressourcen-ID");
        SendMessageW(filter_,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Originaltexte durchsuchen"));SetWindowTextW(filter_,L"");refresh();
        set_status(L"Originalressourcen gelesen — Controller nicht geöffnet");record("RESOURCE_VIEW count="+std::to_string(rows_.size()));
    }
    void refresh(){
        int n=GetWindowTextLengthW(filter_);std::wstring query(n+1,0);GetWindowTextW(filter_,query.data(),n+1);query.resize(n);query=lower(query);
        SendMessageW(list_,WM_SETREDRAW,FALSE,0);ListView_DeleteAllItems(list_);displayed_.clear();
        for(std::size_t i=0;i<rows_.size();++i){const auto&r=rows_[i];if(parameter_view_&&!selected_group_.empty()&&r.group!=selected_group_&&r.group.rfind(selected_group_+L"~",0)!=0)continue;
            if(!query.empty()&&lower(r.text+L" "+r.id).find(query)==std::wstring::npos)continue;
            auto row=int(displayed_.size());displayed_.push_back(i);LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=row;item.pszText=const_cast<wchar_t*>(r.id.c_str());ListView_InsertItem(list_,&item);ListView_SetItemText(list_,row,1,const_cast<wchar_t*>(r.text.c_str()));
            ListView_SetItemText(list_,row,2,const_cast<wchar_t*>(r.stored.c_str()));ListView_SetItemText(list_,row,3,const_cast<wchar_t*>(r.location.c_str()));
        }
        SendMessageW(list_,WM_SETREDRAW,TRUE,0);InvalidateRect(list_,nullptr,TRUE);
        update_edit_controls();EnableWindow(undo_button_,document_&&!undo_.empty());
    }
    void layout(){
        RECT r{};GetClientRect(window_,&r);int width=r.right,height=r.bottom;
        SendMessageW(status_,WM_SIZE,0,0);RECT sr{};GetWindowRect(status_,&sr);int sh=sr.bottom-sr.top;
        MoveWindow(banner_,12,10,std::max(0,width-24),40,TRUE);MoveWindow(filter_,12,55,std::max(0,width-264),26,TRUE);
        MoveWindow(edit_button_,std::max(12,width-244),55,128,26,TRUE);MoveWindow(undo_button_,std::max(148,width-108),55,96,26,TRUE);
        int table=std::max(20,height-sh-235);int split=parameter_view_?std::min(260,std::max(100,width/3)):0;
        if(parameter_view_)MoveWindow(tree_,12,90,split,table,TRUE);
        MoveWindow(list_,12+(split?split+8:0),90,std::max(0,width-24-(split?split+8:0)),table,TRUE);
        MoveWindow(details_,12,100+table,std::max(0,width-24),std::max(0,height-sh-table-112),TRUE);
        const int list_width=std::max(0,width-24-(split?split+8:0));
        ListView_SetColumnWidth(list_,1,std::max(200,list_width-136-(value_view_?290:0)));
        RECT client{};GetClientRect(details_,&client);
        if(client.right>16&&client.bottom>16){RECT text{8,6,client.right-8,client.bottom-6};SendMessageW(details_,EM_SETRECT,0,reinterpret_cast<LPARAM>(&text));}
    }
    void record_modules(){
        HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,0);
        if(snapshot==INVALID_HANDLE_VALUE)throw std::runtime_error("Module snapshot failed");
        MODULEENTRY32W module{};module.dwSize=sizeof(module);
        if(Module32FirstW(snapshot,&module))do{record("MODULE "+narrow(module.szModule)+" "+narrow(module.szExePath));}while(Module32NextW(snapshot,&module));
        CloseHandle(snapshot);
    }
    std::filesystem::path select_file(bool save,const wchar_t* filter,const wchar_t* extension){
        wchar_t path[32768]{};OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=window_;ofn.lpstrFilter=filter;ofn.lpstrFile=path;ofn.nMaxFile=32768;ofn.lpstrDefExt=extension;
        ofn.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|(save?(OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST):(OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST));
        if(save?GetSaveFileNameW(&ofn):GetOpenFileNameW(&ofn))return path;
        return {};
    }
    void decode_file(const std::filesystem::path& path){
        rnet::RndDecoder decoder(*original_);auto plain=decoder.decode(rnet::read_file(path));
        // Show only facts with a known schema interpretation; do not invent parameter values.
        std::wostringstream report;report<<L"RND: "<<path.wstring()<<L"\r\nSchema: "<<unsigned(plain[0])<<L"\r\nEntschlüsselte Bytes: "<<plain.size()<<L"\r\n\r\nGespeicherte Parameterwerte werden nach dem Öffnen eines unterstützten R-net-Dokuments angezeigt.\r\n\r\nKlartext-Anfang:\r\n";
        for(std::size_t i=0;i<std::min<std::size_t>(plain.size(),1024);++i){auto c=plain[i];report<<wchar_t(c>=32&&c<127?c:'.');if(i%96==95)report<<L"\r\n";}
        SetWindowTextW(details_,report.str().c_str());set_status(L"RND entschlüsselt und Versionskennung geprüft");record("RND_DECODED bytes="+std::to_string(plain.size())+" version="+std::to_string(plain[0]));
    }
    void scan(){
        if(scanning_)return;
        if(scan_thread_.joinable())scan_thread_.join();
        scanning_=true;set_status(L"FTDI-Geräte werden aufgelistet …");record("FT_ListDevices enumeration requested; no open/read/write calls");
        try {
            scan_thread_=std::thread([this]{
                try {scan_result_=rnet::DeviceDiscovery{}.scan(installation_/L"ftd2xx.dll");}
                catch(const std::exception&){scan_result_={};scan_result_.status=5;}
                PostMessageW(window_,SCAN_DONE,0,0);
            });
        } catch(...) {scanning_=false;throw;}
    }
    void scan_done(){
        if(scan_thread_.joinable())scan_thread_.join();
        scanning_=false;
        std::wostringstream s;s<<L"FTD2XX-Version: 0x"<<std::hex<<scan_result_.library_version<<L"\r\nFT_STATUS: "<<std::dec<<scan_result_.status<<L"\r\nGefundene Geräte: "<<scan_result_.devices.size()<<L"\r\n";
        for(auto&d:scan_result_.devices)s<<wide8(d.serial)<<L" — "<<wide8(d.description)<<L"\r\n";
        s<<L"\r\nKein Gerät wurde geöffnet. Verbindungs- und Controllerprotokoll sind noch nicht implementiert.";
        SetWindowTextW(details_,s.str().c_str());set_status(L"Geräteauflistung beendet; Controller nicht geöffnet");record("SCAN_DONE status="+std::to_string(scan_result_.status)+" devices="+std::to_string(scan_result_.devices.size()));
        record("FT_GetLibraryVersion(return="+std::to_string(scan_result_.library_status)+", version="+std::to_string(scan_result_.library_version)+")");
        if(scan_result_.enumeration_attempted)record("FT_ListDevices(flags=0x80000000, return="+std::to_string(scan_result_.status)+", devices="+std::to_string(scan_result_.devices.size())+")");
        if(self_test_scan_pending_){
            if(scan_result_.library_status!=0||scan_result_.library_version==0||!scan_result_.enumeration_attempted||scan_result_.status>19)throw std::runtime_error("Original FTDI discovery API test failed");
            record("TEST_PASS genuine FTDI API calls completed; scan worker joined; no device opened");
            record("GUI_SELFTEST_PASSED (controller communication remains untested)");
            SendMessageW(window_,WM_COMMAND,57665,0);
        }
    }
    void export_csv(const std::filesystem::path& path){
        // Refuse original installation/data targets even if the file dialog allows them.
        auto ext=lower(path.extension().wstring());if(ext!=L".csv")throw std::runtime_error("Analysis export requires .csv extension");
        auto parent=std::filesystem::weakly_canonical(path.parent_path());
        if(parent==std::filesystem::weakly_canonical(installation_))throw std::runtime_error("Export into original installation root is disabled");
        std::ofstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Cannot write CSV");
        f<<"\xef\xbb\xbf" "id,text,details"<<(value_view_?",stored_raw_value,profile_element":"")<<"\r\n";
        for(auto i:displayed_){const auto& row=rows_[i];f<<narrow(row.id);std::vector<std::wstring> fields{row.text,row.details};if(value_view_){fields.push_back(row.stored);fields.push_back(row.location);}for(const auto& field:fields){f<<",\"";for(auto c:narrow(field)){if(c=='\"')f<<'\"';f<<c;}f<<'\"';}f<<"\r\n";}
        if(!f)throw std::runtime_error("CSV write failed");
        record("CSV_EXPORTED rows="+std::to_string(displayed_.size()));
    }
    void read_emulator(){
        if(can_busy_)return;
        if(!document_)throw std::runtime_error("Open an R-net document first to preserve its metadata as a template");
        if(!finish_document_change())return;
        if(can_plan_.empty())can_plan_=select_file(false,L"CAN-Leseplan\0*.txt\0\0",L"txt");
        if(can_plan_.empty())return;
        auto selectors=rnet::read_can_plan(can_plan_);
        if(can_thread_.joinable())can_thread_.join();
        can_cancelled_=false;can_error_.clear();can_files_.clear();
        can_busy_=true;EnableWindow(list_,FALSE);EnableWindow(filter_,FALSE);EnableWindow(tree_,FALSE);EnableWindow(edit_button_,FALSE);EnableWindow(undo_button_,FALSE);
        set_status(L"Privater CAN-Emulator: warte auf zyklische Nachrichten …");
        record("CAN_READ_BEGIN selectors="+std::to_string(selectors.size())+"; existing document metadata retained; payloads from live CAN only");
        try{can_thread_=std::thread([this,selectors=std::move(selectors)]{
            try{
                auto transport=rnet::open_can_emulator_transport(output_/L"can-transport.log");
                rnet::CanRepositoryReader reader(*transport,can_cancelled_);reader.detect();
                PostMessageW(window_,CAN_PROGRESS,0,0);
                for(auto selector:selectors){can_files_.push_back(reader.read(selector));PostMessageW(window_,CAN_PROGRESS,1,LPARAM(can_files_.size()));}
            }catch(const std::exception& e){can_error_=e.what();}
            PostMessageW(window_,CAN_READ_DONE,0,0);
        });}catch(...){can_busy_=false;EnableWindow(list_,TRUE);EnableWindow(filter_,TRUE);EnableWindow(tree_,TRUE);update_edit_controls();throw;}
    }
    void emulator_read_done(){
        if(can_thread_.joinable())can_thread_.join();
        can_busy_=false;
        EnableWindow(list_,TRUE);EnableWindow(filter_,TRUE);EnableWindow(tree_,TRUE);update_edit_controls();EnableWindow(undo_button_,document_&&!undo_.empty());
        if(!can_error_.empty())throw std::runtime_error(can_error_);
        if(!document_||can_files_.empty())throw std::runtime_error("No complete emulator repository received");
        auto next=*document_;next.files=can_files_;
        std::vector<std::string> warnings;auto values=rnet::read_values(next,catalog_,&warnings);
        if(rnet::RnetDocument::read(next.write()).write()!=next.write())throw std::runtime_error("Received document cannot roundtrip");
        document_=std::make_unique<rnet::RnetDocument>(std::move(next));values_=std::move(values);value_warnings_=std::move(warnings);
        document_path_=L"Emulator-Read.R-net";owned_document_=false;saved_document_.clear();undo_.clear();
        display_document_values();SetWindowTextW(window_,L"Emulator-Read.R-net — R-net C++ Rekonstruktion");
        SetWindowTextW(banner_,L"Repository vom privaten CAN-Emulator gelesen und geprüft. Dokument-Metadaten stammen aus der geöffneten Vorlage.");
        set_status(L"Emulator gelesen: "+std::to_wstring(document_->files.size())+L" Repository-Dateien, "+std::to_wstring(values_.size())+L" zugeordnete Werte");
        record("CAN_READ_COMPLETE files="+std::to_string(document_->files.size())+" values="+std::to_string(values_.size())+" warnings="+std::to_string(value_warnings_.size()));
        if(can_test_){
            if(GetModuleHandleW(L"FTD2XX.dll"))throw std::runtime_error("Direct CAN test unexpectedly loaded FTDI");
            save_document_as(can_target_);record("CAN_DIRECT_TEST_PASSED; no FTDI, CUSBMsg or original Programmer process");
            SendMessageW(window_,WM_COMMAND,57665,0);
        }
    }
    void monitor_real_can(){
        if(can_busy_)return;
        if(can_thread_.joinable())can_thread_.join();
        can_cancelled_=false;can_error_.clear();can_observed_.clear();can_frame_count_=0;can_busy_=true;
        EnableWindow(list_,FALSE);EnableWindow(filter_,FALSE);EnableWindow(tree_,FALSE);EnableWindow(edit_button_,FALSE);EnableWindow(undo_button_,FALSE);
        set_status(L"can0: Empfang gestartet — keine CAN-Nutzdaten senden");
        record("PHYSICAL_CAN_MONITOR_BEGIN receive-only bridge port 39010; no POP requests");
        try{can_thread_=std::thread([this]{
            try{
                auto transport=rnet::open_can_monitor_transport(output_/L"physical-can-rx.log");
                auto start=std::chrono::steady_clock::now(),update=start;
                while(!can_cancelled_&&std::chrono::steady_clock::now()-start<std::chrono::seconds(10)){
                    rnet::CanFrame frame;
                    if(transport->receive(frame,100)){
                        auto& row=can_observed_[frame.id];++row.count;row.latest=std::move(frame.data);++can_frame_count_;
                        if(can_observed_.size()>4096)throw std::runtime_error("Too many distinct CAN IDs");
                    }
                    auto now=std::chrono::steady_clock::now();
                    if(now-update>=std::chrono::milliseconds(250)){PostMessageW(window_,CAN_MONITOR_PROGRESS,can_frame_count_,0);update=now;}
                }
                if(!can_frame_count_&&!can_cancelled_)throw std::runtime_error("No real CAN data received; receive-only bridge must be running");
            }catch(const std::exception& e){can_error_=e.what();}
            PostMessageW(window_,CAN_MONITOR_DONE,0,0);
        });}catch(...){can_busy_=false;EnableWindow(list_,TRUE);EnableWindow(filter_,TRUE);EnableWindow(tree_,TRUE);update_edit_controls();throw;}
    }
    void can_monitor_done(){
        if(can_thread_.joinable())can_thread_.join();
        can_busy_=false;EnableWindow(list_,TRUE);EnableWindow(filter_,TRUE);EnableWindow(tree_,TRUE);
        if(!can_error_.empty()){update_edit_controls();throw std::runtime_error(can_error_);}
        value_view_=false;catalog_view(false);rows_.clear();
        for(const auto& entry:can_observed_){
            std::wostringstream id,data;id<<L"0x"<<std::hex<<std::uppercase<<entry.first;
            for(auto byte:entry.second.latest)data<<std::hex<<std::uppercase<<std::setw(2)<<std::setfill(L'0')<<unsigned(byte)<<L" ";
            auto text=std::to_wstring(entry.second.count)+L" Frames | DLC "+std::to_wstring(entry.second.latest.size())+L" | "+data.str();
            rows_.push_back({id.str(),text,L"Echter CAN-Empfang auf can0. Letzte Nutzdaten pro ID; keine Parameterinterpretation. Keine CAN-Nutzdaten gesendet.",L""});
        }
        columns(L"Anzahl | DLC | letzte empfangene Bytes",L"CAN-ID");SetWindowTextW(filter_,L"");refresh();
        SetWindowTextW(banner_,L"Echter CAN-Bus: Empfang über can0, 125 kbit/s. 10-Sekunden-Mitschnitt; keine POP-, Power-, Bewegungs- oder Schreibanfragen gesendet.");
        SetWindowTextW(details_,L"Empfangene CAN-Datenframes. Die Anzeige enthält Rohdaten; Controller-Parameter wurden nicht angefragt. Der Adaptertreiber arbeitet im normalen CAN-Modus mit Bus-Bestätigungen.");
        set_status(L"can0: "+std::to_wstring(can_frame_count_)+L" Frames, "+std::to_wstring(can_observed_.size())+L" CAN-IDs empfangen — keine CAN-Nutzdaten gesendet");
        SetWindowTextW(window_,L"can0 — Echter CAN-Empfang — R-net C++ Rekonstruktion");
        record("PHYSICAL_CAN_MONITOR_PASSED frames="+std::to_string(can_frame_count_)+" ids="+std::to_string(can_observed_.size())+"; application CAN TX=0");
        if(!can_monitor_target_.empty()){export_csv(can_monitor_target_);record("PHYSICAL_CAN_MONITOR_CSV_SAVED");can_monitor_target_.clear();}
    }
    void command(UINT id){
        ++commands_;record("COMMAND "+std::to_string(id));
        if(can_busy_&&id!=57665){set_status(L"Emulator wird gelesen …");return;}
        if(id>=ID_DONGLE_FIRST&&id<=ID_DONGLE_LAST){select_dongle_level(static_cast<rnet::AccessLevel>(id-ID_DONGLE_FIRST));return;}
        switch(id){
        case 57665:SendMessageW(window_,WM_CLOSE,0,0);return;
        case 57664:MessageBoxW(window_,L"Eigenständige C++17-Entwicklungsfassung, PE32 i386.\n\nOriginal-Parameterkatalog, gespeicherte Rohwerte, begrenzte Zahlen-/Textbearbeitung, Rückgängig, Speichern von Arbeitskopien, Suche und CSV-Export.\n\nUnvollständig: weitere Parametertypen, Dokumentvarianten und Controllerprotokoll.\nKeine Ersatz-DLLs.",L"R-net Rekonstruktion — unvollständig",MB_OK|MB_ICONINFORMATION);break;
        case 32844:expand_groups(true);break;
        case 32831:expand_groups(false);break;
        case ID_SHOW_DETAILS:ShowWindow(details_,SW_SHOW);break;
        case ID_HIDE_DETAILS:ShowWindow(details_,SW_HIDE);break;
        case 57601:{auto p=select_file(false,L"R-net-Dateien\0*.R-net\0\0",L"R-net");if(!p.empty())open_document(p);break;}
        case 57604:{auto p=select_file(true,L"R-net-Dateien\0*.R-net\0\0",L"R-net");if(!p.empty())save_document_as(p);break;}
        case 57603:save_document();break;
        case 32939:close_document();break;
        case 57636:SetFocus(filter_);break;
        case ID_PARAMETERS:display_parameters();break;
        case ID_VALUES:display_document_values();break;
        case ID_REPOSITORY:display_repository();break;
        case ID_EDIT_VALUE:edit_selected_value();break;
        case 57643:undo_value_edit();break;
        case ID_RESOURCES:display_resources();break;
        case ID_DATABASE:{auto p=select_file(false,L"RND-Dateien\0*.rnd\0\0",L"rnd");if(!p.empty())decode_file(p);break;}
        case ID_SCAN:scan();break;
        case ID_CAN_READ:read_emulator();break;
        case ID_CAN_MONITOR:monitor_real_can();break;
        case ID_REPORT:{auto p=select_file(true,L"CSV-Analyse\0*.csv\0\0",L"csv");if(!p.empty())export_csv(p);break;}
        default:record("UNIMPLEMENTED_COMMAND rejected");break;
        }
    }
    void selftest(){
        // Exercises the real window/message/menu path without opening any hardware.
        auto check=[&](bool ok,const char*s){record(std::string(ok?"TEST_PASS ":"TEST_FAIL ")+s);if(!ok)throw std::runtime_error(s);};
        check(IsWindowVisible(window_),"main window visible");
        check(IsProcessDPIAware(),"DPI awareness prevents virtualized mouse coordinates on secondary monitor");
        RECT menu_item{};
        check(GetMenuItemRect(window_,GetMenu(window_),0,&menu_item)!=FALSE,"menu screen bounds available");
        POINT menu_point{(menu_item.left+menu_item.right)/2,(menu_item.top+menu_item.bottom)/2};
        check(SendMessageW(window_,WM_NCHITTEST,0,MAKELPARAM(menu_point.x,menu_point.y))==HTMENU,"visible menu position is recognized as menu hit");
        SetCursor(nullptr);ShowCursor(FALSE);
        SendMessageW(window_,WM_SETCURSOR,reinterpret_cast<WPARAM>(list_),MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));
        CURSORINFO cursor{};cursor.cbSize=sizeof(cursor);
        check(GetCursorInfo(&cursor)&&(cursor.flags&CURSOR_SHOWING)&&GetCursor()==LoadCursorW(nullptr,IDC_ARROW),"hidden client cursor restored over parameter list");
        SendMessageW(window_,WM_SETCURSOR,reinterpret_cast<WPARAM>(filter_),MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));
        check(GetCursor()==LoadCursorW(nullptr,IDC_IBEAM),"text input retains visible I-beam cursor");
        check(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST)==preferred_monitor().monitor,"application opens on preferred non-primary monitor (single-monitor fallback supported)");
        check(ListView_GetItemCount(list_)==2154,"2154 original parameter definitions shown");
        check(catalog_.parameters.at(1999).name()==u"Switched Input Smoothing","known original parameter name loaded");
        check((GetMenuState(GetMenu(window_),32804,MF_BYCOMMAND)&MF_GRAYED)!=0,"unimplemented controller write disabled");
        check((GetMenuState(GetMenu(window_),57665,MF_BYCOMMAND)&MF_GRAYED)==0,"exit command enabled");
        check(dongle_level_==rnet::AccessLevel::OEM,"Dongle access defaults to OEM");
        for(UINT selected=ID_DONGLE_FIRST;selected<=ID_DONGLE_LAST;++selected){
            SendMessageW(window_,WM_COMMAND,selected,0);
            check(unsigned(dongle_level_)==selected-ID_DONGLE_FIRST,"Dongle command selects requested access level");
            for(UINT item=ID_DONGLE_FIRST;item<=ID_DONGLE_LAST;++item)
                check(bool(GetMenuState(GetMenu(window_),item,MF_BYCOMMAND)&MF_CHECKED)==(item==selected),"exactly one Dongle radio item checked");
            wchar_t status[1024]{};SendMessageW(status_,SB_GETTEXTW,0,reinterpret_cast<LPARAM>(status));
            check(std::wstring(status).find(wide8(rnet::access_level_name(dongle_level_)))!=std::wstring::npos,"status shows selected Dongle level");
        }
        SendMessageW(window_,WM_COMMAND,ID_DONGLE_FIRST+unsigned(rnet::AccessLevel::OEM),0);
        BYTE keys[256]{},saved[256]{};GetKeyboardState(saved);std::copy(std::begin(saved),std::end(saved),std::begin(keys));keys[VK_CONTROL]|=0x80;SetKeyboardState(keys);
        MSG shortcut{};shortcut.hwnd=window_;shortcut.message=WM_KEYDOWN;shortcut.wParam='F';
        int translated=TranslateAcceleratorW(window_,accelerators_,&shortcut);SetKeyboardState(saved);
        check(translated!=0&&GetFocus()==filter_,"original Ctrl+F accelerator dispatches Find");
        check(TreeView_GetCount(tree_)>50,"original RND grouping paths reconstructed as tree");
        const auto controls=group_handles_.at(L"Controls~Profiled");TreeView_EnsureVisible(tree_,controls);TreeView_SelectItem(tree_,controls);
        check(ListView_GetItemCount(list_)==14,"group selection filters 14 Controls/Profiled definitions");
        TreeView_SelectItem(tree_,group_root_);check(ListView_GetItemCount(list_)==2154,"all-parameters group restores full catalog");
        SendMessageW(window_,WM_COMMAND,32844,0);check((TreeView_GetItemState(tree_,group_handles_.at(L"Controls"),TVIS_EXPANDED)&TVIS_EXPANDED)!=0,"original Expand Tree command expands groups");
        SendMessageW(window_,WM_COMMAND,32831,0);check((TreeView_GetItemState(tree_,group_handles_.at(L"Controls"),TVIS_EXPANDED)&TVIS_EXPANDED)==0,"original Collapse Tree command collapses groups");
        SetWindowTextW(filter_,L"Speed");check(ListView_GetItemCount(list_)>0&&ListView_GetItemCount(list_)<2154,"search filters actual parameter names");
        SendMessageW(window_,WM_COMMAND,ID_HIDE_DETAILS,0);check(!IsWindowVisible(details_),"analysis detail hide event handled");
        SendMessageW(window_,WM_COMMAND,ID_SHOW_DETAILS,0);check(IsWindowVisible(details_),"analysis detail show event handled");
        SendMessageW(window_,WM_COMMAND,ID_RESOURCES,0);check(ListView_GetItemCount(list_)==1454,"resource refresh event handled");
        auto out=output_/L"resources.csv";export_csv(out);check(std::filesystem::file_size(out)>1000,"CSV export created data");
        decode_file(installation_/L"Generic_V33_1_1245.rnd");
        check(GetModuleHandleW(L"FTD2XX.dll")==nullptr,"direct CAN app starts without loading FTDI");
        auto reference=installation_/L"reverse-engineering"/L"tests"/L"fixtures"/L"original-reference-default.R-net";
        open_document(reference);check(ListView_GetItemCount(list_)==2026,"2026 original document values shown");
        SetWindowTextW(filter_,L"Maximum Forward Speed");check(ListView_GetItemCount(list_)==8,"all eight saved speed profiles shown");
        check(rows_[displayed_[0]].stored==L"30","default first-profile speed storage value shown");
        SendMessageW(window_,WM_COMMAND,ID_REPOSITORY,0);check(ListView_GetItemCount(list_)==16,"original R-net repository document opened");
        check((GetMenuState(GetMenu(window_),57604,MF_BYCOMMAND)&MF_GRAYED)==0,"Save As enabled for loaded document");
        auto copy=output_/L"gui-roundtrip.R-net";save_document_as(copy);
        check(rnet::read_file(copy)==rnet::read_file(reference),"R-net save under new name preserves every original byte");
        for(const auto& filename:{L"CS16110708.R-net",L"CS16110708-indoor.R-net",L"CS16110708geladenk.R-net",L"CS16110708ges.R-net",L"CS16110708geänder.R-net"}){
            auto userfile=reference.parent_path()/L"user-documents"/filename;open_document(userfile);
            check(ListView_GetItemCount(list_)==2026&&value_warnings_.size()==1,"user document values shown; unsupported Servo v7 reported");
            auto saved=output_/filename;save_document_as(saved);check(rnet::read_file(saved)==rnet::read_file(userfile),"user document preserved byte for byte");
        }
        SetWindowTextW(filter_,L"Maximum Forward Speed");check(ListView_GetItemCount(list_)==8&&rows_[displayed_[0]].stored==L"86","changed user first-profile speed shown as 86");
        ListView_SetItemState(list_,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
        auto required=catalog_.parameters.at(values_[displayed_[0]].parameter).metadata[1]>>5;
        auto access_before=document_->write();
        for(UINT level=0;level<=3;++level){
            SendMessageW(window_,WM_COMMAND,ID_DONGLE_FIRST+level,0);
            check(bool(IsWindowEnabled(edit_button_))==(required<=level),"selected Dongle level gates edit button");
            check(((GetMenuState(GetMenu(window_),ID_EDIT_VALUE,MF_BYCOMMAND)&MF_GRAYED)==0)==(required<=level),"selected Dongle level gates edit menu");
            if(required>level){SendMessageW(window_,WM_COMMAND,ID_EDIT_VALUE,0);check(document_->write()==access_before&&undo_.empty(),"blocked menu edit leaves document and Undo unchanged");}
        }
        check(document_->write()==access_before,"switching Dongle levels does not change document bytes");
        SendMessageW(window_,WM_COMMAND,ID_DONGLE_FIRST+unsigned(rnet::AccessLevel::OEM),0);
        auto before_edit=document_->write();SendMessageW(window_,WM_COMMAND,ID_EDIT_VALUE,0);
        check(!undo_.empty(),"value editor dialog accepted numeric edit");
        SetWindowTextW(filter_,L"Maximum Forward Speed");check(rows_[displayed_[0]].stored==L"87","edited value appears in document view");
        SendMessageW(window_,WM_COMMAND,57603,0);check(rnet::read_file(document_path_)==document_->write(),"Save atomically updates only the created working copy");
        save_document_as(output_/L"gui-edited-values.R-net");
        SendMessageW(window_,WM_COMMAND,57643,0);check(document_->write()==before_edit&&undo_.empty(),"Undo restores every document byte");
        save_document_as(output_/L"gui-undone-values.R-net");SetWindowTextW(filter_,L"Maximum Forward Speed");
        auto values_csv=output_/L"stored-values.csv";export_csv(values_csv);
        check(std::filesystem::file_size(values_csv)>100,"stored values CSV exported");
        auto corpus_file=reference.parent_path()/L"open-rnet"/L"documents"/L"configs"/L"raw"/L"2021 F3"/L"CS21054090-OEM_mod_first.R-net";
        open_document(corpus_file);check(document_->files.size()==42&&ListView_GetItemCount(list_)==4370&&value_warnings_.size()==1,"open-rnet 42-file document displays canonical-ID values");
        auto corpus_copy=output_/L"open-rnet-roundtrip.R-net";save_document_as(corpus_copy);check(rnet::read_file(corpus_copy)==rnet::read_file(corpus_file),"open-rnet document preserves all raw file IDs and bytes");
        SendMessageW(window_,WM_COMMAND,32939,0);check(!document_&&ListView_GetItemCount(list_)==2154,"document close restores parameter catalog");
        self_test_scan_pending_=true;scan();
    }
    LRESULT message(UINT m,WPARAM w,LPARAM l){
        try{
            switch(m){
            case WM_CREATE:created();return 0;
            case WM_SETCURSOR:if(restore_client_cursor(reinterpret_cast<HWND>(w),l))return TRUE;break;
            case WM_SIZE:if(status_)layout();return 0;
            case WM_COMMAND:if(l){if(LOWORD(w)==ID_FILTER&&HIWORD(w)==EN_CHANGE&&list_)refresh();else if(HIWORD(w)==BN_CLICKED&&(LOWORD(w)==ID_EDIT_VALUE||LOWORD(w)==57643))command(LOWORD(w));}else command(LOWORD(w));return 0;
            case WM_NOTIFY:{auto h=reinterpret_cast<NMHDR*>(l);if(h->hwndFrom==list_&&h->code==NM_DBLCLK){edit_selected_value();return 0;}if(h->hwndFrom==tree_&&h->code==TVN_SELCHANGEDW&&parameter_view_){auto n=reinterpret_cast<NMTREEVIEWW*>(l);auto i=std::size_t(n->itemNew.lParam);if(i<group_paths_.size()){selected_group_=group_paths_[i];refresh();}return 0;}if(h->hwndFrom==list_&&h->code==LVN_ITEMCHANGED){int selected=ListView_GetNextItem(list_,-1,LVNI_SELECTED);if(selected>=0&&std::size_t(selected)<displayed_.size()&&displayed_[selected]<rows_.size())SetWindowTextW(details_,rows_[displayed_[selected]].details.c_str());update_edit_controls();}return 0;}
            case SCAN_DONE:scan_done();return 0;
            case SELF_TEST:selftest();return 0;
            case CAN_TEST:read_emulator();return 0;
            case CAN_PROGRESS:set_status(w?L"Emulator bestätigt; Repository-Datei "+std::to_wstring(l)+L" empfangen":L"CAN-Zyklik erkannt; Protokoll wird geprüft …");return 0;
            case CAN_READ_DONE:emulator_read_done();return 0;
            case CAN_MONITOR_START:SendMessageW(window_,WM_COMMAND,ID_CAN_MONITOR,0);return 0;
            case CAN_MONITOR_PROGRESS:set_status(L"can0: "+std::to_wstring(w)+L" echte CAN-Datenframes empfangen — nur Empfang");return 0;
            case CAN_MONITOR_DONE:can_monitor_done();return 0;
            case WM_CLOSE:can_cancelled_=true;if(can_thread_.joinable())can_thread_.join();if(!finish_document_change())return 0;if(scan_thread_.joinable())scan_thread_.join();DestroyWindow(window_);return 0;
            case WM_DESTROY:record("SHUTDOWN clean; commands="+std::to_string(commands_));PostQuitMessage(exit_code_);return 0;
            }
        }catch(const std::exception&e){if(self_test_||can_test_){record(std::string("SELFTEST_FAIL ")+e.what());exit_code_=1;DestroyWindow(window_);}else show_error(e);if(m==WM_CREATE)return -1;return 0;}
        return DefWindowProcW(window_,m,w,l);
    }
    static LRESULT CALLBACK procedure(HWND h,UINT m,WPARAM w,LPARAM l){
        auto self=reinterpret_cast<Application*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(m==WM_NCCREATE){self=static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->window_=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        return self?self->message(m,w,l):DefWindowProcW(h,m,w,l);
    }
    int run(int show){
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_TREEVIEW_CLASSES|ICC_BAR_CLASSES};if(!InitCommonControlsEx(&controls))throw std::runtime_error("Common controls initialization failed");
        WNDCLASSEXW c{};c.cbSize=sizeof(c);c.lpfnWndProc=procedure;c.hInstance=instance_;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);c.lpszClassName=CLASS_NAME;
        if(!RegisterClassExW(&c))throw std::runtime_error("RegisterClass failed");
        auto monitor=preferred_monitor();auto area=monitor.work;
        int width=std::min(1150L,std::max(320L,area.right-area.left-32)),height=std::min(800L,std::max(240L,area.bottom-area.top-32));
        int x=area.left+(area.right-area.left-width)/2,y=area.top+(area.bottom-area.top-height)/2;
        HWND h=CreateWindowExW(0,CLASS_NAME,L"R-net C++ Rekonstruktion — unvollständiger Entwicklungsstand",WS_OVERLAPPEDWINDOW,x,y,width,height,nullptr,nullptr,instance_,this);
        if(!h)throw std::runtime_error("Main window creation failed");
        record("PREFERRED_MONITOR secondary="+std::to_string(monitor.secondary)+" x="+std::to_string(x)+" y="+std::to_string(y)+" width="+std::to_string(width)+" height="+std::to_string(height));
        if(monitor.secondary&&(show==SW_SHOWNORMAL||show==SW_SHOW||show==SW_SHOWDEFAULT))show=SW_SHOWMAXIMIZED;
        ShowWindow(h,show);UpdateWindow(h);MSG msg{};BOOL st;
        while((st=GetMessageW(&msg,nullptr,0,0))>0){if(!accelerators_||!TranslateAcceleratorW(h,accelerators_,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
        return st<0?1:int(msg.wParam);
    }
};
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
    try{
        int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return 1;
        std::filesystem::path installation=L"C:\\Program Files (x86)\\PG Drives Technology\\R-net Programmer OEM Generic";
        std::filesystem::path can_plan,can_target;
        std::filesystem::path monitor_target;
        wchar_t self[32768]{};GetModuleFileNameW(nullptr,self,32768);auto output=std::filesystem::path(self).parent_path()/L"logs";bool test=false;std::filesystem::path document;
        for(int i=1;i<argc;++i){std::wstring arg=argv[i];if(arg==L"--self-test")test=true;else if(arg==L"--installation"&&i+1<argc)installation=argv[++i];else if(arg==L"--output"&&i+1<argc)output=argv[++i];else if(arg==L"--document"&&i+1<argc)document=argv[++i];else if(arg==L"--emulator-read-test"&&i+2<argc){can_plan=argv[++i];can_target=argv[++i];}else if(arg==L"--can-monitor-test"&&i+1<argc)monitor_target=argv[++i];else{LocalFree(argv);throw std::runtime_error("Unknown command-line argument");}}
        LocalFree(argv);if(test&&!can_plan.empty())throw std::runtime_error("Choose one self-test mode");
        if(!monitor_target.empty()&&(test||!can_plan.empty()))throw std::runtime_error("Choose one test mode");
        Application app(instance,installation,output,test,document);if(!can_plan.empty())app.emulator_test(can_plan,can_target);if(!monitor_target.empty())app.monitor_on_start(monitor_target);return app.run(show);
    }catch(const std::exception&e){MessageBoxW(nullptr,wide8(e.what()).c_str(),L"Rekonstruktion: Startfehler",MB_ICONERROR);return 1;}
}

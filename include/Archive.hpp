#pragma once

#include <Util.hpp>
#include <bstream.h>
#include <filesystem>
#include <Compression.hpp>
#include <algorithm>
#include <memory>
#include <vector>
#include <map>

namespace Archive {
    class Rarc;
    class Folder;

    uint16_t Hash(std::string str);
    
    class File : public std::enable_shared_from_this<File>{
        friend Rarc;
        std::shared_ptr<Rarc> mArchive;
        std::shared_ptr<Rarc> mMountedArchive;
        std::shared_ptr<Folder> mParentDir;
        
        std::string mName;
        
        uint8_t* mData;
        uint32_t mSize;

        std::shared_ptr<Rarc> GetMountedArchive(){ return mMountedArchive; }

    public:
        void SetData(unsigned char* data, size_t size){
            mSize = size;

            if(mData != nullptr){
                delete[] mData;
            }
            
            mData = new uint8_t[size];
            memcpy(mData, data, size);
        }

        std::string GetName() { return mName; }
        void SetName(std::string name) { mName = name; }

        uint32_t GetSize() { return mSize; }
        uint8_t* GetData() { return mData; }
        
        
        bool MountAsArchive();

        std::shared_ptr<Rarc> operator->() const {
            return mMountedArchive;
        }

        static std::shared_ptr<File> Create(){
            return std::make_shared<File>();
        }
        
        std::shared_ptr<File> GetPtr(){
            return shared_from_this();
        }

        File(){
            mMountedArchive = nullptr;
            mData = nullptr;
            mSize = 0;
        }
        
        ~File(){
            if(mData != nullptr){
                delete[] mData;
            }
        }
    };

    class Folder : public std::enable_shared_from_this<Folder> {
        std::shared_ptr<Rarc> mArchive;
        std::shared_ptr<Folder> mParentDir;

        std::string mName;
        
        std::vector<std::shared_ptr<Folder>> mFolders;
        std::vector<std::shared_ptr<File>> mFiles;
        
    
    public:
        
        std::string GetName() { return mName; }
        void SetName(std::string name) { mName = name; }
        
        std::shared_ptr<Folder> GetParent() { return mParentDir; }
        void SetParent(std::shared_ptr<Folder> dir) { mParentDir = dir; dir->AddSubdirectory(shared_from_this()); } //fix this later

        void SetParentUnsafe(std::shared_ptr<Folder> dir){ mParentDir = dir; }

        void AddFile(std::shared_ptr<File> file) { mFiles.push_back(file); }
        void DeleteFile(std::shared_ptr<File> file) { if(std::find(mFiles.begin(), mFiles.end(), file) != mFiles.end()) mFiles.erase(std::find(mFiles.begin(), mFiles.end(), file)); }
        
        void AddSubdirectory(std::shared_ptr<Folder> dir);

        std::vector<std::shared_ptr<Folder>>& GetSubdirectories() { return mFolders; }

        std::vector<std::shared_ptr<File>>& GetFiles() { return mFiles; }
        uint16_t GetFileCount() { return (uint16_t)mFiles.size() + (uint16_t)mFolders.size(); }

        std::shared_ptr<Rarc> GetArchive() { return mArchive; }
        void SetArchive(std::shared_ptr<Rarc> arc) { mArchive = arc; }

        std::shared_ptr<Folder> Copy(std::shared_ptr<Rarc> archive);
        
        std::shared_ptr<File> GetFile(std::filesystem::path path);
        std::shared_ptr<Folder> GetFolder(std::filesystem::path path);

        static std::shared_ptr<Folder> Create(std::shared_ptr<Rarc> archive){
            return std::make_shared<Folder>(archive);
        }
        

        std::shared_ptr<Folder> GetPtr(){
            return shared_from_this();
        }

        template<typename T>
        std::shared_ptr<T> Get(std::filesystem::path path){ 
            if constexpr(std::is_same_v<T,File>){
                return GetFile(path);
            } else if constexpr(std::is_same_v<T, Folder>){
                return GetFolder(path);
            } else if constexpr(std::is_same_v<T, Folder>){
                return GetFile(path)->GetMountedArchive();
            }
            return nullptr; 
        }

        Folder(std::shared_ptr<Rarc> archive);

        Folder(){
            mArchive = nullptr;
            mParentDir = nullptr;
        }
        ~Folder(){}
    };

    class Rarc : public std::enable_shared_from_this<Rarc> {
    private:
        friend class Folder;
        std::vector<std::shared_ptr<Folder>> mDirectories;
        
        std::map<std::string, uint32_t> CalculateArchiveSizes();
        
    public:
        bool Load(bStream::CStream* stream);
        void SaveToFile(std::filesystem::path path, Compression::Format compression=Compression::Format::None, uint8_t compressionLevel=7);

        // Directories should all be children of root
        std::shared_ptr<Folder> GetRoot(){ return mDirectories[0]; }
        void SetRoot(std::shared_ptr<Folder> folder) {
            if(mDirectories.size() != 0){
                folder->AddSubdirectory(mDirectories[0]);
            }
            mDirectories.insert(mDirectories.begin(), folder);
        }

        template<typename T>
        std::shared_ptr<T> Get(std::filesystem::path path){ 
            if constexpr(std::is_same_v<T,File>){
                return GetFile(path);
            } else if constexpr(std::is_same_v<T, Folder>){
                return GetFolder(path);
            } else if constexpr(std::is_same_v<T, Folder>){
                return GetFile(path)->GetMountedArchive();
            }
            return nullptr; 
        }

        std::shared_ptr<File> GetFile(std::filesystem::path path) {
            if(path.begin()->string() != "/"){
                return mDirectories[0]->GetFile(path);
            } else {
                std::filesystem::path subPath;
                for(auto it = (++path.begin()); it != path.end(); it++) subPath  = subPath / it->string();
                return mDirectories[0]->GetFile(subPath);
            }
        }

        std::shared_ptr<Folder> GetFolder(std::filesystem::path path){
            if(path.begin()->string() != "/"){
                return mDirectories[0]->GetFolder(path);
            } else {
                std::filesystem::path subPath;
                for(auto it = (++path.begin()); it != path.end(); it++) subPath  = subPath / it->string();

                return mDirectories[0]->GetFolder(subPath);
            }
        }

        static std::shared_ptr<Rarc> Create(){
            return std::make_shared<Rarc>();
        }
        
        std::shared_ptr<Rarc> GetPtr(){
            return shared_from_this();
        }


        Rarc(){}
        ~Rarc(){}
    };
}
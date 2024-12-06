#pragma once

#include <Util.hpp>
#include <bstream.h>
#include <filesystem>
#include <Compression.hpp>
#include <memory>
#include <vector>
#include <map>

namespace Disk {
    struct FSTEntry {
        uint8_t mAttribute;
        uint32_t mNameOffset;
        uint32_t mFirst;
        uint32_t mSecond;
    };

    class Image;
    class Folder;

    class File : public std::enable_shared_from_this<File>{
        friend Image;
        std::shared_ptr<Image> mDisk;
        std::shared_ptr<Folder> mParentDir;
        
        std::string mName;
        
        uint8_t* mData;
        uint32_t mSize;

    public:
        void SetData(unsigned char* data, std::size_t size){
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
        

        static std::shared_ptr<File> Create(){
            return std::make_shared<File>();
        }
        
        std::shared_ptr<File> GetPtr(){
            return shared_from_this();
        }

        File(){
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
        std::shared_ptr<Image> mDisk;
        std::shared_ptr<Folder> mParentDir;

        uint32_t mID { 0 }; // really only used for reading
        uint32_t mParentID { 0 };
        std::string mName;
        
        std::vector<std::shared_ptr<Folder>> mFolders;
        std::vector<std::shared_ptr<File>> mFiles;
    public:
        
        std::string GetName() { return mName; }
        void SetName(std::string name) { mName = name; }
        
        std::shared_ptr<Folder> GetParent() { return mParentDir; }
        void SetParent(std::shared_ptr<Folder> dir) { mParentDir = dir; dir->AddSubdirectory(shared_from_this()); }

        void SetParentUnsafe(std::shared_ptr<Folder> dir){ mParentDir = dir; }

        void AddFile(std::shared_ptr<File> file) { mFiles.push_back(file); }

        void AddSubdirectory(std::shared_ptr<Folder> dir);

        void SetID(uint32_t id) { mID = id; }
        uint32_t GetID() { return mID; }

        void SetParentID(uint32_t id) { mParentID = id; }
        uint32_t GetParentID() { return mParentID; }

        std::vector<std::shared_ptr<Folder>>& GetSubdirectories() { return mFolders; }

        std::vector<std::shared_ptr<File>>& GetFiles() { return mFiles; }
        uint16_t GetFileCount() { return (uint16_t)mFiles.size() + (uint16_t)mFolders.size(); }

        std::shared_ptr<Image> GetDisk() { return mDisk; }

        std::shared_ptr<Folder> Copy(std::shared_ptr<Image> disk);
        std::shared_ptr<File> GetFile(std::filesystem::path path);
        std::shared_ptr<Folder> GetFolder(std::filesystem::path path);

        static std::shared_ptr<Folder> Create(std::shared_ptr<Image> disk){
            return std::make_shared<Folder>(disk);
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
            }
            return nullptr; 
        }

        Folder(std::shared_ptr<Image> disk);

        Folder(){
            mDisk = nullptr;
            mParentDir = nullptr;
        }
        ~Folder(){}
    };

    struct Banner {
        char mBNRImg[0x1800];
        char mGameName[0x20];
        char mDeveloper[0x20];
        char mFullTitle[0x40];
        char mFullDeveloper[0x40];
        char mGameDescription[0x80];
    };

    class Image : public std::enable_shared_from_this<Image> {
    private:
        friend class Folder;
        std::shared_ptr<Folder> mRoot;

        // Some Disk Info
        uint32_t mGameCode { 0 };
        uint16_t mMakerCode { 0 };
        uint8_t mDiskId { 0 };
        uint8_t mVersion { 0 };
        uint8_t mAudioStreaming { 0 };
        uint8_t mStreamBuffSize { 0 };

        uint32_t mDebugMonitorLoadAddr { 0 };

        char mGameName[0x3E0] { 0 };
        void LoadDir(bStream::CStream* stream, std::shared_ptr<Folder> folder, std::vector<std::shared_ptr<Folder>>& folders, std::size_t& startIdx, uint32_t count, uint32_t stringTableOffset);
        std::size_t CalculateFstSize(std::shared_ptr<Folder> folder, std::size_t& stringTableSize);
    public:
        bool Load(bStream::CStream* stream);
        void SaveToFile(std::filesystem::path path);

        // Directories should all be children of root
        std::shared_ptr<Folder> GetRoot(){ return mRoot; }

        template<typename T>
        std::shared_ptr<T> Get(std::filesystem::path path){ 
            if constexpr(std::is_same_v<T,File>){
                return GetFile(path);
            } else if constexpr(std::is_same_v<T, Folder>){
                return GetFolder(path);
            }
            return nullptr; 
        }

        std::shared_ptr<File> GetFile(std::filesystem::path path) {
            if(path.begin()->string() != "/"){
                return mRoot->GetFile(path);
            } else {
                std::filesystem::path subPath;
                for(auto it = (++path.begin()); it != path.end(); it++) subPath  = subPath / it->string();
                return mRoot->GetFile(subPath);
            }
        }

        std::shared_ptr<Folder> GetFolder(std::filesystem::path path){
            if(path.begin()->string() != "/"){
                return mRoot->GetFolder(path);
            } else {
                std::filesystem::path subPath;
                for(auto it = (++path.begin()); it != path.end(); it++) subPath  = subPath / it->string();

                return mRoot->GetFolder(subPath);
            }
        }

        static std::shared_ptr<Image> Create(){
            return std::make_shared<Image>();
        }
        
        std::shared_ptr<Image> GetPtr(){
            return shared_from_this();
        }


        Image(){}
        ~Image(){}
    };
}
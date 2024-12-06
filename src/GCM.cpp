#include "GCM.hpp"
#include "Util.hpp"
#include <stack>
#include <algorithm>
#include <map>

namespace Disk {

///
/// Folder
///

Folder::Folder(std::shared_ptr<Image> disk){
    mDisk = disk;
    mParentDir = nullptr;
}

std::shared_ptr<File> Folder::GetFile(std::filesystem::path path) {
    if(path.begin() == path.end()) return nullptr;


    for(auto file : mFiles){
        if(file->GetName() == path.begin()->string()){
            if(((++path.begin()) == path.end())){
                return file;
            } /* else { // perhaps use this to mount rarc archives directly from isos?
                if(file->MountAsArchive()){
                    std::filesystem::path subPath;
                    for(auto it = (++path.begin()); it != path.end(); it++) subPath  = subPath / it->string();
                    return (*file)->GetFile(subPath);
                }
            }*/
        }
    }

    std::shared_ptr<File> file = nullptr;

    for(auto dir : mFolders){
        if(dir->GetName() == path.begin()->string()){
            std::filesystem::path subPath;
            for(auto it = (++path.begin()); it != path.end(); it++) subPath  = subPath / it->string();
            file = dir->GetFile(subPath);
        }
    }

    return file;
}

std::shared_ptr<Folder> Folder::GetFolder(std::filesystem::path path){
    for(auto dir : mFolders){
        if(dir->GetName() == path.begin()->string() && ((++path.begin()) == path.end())){
            return dir;
        } else if(dir->GetName() == path.begin()->string()) {
            std::filesystem::path subPath;
            for(auto it = (++path.begin()); it != path.end(); it++) subPath  = subPath / it->string();
            return dir->GetFolder(subPath);
        }
    }

    return nullptr;
}

std::shared_ptr<Folder> Folder::Copy(std::shared_ptr<Image> disk){
    std::shared_ptr<Folder> newFolder = Folder::Create(disk);
    newFolder->SetName(mName);
    for(auto dir : mFolders){
        newFolder->AddSubdirectory(dir);
    }

    for(auto file : mFiles){
        newFolder->AddFile(file);
    }
    return newFolder;
}

void Folder::AddSubdirectory(std::shared_ptr<Folder> dir){
    if(dir->GetDisk() != mDisk){
        std::shared_ptr<Folder> copy = dir->Copy(mDisk);
        AddSubdirectory(copy);
    } else {
        dir->SetParentUnsafe(GetPtr());
        mFolders.push_back(dir);
    }
}


///
/// Disk
///


std::size_t Image::CalculateFstSize(std::shared_ptr<Folder> folder, std::size_t& stringTableSize){    
    std::size_t size = 0;
    for(auto f : folder->GetFiles()){
        size += 0xC;
        stringTableSize += f->GetName().size();
    }
    for(auto f : folder->GetSubdirectories()){
        size += 0xC;
        stringTableSize += f->GetName().size();
        size += CalculateFstSize(f, stringTableSize);
    }

    return size;
}

void FstWriteFolder(bStream::CStream& imgStream, bStream::CMemoryStream& stream, bStream::CMemoryStream& stringTable, std::shared_ptr<Folder> folder, std::size_t parentIdx, std::size_t& idx){
    std::size_t parentIdx = idx;
    //if not root..
    
    stream.writeUInt32(0x01 | (stringTable.tell() & 0x00FFFFFFFF));
    for(char c : folder->GetName()){
        stringTable.writeUInt8(c);
    }
    stringTable.writeUInt8(0);

    stream.writeUInt32(idx);
    std::size_t nextOffs = stream.tell();
    stream.writeUInt32(0);

    idx++;

    // otherwise only write informationg

    for(auto f : folder->GetSubdirectories()){
        FstWriteFolder(imgStream, stream, stringTable, f, parentIdx, idx);
    }

    for(auto file : folder->GetFiles()){
        std::size_t nameOffset = stringTable.tell();
        for(char c : file->GetName()){
            stringTable.writeUInt8(c);
        }
        stringTable.writeUInt8(0);

        stream.writeUInt32(0x00 | (nameOffset & 0x00FFFFFFFF));
        stream.writeUInt32(imgStream.tell());
        stream.writeUInt32(file->GetSize());
        idx++;
    }

    std::size_t position = stream.tell();
    stream.seek(nextOffs);
    stream.writeUInt32(idx);
    stream.seek(position);
}

void Image::SaveToFile(std::filesystem::path path){
    std::size_t stringTableSize = 0;
    std::size_t fstSize = CalculateFstSize(mRoot->GetFolder("files"), stringTableSize);
    std::size_t fileDataOffset = Util::PadTo32(0x2440 + mRoot->GetFile("sys/apploader.img")->GetSize() + fstSize + stringTableSize);

    bStream::CMemoryStream fst(fstSize, bStream::Endianess::Big, bStream::OpenMode::Out);
    bStream::CMemoryStream stringTbale(stringTableSize, bStream::Endianess::Big, bStream::OpenMode::Out);

    bStream::CFileStream imgFile(path, bStream::Endianess::Big, bStream::OpenMode::Out);

}

void Image::LoadDir(bStream::CStream* stream, std::shared_ptr<Folder> folder, std::vector<std::shared_ptr<Folder>>& folders, std::size_t& idx, uint32_t count, uint32_t stringTableOffset){
    while(idx < count){
        uint32_t attr = stream->readUInt32();
        bool isDir = attr & 0xFF000000;
        uint32_t nameOffset = attr & 0x00FFFFFF;
        uint32_t first = stream->readUInt32();
        uint32_t second = stream->readUInt32();
        
        uint32_t pos = stream->tell();
        stream->seek(stringTableOffset + nameOffset);
        
        std::string entryName = "";
        char c = stream->readUInt8();
        while(c != '\0'){
            entryName.push_back(c);
            c = stream->readUInt8();
        }
        
        stream->seek(pos);

        if(isDir) {
            std::shared_ptr<Folder> newFolder = Folder::Create(GetPtr());
            newFolder->SetName(entryName);
            newFolder->SetID(idx);
            newFolder->SetParentID(first);
            folders.push_back(newFolder);
            idx++;
            LoadDir(stream, newFolder, folders, idx, second, stringTableOffset);
        } else {
            std::shared_ptr<File> file = File::Create();
            file->SetName(entryName);

            // Read File Data
            uint32_t pos = stream->tell();

            stream->seek(first);
            
            uint8_t* data = new uint8_t[second];
            stream->readBytesTo(data, second);
            file->SetData(data, second);
            
            delete[] data;
            
            stream->seek(pos);

            folder->AddFile(file);
            idx++;
        }
    }
}

// Needs error checking
bool Image::Load(bStream::CStream* stream){
    mRoot = Folder::Create(GetPtr());
    mRoot->SetName("root");
    std::shared_ptr<Folder> sys = Folder::Create(GetPtr());
    std::shared_ptr<Folder> files = Folder::Create(GetPtr());

    sys->SetName("sys");
    files->SetName("files");

    stream->seek(0x0424);
    uint32_t fstOffset = stream->readUInt32();
    uint32_t fstSize = stream->readUInt32();

    stream->seek(fstOffset);

    // FST Root
    uint32_t attr = stream->readUInt32();
    uint8_t isDir = attr & 0xFF;
    uint32_t nameOffset = attr & 0x00FFFFFF;
    uint32_t parentOffset = stream->readUInt32();
    uint32_t numEntries = stream->readUInt32();

    // Entries
    std::vector<FSTEntry> entries(numEntries+1);
    entries[0] = {
        .mAttribute = isDir,
        .mNameOffset = nameOffset,
        .mFirst = parentOffset,
        .mSecond = numEntries
    };

    std::size_t idx = 1;
    std::vector<std::shared_ptr<Folder>> folders = { files };
    LoadDir(stream, files, folders, idx, numEntries, fstOffset+(numEntries*0xC));

    // I'm not the biggest fan of this, but its far easier than doing stuff on the fly
    for(auto folder : folders){
        for(auto parent : folders){
            if(folder->GetID() != 0 && parent->GetID() == folder->GetParentID()){
                parent->AddSubdirectory(folder);
                break;
            }
        }
    }


    stream->seek(0x2440 + 0x14);
    uint32_t apploaderSize = stream->readUInt32();
    uint32_t apploaderTrailerSize = stream->readUInt32();

    stream->seek(0x2440);

    // read apploader
    uint8_t* apploader = new uint8_t[apploaderSize + apploaderTrailerSize + 0x20]{0};
    stream->readBytesTo(apploader, apploaderSize + apploaderTrailerSize + 0x20);

    std::shared_ptr<File> apploaderFile = File::Create();
    apploaderFile->SetName("apploader.img");
    apploaderFile->SetData(apploader, apploaderSize + apploaderTrailerSize + 0x20);
    sys->AddFile(apploaderFile);
    delete[] apploader;

    stream->seek(0x440);

    // read bi2
    uint8_t* bi2 = new uint8_t[0x2000]{0};
    stream->readBytesTo(bi2, 0x2000);

    std::shared_ptr<File> diskHeaderInfo = File::Create();
    diskHeaderInfo->SetName("bi2.bin");
    diskHeaderInfo->SetData(bi2, 0x2000);
    sys->AddFile(diskHeaderInfo);
    delete[] bi2;

    stream->seek(0);
    uint8_t* boot = new uint8_t[0x440]{0};
    stream->readBytesTo(boot, 0x440);

    std::shared_ptr<File> diskHeader = File::Create();
    diskHeader->SetName("boot.bin");
    diskHeader->SetData(boot, 0x440);
    sys->AddFile(diskHeader);
    delete[] boot;

    stream->seek(fstOffset);
    uint8_t* fstData = new uint8_t[fstSize]{0};
    stream->readBytesTo(fstData, fstSize);

    std::shared_ptr<File> fstFile = File::Create();
    fstFile->SetName("fst.bin");
    fstFile->SetData(fstData, fstSize);
    sys->AddFile(fstFile);
    delete[] fstData;

    stream->seek(0x420);
    uint32_t dolOffset = stream->readUInt32();
    
    uint32_t dolSize = 0x100;
    stream->seek(dolOffset + 0x90);
    for(uint32_t dfp = 0; dfp < 18; dfp++){
        dolSize += stream->readUInt32();
    }

    stream->seek(dolOffset);
    uint8_t* dolData = new uint8_t[dolSize]{0};
    stream->readBytesTo(dolData, dolSize);

    std::shared_ptr<File> dolFile = File::Create();
    dolFile->SetName("main.dol");
    dolFile->SetData(dolData, dolSize);
    sys->AddFile(dolFile);
    delete[] dolData;

    mRoot->AddSubdirectory(sys);
    mRoot->AddSubdirectory(files);

    return true;
}

}